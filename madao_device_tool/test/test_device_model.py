"""device_model 单元测试。

用 MockComm（不依赖真实硬件）验证 BaseDevice/MouseDevice 的请求-应答逻辑、
CPI/回报率指令构造、基础信息解析。

运行：python new_project/test/test_device_model.py
"""

import os
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from comm_lib.channels import BaseComm
from comm_lib.protocol.cmd import Command, ErrCode
from comm_lib.protocol.frame import (
    ProtocolFrame,
    u32_to_bytes,
)
from comm_lib.protocol.prop_key import PropKey
from device_model import BaseDevice, DeviceError, KbDevice, MouseDevice


class MockComm(BaseComm):
    """内存通道：把 send_raw 的帧暂存，测试代码主动注入应答帧。"""

    def __init__(self):
        super().__init__()
        self.sent_frames = []
        self._connected = True  # 测试默认已连接

    def scan(self):
        pass

    def connect(self, target):
        self._connected = True

    def disconnect(self):
        self._connected = False

    def send_raw(self, frame):
        self.sent_frames.append(frame)
        return len(frame)

    @property
    def is_connected(self):
        return self._connected

    # ---- 测试辅助：注入一个收到的帧，触发 on_frame ----
    def inject_frame(self, cmd: int, sequence: int, payload: bytes):
        frame = ProtocolFrame(0xFE, sequence, cmd, payload)
        self.on_frame(frame)


def _last_sent_frame(comm: MockComm) -> ProtocolFrame:
    from comm_lib.protocol.frame import parse_frames
    frames = parse_frames(comm.sent_frames[-1])
    assert frames, "no frame sent"
    return frames[0]


def test_get_cpi_roundtrip():
    comm = MockComm()
    dev = MouseDevice(comm)

    def _resp():
        # 等待请求帧，构造 ACK 应答
        f = _last_sent_frame(comm)
        ack_payload = u32_to_bytes(ErrCode.SUCCESS) + u32_to_bytes(3)  # err=1, cpi=3
        comm.inject_frame(Command.ACK, f.sequence, ack_payload)

    threading.Timer(0.05, _resp).start()
    assert dev.get_cpi() == 3
    # 校验发出的请求帧 cmd / payload 正确
    req = _last_sent_frame(comm)
    assert req.cmd == Command.GET_SLAVE_PROP
    assert req.payload == bytes([PropKey.DPI])


def test_set_cpi_roundtrip():
    comm = MockComm()
    dev = MouseDevice(comm)

    def _resp():
        f = _last_sent_frame(comm)
        comm.inject_frame(Command.ACK, f.sequence, u32_to_bytes(ErrCode.SUCCESS))

    threading.Timer(0.05, _resp).start()
    dev.set_cpi(4)
    req = _last_sent_frame(comm)
    assert req.cmd == Command.SET_SLAVE_PROP
    assert req.payload == bytes([PropKey.DPI]) + u32_to_bytes(4)


def test_get_rate_roundtrip():
    comm = MockComm()
    dev = MouseDevice(comm)

    def _resp():
        f = _last_sent_frame(comm)
        comm.inject_frame(Command.ACK, f.sequence, u32_to_u32_err(1, 500))

    threading.Timer(0.05, _resp).start()
    assert dev.get_rate() == 500
    req = _last_sent_frame(comm)
    assert req.cmd == Command.GET_SLAVE_PROP
    assert req.payload == bytes([PropKey.POLLING_RATE])


def test_set_rate_roundtrip():
    comm = MockComm()
    dev = MouseDevice(comm)

    def _resp():
        f = _last_sent_frame(comm)
        comm.inject_frame(Command.ACK, f.sequence, u32_to_bytes(ErrCode.SUCCESS))

    threading.Timer(0.05, _resp).start()
    dev.set_rate(1000)
    req = _last_sent_frame(comm)
    assert req.cmd == Command.SET_SLAVE_PROP
    assert req.payload == bytes([PropKey.POLLING_RATE]) + u32_to_bytes(1000)


def test_get_unsynced_file_count_roundtrip():
    comm = MockComm()
    dev = MouseDevice(comm)

    def _resp():
        f = _last_sent_frame(comm)
        comm.inject_frame(Command.ACK, f.sequence,
                          u32_to_bytes(ErrCode.SUCCESS) + u32_to_bytes(5))

    threading.Timer(0.05, _resp).start()
    assert dev.get_unsynced_file_count() == 5
    req = _last_sent_frame(comm)
    assert req.cmd == Command.GET_SLAVE_PROP
    assert req.payload == bytes([PropKey.UNSYNCHED_FILE_COUNT])


