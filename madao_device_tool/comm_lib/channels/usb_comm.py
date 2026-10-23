"""USB 通信通道，基于 pyusb 实现。

支持两种连接模式（验收 3.1 末条要求 USB 区分直连/接收器，且单收归入多收）：

1. 直连模式（DIRECT）：鼠标/键盘通过 USB 线直连 PC。
   - VID=0x2480 PID=0x6698（对齐 mouseTool/mspeech_usb.py）
   - interface=1, EP_IN=0x83(endpoint 3 in), EP_OUT=0x06(endpoint 6 out)
   - 上位机 <-> 设备 直接收发私有协议帧
   对齐 src_archive/windows/libUsb/usb_manager.c

2. 多通道接收器模式（DONGLE）：dongle 接收器插 PC，鼠标/键盘无线连 dongle。
   - dongle 作为协议代理：上位机 <-> dongle <-> 多个子设备
   - 下行：上位机发给子设备 i 的私有帧 F，要包装成
         cmd=WRITE_DATA(0x45), payload=[0x02][mac[6]][F...]
     再走私有帧封装发给 dongle。
   - 上行：dongle 用 cmd=DEVICE_EVENT(0x49) 上报子设备事件/数据，
         payload=[type][version][state][mac[6]][data...], 其中 data 才是子设备私有帧。
   对齐 src_archive/windows/mspeech_usb_manager/mspeech_usb_manager.c
"""

from __future__ import annotations

import logging
import struct
import threading
import time
from typing import Callable, List, Optional

from .base import BaseComm, CommError, DeviceInfo
from ..protocol.cmd import DeviceManagerAction, UsbManagerCommand
from ..protocol.frame import (
    ProtocolDecoder,
    ProtocolFrame,
    build_frame,
    parse_frames,
)

logger = logging.getLogger(__name__)

# 对齐 mouseTool/mspeech_usb.py 末尾的 VID/PID
USB_VENDOR_ID = 0x2480
USB_PRODUCT_ID = 0x6698
USB_INTERFACE = 1
# 默认值对齐 C 源码 usb_manager.c（EP_IN=0x83, EP_OUT=0x06）。
# 实际设备的 endpoint 地址可能因固件版本不同（实测有 EP_OUT=0x04 的），
# connect() 时会自动从 interface 描述符探测并覆盖这两个值。
USB_EP_IN = 0x83
USB_EP_OUT = 0x06
USB_TIMEOUT_MS = 1000

# libusb 错误码（对齐 src_archive/windows/libUsb/include/libusb.h）
#   -1 IO / -4 NO_DEVICE：设备真的不在了
#   -9 PIPE：端点 stall（瞬时错误，设备仍在）
USB_ERROR_IO = -1
USB_ERROR_NO_DEVICE = -4
USB_ERROR_PIPE = -9
# Windows errno：19=ENODEV（设备拔出），4=EBUSY
WIN_ERRNO_NO_DEVICE = (19, 4)


class UsbDeviceInfo(DeviceInfo):
    def __init__(self, *, address: str, name: str = "", device_type: Optional[int] = None,
                 via_dongle: bool = False, extra: Optional[dict] = None) -> None:
        transport = "usb"
        super().__init__(transport=transport, address=address, name=name,
                         device_type=device_type, extra=extra)
        self.via_dongle = via_dongle


