"""设备模型基类。

依赖 comm_lib.BaseComm 做收发，自身不感知底层是 BLE 还是 USB。

业务方法采用"请求-应答"模型：
    send_and_wait(cmd, payload, timeout) -> (err_code, data)
构造私有帧发送后，挂起等待对应 sequence 的 ACK 帧（设备端 ACK 会回原 sequence）。
ACK 帧的 cmd 固定为 Command.ACK（0x81），用 sequence 关联请求与应答。
对齐 src_archive/windows/libProtocolFramework/protocol_logic_manager.c
的 __push_command_map / __ack 机制。
"""

from __future__ import annotations

import logging
import threading
from typing import Callable, Dict, Optional

from comm_lib.channels import BaseComm, CommError
from comm_lib.protocol.cmd import Command, ErrCode, EventType
from comm_lib.protocol.frame import (
    ProtocolFrame,
    SequenceCounter,
    bytes_to_u32,
    decode_ack_payload,
    decode_event_payload,
    u32_to_bytes,
)
from comm_lib.protocol.prop_key import PropKey
from exceptions import DeviceToolError

logger = logging.getLogger(__name__)


class DeviceError(DeviceToolError):
    """设备业务错误。"""


class _PendingRequest:
    """一次等待 ACK 的请求。"""

    def __init__(self, sequence: int, cmd: int) -> None:
        self.sequence = sequence
        self.cmd = cmd
        self.event = threading.Event()
        self.err_code: Optional[int] = None
        self.data: bytes = b""

    def complete(self, err_code: int, data: bytes) -> None:
        self.err_code = err_code
        self.data = data
        self.event.set()