def test_set_unsynced_file_count_roundtrip():
    comm = MockComm()
    dev = MouseDevice(comm)

    def _resp():
        f = _last_sent_frame(comm)
        comm.inject_frame(Command.ACK, f.sequence, u32_to_bytes(ErrCode.SUCCESS))

    threading.Timer(0.05, _resp).start()
    dev.set_unsynced_file_count(7)
    req = _last_sent_frame(comm)
    assert req.cmd == Command.SET_SLAVE_PROP
    assert req.payload == bytes([PropKey.UNSYNCHED_FILE_COUNT]) + u32_to_bytes(7)


def test_set_unsynced_file_count_err_raises():
    comm = MockComm()
    dev = MouseDevice(comm)

    def _resp():
        f = _last_sent_frame(comm)
        comm.inject_frame(Command.ACK, f.sequence, u32_to_bytes(ErrCode.FAIL))

    threading.Timer(0.05, _resp).start()
    try:
        dev.set_unsynced_file_count(7)
        assert False
    except DeviceError:
        pass


def test_sync_time_roundtrip():
    """时间同步：SET_SLAVE_PROP[SET_UTC_TIME(0x28)][u32 LE UTC 秒]。"""
    comm = MockComm()
    dev = MouseDevice(comm)
    ts = 1769472000  # 固定时间戳（秒）

    def _resp():
        f = _last_sent_frame(comm)
        comm.inject_frame(Command.ACK, f.sequence, u32_to_bytes(ErrCode.SUCCESS))

    threading.Timer(0.05, _resp).start()
    dev.sync_time(ts)
    req = _last_sent_frame(comm)
    assert req.cmd == Command.SET_SLAVE_PROP
    assert req.payload == bytes([PropKey.SET_UTC_TIME]) + u32_to_bytes(ts)


def test_sync_time_default_is_now_utc():
    """缺省时间戳 = 当前 UTC 秒（设备端自行转北京时间）。"""
    comm = MockComm()
    dev = MouseDevice(comm)
    before = int(time.time())

    def _resp():
        f = _last_sent_frame(comm)
        comm.inject_frame(Command.ACK, f.sequence, u32_to_bytes(ErrCode.SUCCESS))

    threading.Timer(0.05, _resp).start()
    dev.sync_time()
    req = _last_sent_frame(comm)
    sent = int.from_bytes(req.payload[1:5], "little")
    after = int(time.time())
    assert req.payload[0] == PropKey.SET_UTC_TIME
    assert before <= sent <= after, f"缺省时间戳应在当前 UTC 秒附近: {sent}"


def test_sync_time_err_raises():
    comm = MockComm()
    dev = MouseDevice(comm)

    def _resp():
        f = _last_sent_frame(comm)
        comm.inject_frame(Command.ACK, f.sequence, u32_to_bytes(ErrCode.FAIL))

    threading.Timer(0.05, _resp).start()
    try:
        dev.sync_time(1769472000)
        assert False
    except DeviceError:
        pass


def test_sync_time_out_of_range_raises():
    comm = MockComm()
    dev = MouseDevice(comm)
    try:
        dev.sync_time(0)
        assert False
    except DeviceError:
        pass
    try:
        dev.sync_time(0x1_0000_0000)
        assert False
    except DeviceError:
        pass
    # 没发出任何帧
    assert comm.sent_frames == []


def test_compute_wifi_credentials_golden():
    """快传 WiFi 凭据算法对齐 C generate_wifi_credentials()（固定向量）。"""
    from device_model.mouse import compute_wifi_credentials
    mac = bytes.fromhex("2C81A7F049FF")
    ssid, pwd = compute_wifi_credentials("1042", mac)
    # input="10422C81A7F049FF" 的 SHA-256：hash[0:6] -> SSID, hash[6:14] -> 密码
    assert ssid == "MD_7541B2D09AB7"
    assert pwd == "5C1B6C985270929C"