class UsbComm(BaseComm):
    """USB 通信通道。同一实例只能工作在一种模式。

    支持三种连接场景（验收 3.1 末条）：
        Mode.DIRECT        —— 设备 USB 线直连 PC（鼠标/键盘有线版）
        Mode.DONGLE        —— 多通道接收器，可同时挂多个子设备
        Mode.SINGLE_DONGLE —— 单通道接收器，仅挂一个子设备

    实现上 SINGLE_DONGLE 复用 DONGLE 的协议逻辑（dongle 代理协议相同，
    只是子设备数量为 1），对外保留独立 Mode 值以便上层区分场景。

    用法：
        # 直连
        comm = UsbComm(); comm.connect(UsbComm.Mode.DIRECT)
        # 接收器（单/多通道）
        comm = UsbComm(); comm.connect(UsbComm.Mode.DONGLE)
        comm.attach_subdevice(mac_bytes)
    """

    class Mode:
        DIRECT = "direct"
        DONGLE = "dongle"
        SINGLE_DONGLE = "single_dongle"

    def __init__(self, vendor_id: int = USB_VENDOR_ID, product_id: int = USB_PRODUCT_ID) -> None:
        super().__init__()
        try:
            import usb.core  # noqa: F401
            import usb.util  # noqa: F401
        except ImportError as e:
            raise CommError("缺少 pyusb 依赖，请 pip install pyusb") from e
        self._vid = vendor_id
        self._pid = product_id
        self._dev = None          # usb.core.Device
        self._mode: Optional[str] = None
        self._ep_in = USB_EP_IN   # 实例级 endpoint，connect 时自动探测覆盖
        self._ep_out = USB_EP_OUT
        self._rx_thread: Optional[threading.Thread] = None
        self._running = threading.Event()
        # dongle 模式下当前附着的子设备 mac（用于 send_raw 包装）
        self._sub_macs: List[bytes] = []
        self._dongle_decoder = ProtocolDecoder()  # 解 dongle 上报的外层帧
        # 写锁：dongle 下多个子设备并发发送（如并发 OTA）时，
        # 串行化 USB 写，防止两帧在总线上交错导致协议错乱。
        self._tx_lock = threading.Lock()
        # dongle 子设备被动掉线回调：(mac: bytes) -> None。
        # 由 device_mgr 挂接（转发到 drop_dongle_sub 清理设备池并通知 GUI）。
        self.on_subdevice_disconnected: Optional[Callable[[bytes], None]] = None

    # ---- 扫描 ----
    def scan(self) -> None:
        """扫描已插入的 USB 设备/dongle。

        直连模式：上报每个匹配 VID/PID 的 USB 设备（address="bus:dev"）。
        dongle 模式：连接 dongle 后通过 GET_CONNECTED_DEVICE 查询已挂载子设备
        （在 attach_subdevice 流程里触发，见 device_mgr）。
        """
        import usb.core
        import logging
        _logger = logging.getLogger(__name__)

        devs = list(usb.core.find(find_all=True, idVendor=self._vid, idProduct=self._pid))
        _logger.debug(f"USB scan: found {len(devs)} device(s) matching VID=0x{self._vid:04x} PID=0x{self._pid:04x}")
        for d in devs:
            dt = self._read_device_type(d)
            _logger.debug(f"  USB device bus={d.bus} addr={d.address} bcdDevice=0x{d.bcdDevice:04x} device_type={dt}")
            info = UsbDeviceInfo(
                address=f"{d.bus}:{d.address}",
                name=f"USB {self._vid:04x}:{self._pid:04x}",
                device_type=dt,
                via_dongle=False,
                extra={"bus": d.bus, "usb_address": d.address, "usb_device": d},
            )
            if self.on_device_found:
                self.on_device_found(info)

    @staticmethod
    def _read_device_type(d) -> Optional[int]:
        # 对齐 usb_manager.c::scan_device: device_type = bcdDevice >> 8
        try:
            return d.bcdDevice >> 8
        except Exception:
            return None

    # ---- 连接 / 断开 ----
    def connect(self, target, mode_hint: Optional[str] = None) -> None:
        """连接 USB 设备。

        target 可以是：
            UsbComm.Mode.* : 按默认 VID/PID 打开第一个匹配设备（DIRECT/DONGLE/SINGLE_DONGLE）
            DeviceInfo     : 用其 extra 中的 bus/usb_address 精确打开
        mode_hint: 当 target 是 DeviceInfo 时，可指定 "dongle" 强制走 dongle 模式
                   （默认按 via_dongle 属性判断，但该属性在直连扫描阶段总是 False）。
        """
        if self.is_connected:
            return
        import usb.core
        import usb.util

        if target == self.Mode.DIRECT:
            self._mode = self.Mode.DIRECT
            dev = usb.core.find(idVendor=self._vid, idProduct=self._pid)
        elif target in (self.Mode.DONGLE, self.Mode.SINGLE_DONGLE):
            # SINGLE_DONGLE 与 DONGLE 走同一套 dongle 代理协议，
            # 区别仅在于上层挂载的子设备数量（由 attach_subdevice 决定）。
            self._mode = target
            dev = usb.core.find(idVendor=self._vid, idProduct=self._pid)
        elif isinstance(target, DeviceInfo):
            if mode_hint == "dongle":
                self._mode = self.Mode.DONGLE
            elif mode_hint == "direct":
                self._mode = self.Mode.DIRECT
            else:
                # 按 USB device_type（bcdDevice>>8）自动判断：
                #   1=单通道接收器，3=USB 直连 → direct 模式（直接私有协议通信）
                #   2=多联接收器 → dongle 模式（需 dongle 代理协议）
                dt = target.device_type
                if dt == 2:
                    self._mode = self.Mode.DONGLE
                else:
                    self._mode = self.Mode.DIRECT
            bus = target.extra.get("bus")
            addr = target.extra.get("usb_address")
            # 优先用 scan 时缓存的 usb_device 对象
            # （find_all 在设备被 claim 后返回缓存，bus/addr 会错乱）
            cached_dev = target.extra.get("usb_device")
            if cached_dev is not None:
                dev = cached_dev
            elif bus and addr:
                dev = None
                for d in usb.core.find(find_all=True, idVendor=self._vid, idProduct=self._pid):
                    if d.bus == bus and d.address == addr:
                        dev = d
                        break
            else:
                dev = None
        else:
            raise CommError(f"不支持的 connect target: {target!r}")

        if dev is None:
            raise CommError("未找到匹配的 USB 设备")

        # 对齐 usb_manager.c::_init_new：detach kernel driver + claim interface
        try:
            if dev.is_kernel_driver_active(USB_INTERFACE):
                dev.detach_kernel_driver(USB_INTERFACE)
        except Exception:
            pass  # 非 Linux 或无内核驱动
        try:
            dev.set_configuration()
        except usb.core.USBError:
            pass  # 已配置
        try:
            usb.util.claim_interface(dev, USB_INTERFACE)
        except usb.core.USBError as e:
            raise CommError(f"claim interface 失败: {e}") from e

        # 从 interface 描述符自动探测 IN/OUT endpoint 地址。
        # C 源码硬编码 EP_OUT=0x06，但实测部分固件是 EP_OUT=0x04，硬编码会失败。
        ep_in, ep_out = self._detect_endpoints(dev)
        self._ep_in = ep_in
        self._ep_out = ep_out

        self._dev = dev
        self._start_rx()

    @staticmethod
    def _detect_endpoints(dev) -> tuple[int, int]:
        """从 USB_INTERFACE 描述符探测 IN/OUT interrupt endpoint 地址。"""
        import usb.util
        cfg = dev.get_active_configuration()
        intf = usb.util.find_descriptor(cfg, bInterfaceNumber=USB_INTERFACE)
        if intf is None:
            # 回退到默认值
            return USB_EP_IN, USB_EP_OUT
        ep_in = USB_EP_IN
        ep_out = USB_EP_OUT
        for ep in intf:
            addr = ep.bEndpointAddress
            if addr & 0x80:  # IN
                ep_in = addr
            else:            # OUT
                ep_out = addr
        return ep_in, ep_out

    def _start_rx(self):
        self._running.set()
        self._rx_thread = threading.Thread(target=self._rx_loop, name="usb-rx", daemon=True)
        self._rx_thread.start()

    def _rx_loop(self):
        import usb.core
        import logging
        # 注意：不能用 "usb.rx" 这类顶层 usb.* logger 名——pyusb 在 import 时
        # 把 "usb" logger 设为 CRITICAL+10，usb.* 下的日志会被全部吞掉。
        log = logging.getLogger(__name__).getChild("rx")
        dev = self._dev
        log.debug("USB RX loop started, ep_in=0x%02X, dev=%s", self._ep_in, dev is not None)
        while self._running.is_set() and dev is not None:
            try:
                data = dev.read(self._ep_in, 64, timeout=USB_TIMEOUT_MS)
                log.debug("USB RX %d bytes: %s", len(data), bytes(data).hex())
                self._handle_rx(bytes(data))
            except usb.core.USBTimeoutError:
                continue  # 正常超时，继续
            except usb.core.USBError as e:
                code = getattr(e, 'backend_error_code', None)
                os_errno = getattr(e, 'errno', None)
                # 只有"设备真的不在了"才算掉线。
                # 其它错误（如 PIPE=-9 端点 stall）按 C 库 usb_manager.c::_start_read 的做法：
                # Sleep(1) 后继续读，不能因为一次瞬时错误就把连接拆掉
                # （否则 OTA 传输中一次 stall 就会让设备被判为"未连接"）。
                if code in (USB_ERROR_IO, USB_ERROR_NO_DEVICE) or os_errno in WIN_ERRNO_NO_DEVICE:
                    log.warning("USB 设备已断开: %s (backend_code=%s)", e, code)
                    self._running.clear()
                    self._notify_disconnected()
                    break
                log.warning("USB RX 瞬时错误（继续读取）: %s (backend_code=%s)", e, code)
                if code == USB_ERROR_PIPE:
                    # 端点 stall：清掉 halt 状态后再读
                    try:
                        dev.clear_halt(self._ep_in)
                    except Exception:
                        pass
                time.sleep(0.05)

    def _handle_rx(self, data: bytes):
        """处理 USB 接收到的原始字节。

        直连模式：直接喂私有协议解码器。
        dongle 模式：先解 dongle 外层帧（cmd=DEVICE_EVENT 等），从中剥离子设备私有帧。
        """
        if self._mode == self.Mode.DIRECT:
            self._feed_rx(data)
            return
        # dongle 模式
        self._dongle_decoder.feed(data)
        for outer in self._dongle_decoder.take_frames():
            self._handle_dongle_frame(outer)

    def _handle_dongle_frame(self, outer: ProtocolFrame):
        """处理 dongle 上报的外层帧。"""
        import logging
        # pyusb 把顶层 "usb" logger 设为 CRITICAL+10，usb.* 下的日志会被吞掉，
        # 因此这里挂在 comm_lib.channels.usb_comm 命名空间下。
        _log = logging.getLogger(__name__).getChild("dongle")
        _log.debug(
            "RX dongle frame cmd=0x%02X payload(%d)=%s",
            outer.cmd, len(outer.payload), outer.payload.hex()
        )
        if outer.cmd == UsbManagerCommand.DEVICE_EVENT:
            # payload=[type][version][state/rssi][mac[6]][data...]
            # 对齐 mspeech_usb_manager.c::_get_usb_event_data
            p = outer.payload
            if len(p) < 9:
                return
            event_type = p[0]
            if event_type == 0x03:  # EVENT_TYPE_SCAN_DEVICE
                # 扫描应答与连接事件共用 DEVICE_EVENT 通道（对齐 lib_usb_manager.c
                # protocol_callback: EVENT_TYPE_SCAN_DEVICE 也从 cmd=DEVICE_EVENT 来）
                self._dispatch_dongle_scan(p)
                return
            state = p[2]
            mac = bytes(p[3:9])
            inner = bytes(p[9:])
            self._dispatch_dongle_event(event_type, state, mac, inner)
        elif outer.cmd == UsbManagerCommand.WRITE_DATA_RESP:
            # 子设备对透传指令的应答，对齐 mspeech_usb_manager.c::_get_usb_write_data:
            # payload=[type][mac[6]][data...], data 是子设备的私有协议帧
            p = outer.payload
            if len(p) < 7:
                return
            mac = bytes(p[1:7])
            inner = bytes(p[7:])
            if inner:
                for frame in parse_frames(inner):
                    # 标记来源子设备：多子设备共用一个 dongle 时，
                    # 上层据此把应答隔离到各自设备（并发升级必需）。
                    frame.source_mac = mac
                    if self.on_frame:
                        self.on_frame(frame)
        elif outer.cmd == UsbManagerCommand.SCAN_DEVICE:
            # dongle 扫描应答：payload=[type][version][rssi][mac[6]][data...]
            self._dispatch_dongle_scan(outer.payload)

    def _dispatch_dongle_event(self, event_type: int, state: int, mac: bytes, inner: bytes):
        """dispatch dongle DEVICE_EVENT。"""
        from ..protocol.cmd import Command
        # 子设备连接/断开（对齐 mspeech_usb_manager.h::USB_EVENT_TYPE）
        if event_type == 0x01:  # EVENT_TYPE_CONNECTED_DEVICE
            # GET_CONNECTED_DEVICE 查询和真实上线都走本事件：
            # state=1 已连接，其它=已配对未连接。以此维护 _sub_macs 权威状态。
            if state == 1 and mac not in self._sub_macs:
                self._sub_macs.append(mac)
            elif state != 1 and mac in self._sub_macs:
                self._sub_macs = [m for m in self._sub_macs if m != mac]
            if self.on_device_found:
                self.on_device_found(UsbDeviceInfo(
                    address=mac.hex(":"),
                    name=f"sub-{mac.hex()}",
                    via_dongle=True,
                    extra={"mac": mac, "dongle_state": state},
                ))
        elif event_type == 0x02:  # EVENT_TYPE_DISCONNECT_DEVICE
            # 子设备掉线：从 mac 列表移除，并通知上层清理设备池/刷新 GUI
            self._sub_macs = [m for m in self._sub_macs if m != mac]
            if self.on_subdevice_disconnected is not None:
                try:
                    self.on_subdevice_disconnected(mac)
                except Exception:
                    logger.debug("on_subdevice_disconnected callback failed", exc_info=True)
        # inner 可能是子设备发的私有协议帧（透传）
        if inner:
            for frame in parse_frames(inner):
                # 标记来源子设备（见 WRITE_DATA_RESP 分支说明）
                frame.source_mac = mac
                if self.on_frame:
                    self.on_frame(frame)

    def _dispatch_dongle_scan(self, payload: bytes):
        import logging
        _log = logging.getLogger(__name__).getChild("dongle")
        if len(payload) < 9:
            _log.warning(
                "SCAN resp too short (%d): %s", len(payload), payload.hex()
            )
            return
        version = payload[1]
        rssi = payload[2]
        mac = bytes(payload[3:9])
        # data 区是设备广播名（对齐 mspeech_usb_scan_data: [version][rssi][mac][data]）
        name = ""
        data = bytes(payload[9:])
        if data:
            name = data.split(b"\x00")[0].decode("utf-8", errors="ignore").strip()
        _log.debug(
            "SCAN result mac=%s rssi=%d version=%d name=%s",
            mac.hex(":"), rssi, version, name,
        )
        if self.on_device_found:
            self.on_device_found(UsbDeviceInfo(
                address=mac.hex(":"),
                name=name or f"scan-{mac.hex()}",
                via_dongle=True,
                extra={"mac": mac, "rssi": rssi, "version": version},
            ))

    def attach_subdevice(self, mac: bytes) -> None:
        """dongle 模式下：通知 dongle 连接指定 mac 的子设备。

        对齐 mspeech_usb_manager.c::_connect_device:
          cmd=DEVICE_MANAGER, payload=[CONNECT_DEVICE(0x01)][mac[6]]
        """
        if self._mode == self.Mode.DIRECT or self._dev is None:
            raise CommError("仅 dongle 模式且已连接时可 attach 子设备")
        payload = bytes([DeviceManagerAction.CONNECT_DEVICE]) + bytes(mac)[:6]
        from ..protocol.frame import SequenceCounter
        seq = SequenceCounter().next()
        outer_frame = build_frame(UsbManagerCommand.DEVICE_MANAGER, payload, seq)
        self._write_raw_ep(outer_frame)
        if mac not in self._sub_macs:
            self._sub_macs.append(mac)

    def detach_subdevice(self, mac: bytes) -> None:
        """dongle 模式下：通知 dongle 断开指定子设备。

        对齐 mspeech_usb_manager.c::_remove_device:
          cmd=DEVICE_MANAGER, payload=[REMOVE_DEVICE(0x02)][mac[6]]
        """
        if self._dev is None:
            return
        payload = bytes([DeviceManagerAction.REMOVE_DEVICE]) + bytes(mac)[:6]
        from ..protocol.frame import SequenceCounter
        seq = SequenceCounter().next()
        outer_frame = build_frame(UsbManagerCommand.DEVICE_MANAGER, payload, seq)
        self._write_raw_ep(outer_frame)
        self._sub_macs = [m for m in self._sub_macs if m != mac]

    def disconnect(self) -> None:
        """断开，释放 USB 句柄。对齐 usb_manager.c::_uninit / _stop。"""
        self._running.clear()
        # 被动拔出时本方法由 RX 线程触发（_notify_disconnected 回调链），
        # 线程不能 join 自己，否则抛 RuntimeError 跳过后续句柄释放。
        if (self._rx_thread and self._rx_thread.is_alive()
                and self._rx_thread is not threading.current_thread()):
            self._rx_thread.join(timeout=2.0)
        self._rx_thread = None
        dev = self._dev
        if dev is not None:
            try:
                import usb.util
                usb.util.release_interface(dev, USB_INTERFACE)
                dev.reset()
            except Exception:
                pass
            try:
                import usb.util
                usb.util.dispose_resources(dev)
            except Exception:
                pass
        self._dev = None
        self._sub_macs.clear()
        self._mode = None
        self._dongle_decoder = ProtocolDecoder()

    @property
    def is_connected(self) -> bool:
        return self._dev is not None and self._mode is not None

    # ---- 发送 ----
    def send_raw(self, frame: bytes) -> int:
        """发送一个完整帧。

        直连模式：直接走 EP_OUT。
        dongle 模式：frame 是发给某个子设备的私有协议帧，
                     要包装成 cmd=WRITE_DATA(0x45), payload=[0x02][mac[6]][frame...]。
        """
        if self._dev is None:
            raise CommError("USB 未连接")
        if self._mode == self.Mode.DIRECT:
            return self._write_raw_ep(frame)
        # dongle 模式：需要有附着的子设备
        if not self._sub_macs:
            raise CommError("dongle 模式下尚未 attach 任何子设备")
        mac = self._sub_macs[0]  # 简化：单子设备；多子设备时由 device_mgr 明确指定
        return self.send_to_subdevice(mac, frame)

    def send_to_subdevice(self, mac: bytes, frame: bytes) -> int:
        """dongle 模式下：把私有帧 frame 转发给指定 mac 的子设备。

        对齐 mspeech_usb_manager.c::_convert_to_usb_data:
          payload = [type=0x02][mac[6]][frame...]
          cmd = WRITE_DATA(0x45)
        再用私有帧封装发给 dongle。
        """
        if self._dev is None:
            raise CommError("USB 未连接")
        payload = bytes([0x02]) + bytes(mac)[:6] + frame
        from ..protocol.frame import SequenceCounter
        seq = SequenceCounter().next()
        outer_frame = build_frame(UsbManagerCommand.WRITE_DATA, payload, seq)
        return self._write_raw_ep(outer_frame)

    def _write_raw_ep(self, data: bytes) -> int:
        """走 EP_OUT 写原始字节。对齐 usb_manager.c::_write。"""
        import logging
        _log = logging.getLogger(__name__).getChild("tx")
        _log.debug("USB TX %d bytes: %s", len(data), data.hex())
        dev = self._dev
        try:
            # 写锁：并发升级时多个子设备的帧不能交错到同一次 USB 写里
            with self._tx_lock:
                written = dev.write(self._ep_out, data, timeout=USB_TIMEOUT_MS)
            return int(written)
        except Exception as e:
            _log.warning(
                "USB TX failed: %s (backend_code=%s, ep_out=0x%02X)",
                e, getattr(e, 'backend_error_code', '?'), self._ep_out)
            raise CommError(f"USB 写入失败: {e}") from e

    # ---- dongle 主动扫描子设备 ----
    def dongle_scan_subdevices(self, scan_ms: int = 3000, rssi: int = 0) -> None:
        """dongle 模式下：触发 dongle 扫描周围的 BLE 子设备。

        对齐 mspeech_usb_manager.c::_scan_device:
          cmd=SCAN_DEVICE(0x41), payload=[scan_ms(4 LE)][rssi(1)]
        扫描结果通过 on_device_found 回调（dongle 用 SCAN_DEVICE cmd 上报）。
        """
        if self._mode == self.Mode.DIRECT or self._dev is None:
            raise CommError("仅 dongle 模式且已连接时可扫描子设备")
        payload = struct.pack("<I", int(scan_ms)) + bytes([rssi & 0xFF])
        from ..protocol.frame import SequenceCounter
        seq = SequenceCounter().next()
        outer_frame = build_frame(UsbManagerCommand.SCAN_DEVICE, payload, seq)
        self._write_raw_ep(outer_frame)

    def dongle_query_connected(self) -> None:
        """dongle 模式下：查询 dongle 当前已挂载的子设备列表。

        对齐 mspeech_usb_manager.c::_get_connected_device:
          cmd=DEVICE_MANAGER(0x48), payload=[GET_CONNECTED_DEVICE(0x03)]
        结果通过 on_frame / on_device_found 回调（dongle 用 DEVICE_EVENT 上报）。
        """
        if self._mode == self.Mode.DIRECT or self._dev is None:
            raise CommError("仅 dongle 模式且已连接时可查询子设备")
        payload = bytes([DeviceManagerAction.GET_CONNECTED_DEVICE])
        from ..protocol.frame import SequenceCounter
        seq = SequenceCounter().next()
        outer_frame = build_frame(UsbManagerCommand.DEVICE_MANAGER, payload, seq)
        self._write_raw_ep(outer_frame)