class BaseDevice:
    """设备模型基类。全设备通用能力在此。"""

    # 设备类型，子类覆盖
    DEVICE_TYPE: Optional[int] = None

    def __init__(self, comm: BaseComm, address: str = "", name: str = "",
                 transport: str = "", connection_type: str = "") -> None:
        self._comm = comm
        self.address = address
        self.name = name
        self.transport = transport
        self.connection_type = connection_type  # "ble" / "usb_direct" / "usb_dongle"
        self._seq = SequenceCounter()
        self._pending: Dict[int, _PendingRequest] = {}
        self._lock = threading.Lock()
        # 设备信息缓存
        self.product_no: Optional[bytes] = None
        self.device_mac: Optional[bytes] = None
        self.device_version: Optional[bytes] = None
        # 事件回调
        self.on_event: Optional[Callable[[int, int], None]] = None  # (event_type, value)
        self.on_audio: Optional[Callable[[int, int, bytes], None]] = None  # (type, channel, data)
        # EVENT 等待表：event_type -> threading.Event + 结果
        # 用于 BATTERY 等通过 EVENT 应答（而非 ACK）的属性
        self._event_waiters: Dict[int, list] = {}
        # 鉴权状态（authenticate() 过程中填充）
        self._auth_device_finger: Optional[bytes] = None
        self._auth_client_finger: Optional[bytes] = None
        self._auth_client_random: Optional[bytes] = None
        self._auth_session_key: Optional[bytes] = None
        self._auth_phase1_payload: Optional[bytes] = None  # 收到 phase1 challenge 后暂存
        self._auth_phase1_event = threading.Event()
        self._auth_phase3_payload: Optional[bytes] = None
        self._auth_phase3_event = threading.Event()
        self._auth_success: Optional[bool] = None
        # 注册帧处理
        self._comm.on_frame = self._handle_frame

    # ---- 基础属性 ----
    @property
    def comm(self) -> BaseComm:
        return self._comm

    @property
    def is_connected(self) -> bool:
        return self._comm.is_connected

    # ---- 帧分发 ----
    def _handle_frame(self, frame: ProtocolFrame) -> None:
        """处理从通信层上抛的帧。"""
        if frame.cmd == Command.ACK:
            self._handle_ack(frame.sequence, frame.payload)
        elif frame.cmd == Command.EVENT:
            type_, value = decode_event_payload(frame.payload)
            self._dispatch_event(type_, value)
        elif frame.cmd == Command.AUTHENTICATE:
            self._handle_authenticate_frame(frame.payload)
        elif frame.cmd == Command.ASYNC_DATA:
            # 录音音频数据：payload=[type][channel][data...]
            p = frame.payload
            if len(p) >= 2 and self.on_audio:
                self.on_audio(p[0], p[1], bytes(p[2:]))

    def _handle_ack(self, sequence: int, payload: bytes) -> None:
        err_code, data = decode_ack_payload(payload)
        with self._lock:
            req = self._pending.pop(sequence, None)
        if req:
            req.complete(err_code, data)

    def _dispatch_event(self, type_: int, value: int) -> None:
        # 先唤醒等待该 event_type 的请求（如 BATTERY 走 EVENT 应答）
        with self._lock:
            waiter = self._event_waiters.pop(type_, None)
        if waiter is not None:
            waiter[1] = value
            waiter[0].set()
        if self.on_event:
            self.on_event(type_, value)

    def _wait_event(self, event_type: int, timeout: float = 3.0) -> int:
        """阻塞等待指定 event_type 的下一个 EVENT 帧，返回 value。

        用于 BATTERY 等通过 EVENT 上报的属性（见 AM50 logic_protocol_manager.c::
        _prop_resp 的 BATTERY 分支用 event_pack 而非 get_prop_resp）。
        """
        ev = threading.Event()
        waiter = [ev, None]
        with self._lock:
            self._event_waiters[event_type] = waiter
        if not ev.wait(timeout=timeout):
            with self._lock:
                self._event_waiters.pop(event_type, None)
            raise DeviceError(f"等待 EVENT(type={event_type:#x}) 超时")
        return waiter[1] if waiter[1] is not None else 0

    # ---- 请求-应答 ----
    def send_and_wait(self, cmd: int, payload: bytes, timeout: float = 5.0) -> tuple[int, bytes]:
        """发送指令并等待对应 sequence 的 ACK。

        返回 (err_code, data)。err_code != SUCCESS 视为业务失败但不抛异常，
        交由调用方判断；通信失败或超时抛 DeviceError。
        """
        if not self.is_connected:
            raise DeviceError("设备未连接")
        sequence = self._seq.next()
        req = _PendingRequest(sequence, cmd)
        with self._lock:
            self._pending[sequence] = req
        try:
            self._send_frame(cmd, payload, sequence)
        except CommError as e:
            with self._lock:
                self._pending.pop(sequence, None)
            raise DeviceError(f"发送失败: {e}") from e
        if not req.event.wait(timeout=timeout):
            with self._lock:
                self._pending.pop(sequence, None)
            raise DeviceError(f"等待 ACK 超时 (cmd={cmd:#04x}, seq={sequence})")
        return req.err_code if req.err_code is not None else ErrCode.UNKNOW, req.data

    def _send_frame(self, cmd: int, payload: bytes, sequence: int) -> None:
        """实际发送。dongle 多子设备场景由子类/上层重写以指定 mac。"""
        self._comm.send_raw(self._build_frame(cmd, payload, sequence))

    def _build_frame(self, cmd: int, payload: bytes, sequence: int) -> bytes:
        from comm_lib.protocol.frame import build_frame
        return build_frame(cmd, payload, sequence)

    def _get_slave_prop(self, prop_key: int, timeout: float = 5.0) -> tuple[int, bytes]:
        """GET_SLAVE_PROP 通用封装。对齐 protocol_logic_manager.c::_get_* 的 send_cmd(GET_*)。"""
        return self.send_and_wait(Command.GET_SLAVE_PROP, bytes([prop_key]), timeout)

    def _set_slave_prop(self, prop_key: int, value_bytes: bytes, timeout: float = 5.0) -> tuple[int, bytes]:
        """SET_SLAVE_PROP 通用封装。对齐 protocol_logic_manager.c::_set_dpi 等。"""
        return self.send_and_wait(Command.SET_SLAVE_PROP, bytes([prop_key]) + value_bytes, timeout)

    # ---- 基础信息读写（验收 3.3 首条） ----
    def get_product_no(self, timeout: float = 5.0) -> bytes:
        """获取设备产品系列编码（4 字节）。

        对齐 protocol_logic_manager.c::_get_productNo。
        """
        err, data = self._get_slave_prop(PropKey.PRODUCT_NO, timeout=timeout)
        if err != ErrCode.SUCCESS or len(data) < 4:
            raise DeviceError(f"获取 product_no 失败 err={err}")
        self.product_no = bytes(data[:4])
        return self.product_no

    def get_device_mac(self, timeout: float = 5.0) -> bytes:
        """获取设备 MAC 地址（6 字节）。

        对齐 protocol_logic_manager.c::_get_mac_address(MAC_MOURSE_DATA)。
        """
        err, data = self._get_slave_prop(PropKey.MAC_MOURSE, timeout=timeout)
        if err != ErrCode.SUCCESS or len(data) < 6:
            raise DeviceError(f"获取 device_mac 失败 err={err}")
        self.device_mac = bytes(data[:6])
        return self.device_mac

    def get_dongle_mac(self, timeout: float = 5.0) -> bytes:
        """获取接收器（dongle/WiFi 模块）MAC 地址（6 字节）。

        快传 WiFi 热点由接收器开启，SSID/密码生成用的是 dongle 的 MAC。
        """
        err, data = self._get_slave_prop(PropKey.MAC_DONGLE, timeout=timeout)
        if err != ErrCode.SUCCESS or len(data) < 6:
            raise DeviceError(f"获取 dongle_mac 失败 err={err}")
        self.dongle_mac = bytes(data[:6])
        return self.dongle_mac

    def get_device_version(self, timeout: float = 5.0) -> int:
        """获取设备版本号（u32）。

        对齐 protocol_logic_manager.c::_get_version(VERSION_MOURSE_DATA)。
        """
        err, data = self._get_slave_prop(PropKey.VERSION_MOURSE, timeout=timeout)
        if err != ErrCode.SUCCESS or len(data) < 4:
            raise DeviceError(f"获取 device_version 失败 err={err}")
        ver = bytes_to_u32(data[:4])
        self.device_version = data[:4]
        return ver

    def get_sleep_time(self) -> int:
        """获取设备休眠时间（秒）。

        cmd=GET_SLAVE_PROP, payload=[SLEEP_TIME(0x10)]
        应答 4 字节 LE 秒数。
        """
        err, data = self._get_slave_prop(PropKey.SLEEP_TIME)
        if err != ErrCode.SUCCESS or len(data) < 4:
            raise DeviceError(f"获取休眠时间失败 err={err}")
        return bytes_to_u32(data[:4])

    def set_sleep_time(self, seconds: int) -> int:
        """设置设备休眠时间（秒）。

        cmd=SET_SLAVE_PROP, payload=[SLEEP_TIME(0x10)][seconds(4 LE)]
        """
        err, _ = self._set_slave_prop(PropKey.SLEEP_TIME, u32_to_bytes(seconds))
        if err != ErrCode.SUCCESS:
            raise DeviceError(f"设置休眠时间失败 err={err}")
        return err

    def get_battery(self) -> int:
        """获取电池电量（单位 mV，见 AM50 logic_protocol_manager.c::_prop_resp 的 BATTERY 分支）。

        注意：设备端对 BATTERY 属性用 event_pack(TYPE_BATTERY, batt_vol_mv_real) 应答，
        而非标准 ACK。所以本方法发 GET_SLAVE_PROP 后等待 EVENT(BATTERY) 帧取值，
        而不是像 get_product_no 那样等 ACK。
        """
        sequence = self._seq.next()
        req = _PendingRequest(sequence, Command.GET_SLAVE_PROP)
        # 同时挂 ACK 等待（兼容部分固件用 ACK 回 battery）和 EVENT 等待
        from comm_lib.protocol.frame import build_frame
        frame = build_frame(Command.GET_SLAVE_PROP, bytes([PropKey.BATTERY]), sequence)
        with self._lock:
            self._pending[sequence] = req
        ev = threading.Event()
        waiter = [ev, None]
        with self._lock:
            self._event_waiters[EventType.BATTERY] = waiter
        try:
            self._comm.send_raw(frame)
        except CommError as e:
            with self._lock:
                self._pending.pop(sequence, None)
                self._event_waiters.pop(EventType.BATTERY, None)
            raise DeviceError(f"发送失败: {e}") from e
        # 等任意一个到达
        ack_done = req.event.wait(timeout=5.0)
        if ack_done:
            with self._lock:
                self._event_waiters.pop(EventType.BATTERY, None)
            if req.err_code == ErrCode.SUCCESS and len(req.data) >= 4:
                return bytes_to_u32(req.data[:4])
            # ACK 失败，再尝试 EVENT 结果
        if waiter[0].is_set():
            return waiter[1]
        if not ev.wait(timeout=0.1):
            with self._lock:
                self._pending.pop(sequence, None)
                self._event_waiters.pop(EventType.BATTERY, None)
            raise DeviceError("获取 battery 超时")
        return waiter[1] if waiter[1] is not None else 0

    def get_protocol_version(self) -> int:
        """获取设备协议版本号。"""
        err, data = self._get_slave_prop(PropKey.PROTOCOL_VERSION)
        if err != ErrCode.SUCCESS or len(data) < 4:
            raise DeviceError(f"获取 protocol_version 失败 err={err}")
        return bytes_to_u32(data[:4])

    def get_slave_device_version(self) -> list:
        """获取从机版本号（AM60 鼠标 / AT80 键盘等带从机的设备）。

        cmd=GET_SLAVE_PROP, payload=[SLAVE_DEVICE_VERSION(0x1F)]

        应答有两种形态（真机实测）：
        - 6 字节整条：2 字节从机设备号 + 4 字节版本号，可以有多条
          （如 AM60：设备号 2、版本 1.0.0.13）
        - 短应答（如 AT80 只回 2 字节 0x0157）：没有设备号，整段就是版本号

        返回 [(device_id, version_bytes), ...]。version_bytes 按应答里的字节序原样返回，
        调用方用 ".".join(str(b) for b in version_bytes) 拼成 x.y.z.w。
        """
        err, data = self._get_slave_prop(PropKey.SLAVE_DEVICE_VERSION)
        if err != ErrCode.SUCCESS:
            raise DeviceError(f"获取从机版本号失败 err={err} data={data.hex()}")
        if not data:
            return []
        if len(data) % 6 == 0:
            result = []
            for i in range(0, len(data), 6):
                dev_id = int.from_bytes(data[i:i + 2], "little")
                result.append((dev_id, bytes(data[i + 2:i + 6])))
            return result
        # 短应答：整段作为版本号（无设备号，用 0 占位）
        return [(0, bytes(data))]

    def handshake(self) -> int:
        """握手。

        对齐 protocol_logic_manager.c::_handshake。
        返回 ACK 的 err_code（设备可能置位 0x10000 表示已鉴权，见 AM50 logic_protocol_manager.c）。
        """
        err, _ = self.send_and_wait(Command.HANDSHAKE, b"")
        return err

    # ---- 鉴权握手（完整实现） ----
    def authenticate(self, timeout: float = 15.0) -> bool:
        """执行完整鉴权握手。

        对齐 lib_device_manager.c::_start_authentication 的流程：
          1. set_os_type (Windows)
          2. set_client_finger (上位机指纹)
          3. get_device_finger (设备指纹)
          4. get_device_mac
          5. get_device_version
          6. handshake -> 设备若未鉴权则主动发 AUTHENTICATE(phase1)
          7. 处理 phase1 -> 构造 phase2 应答发回设备
          8. 设备验证后发 phase3 -> 上位机验证 -> 返回成功/失败

        若设备 flash 中鉴权标志已置位（handshake ACK 含 0x10000），
        跳过 phase1-3 流程，直接返回成功。

        返回 True 表示鉴权成功。
        """
        from .auth import (
            AUTH_AUTHENTICATED_FLAG,
            build_phase2_response,
            derive_session_key,
            get_client_finger,
            verify_phase3,
        )
        import secrets as _secrets

        if not self.is_connected:
            raise DeviceError("设备未连接")

        logger.debug("auth start: address=%s name=%s", self.address, self.name)

        # 重置鉴权状态
        self._auth_phase1_event.clear()
        self._auth_phase3_event.clear()
        self._auth_phase1_payload = None
        self._auth_phase3_payload = None
        self._auth_success = None

        # step1 set_os_type=Windows（对齐 _set_os_type OS_TYPE_WINDOWS=1）
        # 注：AM50/T8 设备端 _set_prop 的 OS_TYPE 分支注释掉了 _action_resp，不回 ACK，
        # 用 fire-and-forget 方式发送（send_and_wait 会超时）。
        seq = self._seq.next()
        self._send_frame(
            Command.SET_SLAVE_PROP,
            bytes([PropKey.OS_TYPE]) + u32_to_bytes(1),
            seq,
        )

        # step2 set_client_finger
        client_finger = get_client_finger()
        self._auth_client_finger = client_finger
        self.send_and_wait(
            Command.SET_SLAVE_PROP,
            bytes([PropKey.CLIENT_FINGER]) + client_finger,
            timeout=3.0,
        )

        # step3 get_device_finger
        err, data = self._get_slave_prop(PropKey.FINGER, timeout=3.0)
        if err != ErrCode.SUCCESS or len(data) < 16:
            raise DeviceError(f"获取 device_finger 失败 err={err}")
        device_finger = bytes(data[:16])
        self._auth_device_finger = device_finger

        # step4 get_device_mac
        device_mac = self.get_device_mac()
        # step5 get_device_version
        err, data = self._get_slave_prop(PropKey.VERSION_MOURSE, timeout=3.0)
        if err != ErrCode.SUCCESS or len(data) < 4:
            raise DeviceError(f"获取 device_version 失败 err={err}")
        device_version = bytes(data[:4])
        self.device_version = device_version

        # 派生 session_key + 生成 client_random
        self._auth_session_key = derive_session_key(device_mac, device_version)
        self._auth_client_random = _secrets.token_bytes(16)

        # step6 handshake
        err, _ = self.send_and_wait(Command.HANDSHAKE, b"", timeout=3.0)
        if err & AUTH_AUTHENTICATED_FLAG:
            # 设备已鉴权（flash 标志），跳过 phase 流程
            logger.debug("auth: device already authenticated (flash flag), skip phases")
            self._auth_success = True
            return True

        logger.debug("auth: waiting for device phase1 challenge")
        if not self._auth_phase1_event.wait(timeout=timeout):
            raise DeviceError("等待设备 phase1 challenge 超时")
        challenge1 = self._auth_phase1_payload
        if challenge1 is None or len(challenge1) < 17:
            raise DeviceError("phase1 challenge 格式错误")

        # 构造 phase2 应答并发出（AUTHENTICATE 帧，不需要等 ACK）
        phase2_payload = build_phase2_response(
            challenge1[1:17],  # 去掉 phase1 标志字节
            self._auth_session_key,
            device_finger,
            client_finger,
            self._auth_client_random,
        )
        seq = self._seq.next()
        self._send_frame(Command.AUTHENTICATE, phase2_payload, seq)
        logger.debug("auth: phase2 sent, waiting for phase3")

        # step8 等待设备发 phase3
        if not self._auth_phase3_event.wait(timeout=timeout):
            raise DeviceError("等待设备 phase3 应答超时")
        phase3_data = self._auth_phase3_payload
        if phase3_data is None or len(phase3_data) < 17:
            raise DeviceError("phase3 应答格式错误")

        ok = verify_phase3(
            phase3_data[1:17],  # 去掉 phase3 标志字节
            client_finger,
            self._auth_client_random,
        )
        self._auth_success = ok
        logger.debug("auth done: success=%s", ok)
        return ok

    def _handle_authenticate_frame(self, payload: bytes) -> None:
        """处理设备主动上报的 AUTHENTICATE 帧。

        payload[0] = phase (PHASE1/PHASE3)，后续是 phase 对应数据。
        对齐设备端 logic_protocol_manager.c::_authenticate_action。
        """
        if not payload:
            return
        phase = payload[0]
        if phase == 0x1:  # PHASE1: 设备发来 challenge
            self._auth_phase1_payload = bytes(payload)
            self._auth_phase1_event.set()
        elif phase == 0x3:  # PHASE3: 设备验证 phase2 后的应答
            self._auth_phase3_payload = bytes(payload)
            self._auth_phase3_event.set()

    # ---- 录音（验收 1.2 第4条，本轮留接口占位） ----
    def start_record(self) -> int:
        """开始录音。

        对齐 protocol_logic_manager.c::_start_record: cmd=START_RECORD, payload=[0]。
        音频格式由设备决定，编码信息体现在 ASYNC_DATA 帧 payload 的 type/channel 字段。
        本轮仅发送指令；音频数据通过 on_audio 回调上抛（见 _handle_frame）。
        """
        err, _ = self.send_and_wait(Command.START_RECORD, bytes([0]))
        return err

    def stop_record(self) -> int:
        """停止录音。对齐 protocol_logic_manager.c::_stop_record: cmd=STOP_RECORD, payload 空。"""
        err, _ = self.send_and_wait(Command.STOP_RECORD, b"")
        return err

    # ---- OTA 升级（完整实现） ----
    def perform_ota(self, plaintext_firmware: bytes,
                    on_progress: Optional[Callable[[int, int], None]] = None,
                    on_check_cancel: Optional[Callable[[], bool]] = None,
                    timeout: float = 8.0) -> bool:
        """执行完整 OTA 升级流程。

        前置条件：已完成 authenticate()（需要 session_key）。
        流程：
          1. start_ota_session(device, session_key) -> ota_session（传输密钥）
          2. transfer_firmware(device, 明文, session_key, ota_session) -> 32B 分片传输 + 变换
          3. end_ota(device, expected_crc32) -> 校验累计 CRC + 启用新固件

        注意：传入的是原始明文固件。C 库内部会先做 firmware 加密校验
        （_check_the_firmware），校验通过后解密回明文再传输。Python 重写时
        直接传明文，传输层负责 XOR+AES 变换。

        on_check_cancel() 回调返回 True 时中止传输并通知设备取消（end_ota enable=0）。

        返回 True 表示升级成功。
        注意：OTA 有变砖风险，确认固件正确后再调用。
        """
        from .ota import start_ota_session, transfer_firmware, end_ota, compute_firmware_crc32, OtaError

        if self._auth_session_key is None:
            raise DeviceError("OTA 需要先完成 authenticate() 获取 session_key")
        if not self.product_no:
            self.get_product_no()
        if self.product_no is None:
            raise DeviceError("无法获取 product_no")

        logger.debug("ota start: firmware_size=%d", len(plaintext_firmware))

        # CRC 基于明文固件（设备端对还原的明文 chunk 累计 CRC）
        expected_crc = compute_firmware_crc32(plaintext_firmware)

        # 1. 建立 OTA session（返回 ota_session 供传输变换使用）
        ota_session = start_ota_session(self, self._auth_session_key, timeout=timeout)
        logger.debug("ota: session established")

        # 2. 分片传输明文（每个 chunk 经 XOR(session_key)+AES(ota_session) 变换）
        try:
            transfer_firmware(self, plaintext_firmware, self._auth_session_key, ota_session,
                              on_progress=on_progress, on_check_cancel=on_check_cancel,
                              timeout=timeout)
        except OtaError as e:
            # 用户取消或传输出错：通知设备终止 OTA（enable=0，不启用新固件）
            logger.debug("ota aborted: %s, sending end_ota(0)", e)
            try:
                end_ota(self, 0, enable=0, timeout=timeout)
            except Exception:
                pass
            raise
        logger.debug("ota: firmware transfer done")

        # 3. 结束并校验
        err = end_ota(self, expected_crc, enable=1, timeout=timeout)
        logger.debug("ota done: err=0x%x", err)
        return err == ErrCode.SUCCESS