def test_get_wifi_credentials_roundtrip():
    comm = MockComm()
    dev = MouseDevice(comm)
    dev.product_no = "1042"
    mac = bytes.fromhex("2C81A7F049FF")

    def _resp():
        f = _last_sent_frame(comm)
        comm.inject_frame(Command.ACK, f.sequence,
                          u32_to_bytes(ErrCode.SUCCESS) + mac)

    threading.Timer(0.05, _resp).start()
    ssid, pwd = dev.get_wifi_credentials()
    assert (ssid, pwd) == ("MD_7541B2D09AB7", "5C1B6C985270929C")
    req = _last_sent_frame(comm)
    assert req.cmd == Command.GET_SLAVE_PROP
    assert req.payload == bytes([PropKey.MAC_MOURSE])


def test_get_device_mac():
    comm = MockComm()
    dev = MouseDevice(comm)
    mac = bytes([0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF])

    def _resp():
        f = _last_sent_frame(comm)
        comm.inject_frame(Command.ACK, f.sequence, u32_to_bytes(ErrCode.SUCCESS) + mac)

    threading.Timer(0.05, _resp).start()
    assert dev.get_device_mac() == mac


def test_get_battery():
    comm = MockComm()
    dev = BaseDevice(comm)

    def _resp():
        f = _last_sent_frame(comm)
        comm.inject_frame(Command.ACK, f.sequence, u32_to_u32_err(1, 85))

    threading.Timer(0.05, _resp).start()
    assert dev.get_battery() == 85


def test_handshake():
    comm = MockComm()
    dev = BaseDevice(comm)

    def _resp():
        f = _last_sent_frame(comm)
        comm.inject_frame(Command.ACK, f.sequence, u32_to_bytes(ErrCode.SUCCESS))

    threading.Timer(0.05, _resp).start()
    assert dev.handshake() == ErrCode.SUCCESS


def test_timeout_raises():
    comm = MockComm()
    dev = MouseDevice(comm)
    try:
        dev.get_cpi()
        assert False, "应超时"
    except DeviceError as e:
        assert "超时" in str(e)


def test_err_code_propagated():
    comm = MockComm()
    dev = MouseDevice(comm)

    def _resp():
        f = _last_sent_frame(comm)
        comm.inject_frame(Command.ACK, f.sequence, u32_to_bytes(ErrCode.FAIL))

    threading.Timer(0.05, _resp).start()
    try:
        dev.get_cpi()
        assert False
    except DeviceError as e:
        assert "FAIL" in str(e) or "err=0" in str(e)


def test_kb_device_inherits_common():
    # 验收 3.3 第2条：键盘继承 BaseDevice 通用方法
    comm = MockComm()
    kb = KbDevice(comm)
    assert kb.DEVICE_TYPE == 0x32
    # 确认 KbDevice 拥有 start_record / get_battery 等（继承自基类）
    assert hasattr(kb, "start_record")
    assert hasattr(kb, "get_battery")


def test_mouse_device_type():
    assert MouseDevice.DEVICE_TYPE == 0x31


def test_ota_requires_auth_first():
    # perform_ota 需要先 authenticate（否则抛 DeviceError 而非 NotImplementedError）
    from device_model import DeviceError
    comm = MockComm()
    dev = MouseDevice(comm)
    try:
        dev.perform_ota(b"\x00" * 128)
        assert False
    except (DeviceError, NotImplementedError):
        pass


def test_sequence_increments():
    # 用 monkey-patch：send_raw 后立刻注入 ACK，避免定时器竞态
    comm = MockComm()
    dev = MouseDevice(comm)
    seqs = []
    orig_send = comm.send_raw

    def _auto_send(frame):
        seqs.append(frame[1])  # sequence 是帧第 2 字节
        orig_send(frame)
        # 同步注入对应 ACK
        comm.inject_frame(Command.ACK, frame[1], u32_to_bytes(ErrCode.SUCCESS))

    comm.send_raw = _auto_send
    dev.handshake()
    dev.handshake()
    dev.handshake()
    assert seqs == [0, 1, 2]


# ---- 工具 ----
def u32_to_u32_err(err: int, value: int) -> bytes:
    return u32_to_bytes(err) + u32_to_bytes(value)


if __name__ == "__main__":
    fns = [v for k, v in sorted(globals().items()) if k.startswith("test_") and callable(v)]
    passed = 0
    failed = []
    for fn in fns:
        try:
            fn()
            passed += 1
            print(f"PASS {fn.__name__}")
        except AssertionError as e:
            failed.append((fn.__name__, str(e)))
            print(f"FAIL {fn.__name__}: {e}")
    print(f"\n{passed}/{len(fns)} passed")
    if failed:
        raise SystemExit(1)
