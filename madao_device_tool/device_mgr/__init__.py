"""设备管理器：扫描、连接、多设备池管理。

职责（对照验收 3.3 / 3.4）：
- scan_ble() / scan_usb() 两个独立扫描入口
- 扫描结果区分鼠标/键盘（device_type）
- connect(info) 按 transport 选择对应通道，创建设备实例并入池
- 设备池支持遍历、单独断开、全部断开
- 断连自动释放通信资源
- dongle 模式下多子设备共用同一个 UsbComm，按 mac 路由发送
"""

from __future__ import annotations

import logging
import threading
from typing import Callable, Dict, Iterator, List, Optional

from comm_lib.channels import BaseComm, CommError, DeviceInfo
from comm_lib.channels.usb_comm import UsbComm, UsbDeviceInfo
from device_config import get_device_config, infer_device_type
from device_model import BaseDevice, KbDevice, MouseDevice

logger = logging.getLogger(__name__)

# 设备类型常量（对齐 lib_device_define.h）
DEVICE_TYPE_MOUSE = 0x31
DEVICE_TYPE_KEYBOARD = 0x32

# product_no 首位 -> 设备类型字符串（用于 device_model 子类选择）
_PREFIX_TYPE_MAP = {"1": "mouse", "3": "keyboard"}

# USB device_type（bcdDevice >> 8）区分 USB 设备形态：
#   1 = 单通道接收器（dongle），2 = 多联接收器（多通道 dongle），3 = USB 直连
USB_DEVICE_TYPE_SINGLE_DONGLE = 1
USB_DEVICE_TYPE_MULTI_DONGLE = 2
USB_DEVICE_TYPE_DIRECT = 3
_DONGLE_TYPES = {USB_DEVICE_TYPE_SINGLE_DONGLE, USB_DEVICE_TYPE_MULTI_DONGLE}


class _DongleSubCommAdapter(BaseComm):
    """dongle 模式下，每个子设备一个 adapter，把 send_raw 路由到对应 mac。

    多个子设备共享同一个底层 UsbComm（dongle），但各自绑定不同 mac。
    通过本 adapter 让 BaseDevice.send_raw 无感地走 UsbComm.send_to_subdevice。
    """

    def __init__(self, dongle: UsbComm, mac: bytes) -> None:
        super().__init__()
        self._dongle = dongle
        self._mac = mac
        # 复用底层 dongle 的帧回调：dongle 已在 _handle_dongle_frame 里分发到 on_frame，
        # 这里让本 adapter 的 on_frame 由上层 device 设置后，也挂到 dongle.on_frame。
        # 但多个 adapter 共用一个 dongle.on_frame 会冲突——改用 mac 过滤。
        self._installed = False

    def install_frame_router(self):
        """把 dongle 的帧按 mac 路由到本 adapter。

        dongle 的 _handle_dongle_frame 把子设备内层私有帧上抛时，
        已在帧上打上 source_mac（来源子设备 mac）。本 adapter 只接收
        属于自己 mac 的帧——多个子设备共用一个 dongle 时，各设备的
        sequence 计数是各自独立的（会撞号），必须按 mac 隔离，
        否则并发请求（如并发 OTA）的 ACK 会串到别的设备上。
        """
        if self._installed:
            return
        parent_on_frame = self._dongle.on_frame

        def _route(frame):
            if frame.source_mac is not None and frame.source_mac != self._mac:
                return  # 别的子设备的帧，丢弃
            if self.on_frame:
                self.on_frame(frame)

        # 串联：dongle.on_frame 先调原有（如有），再调本 adapter
        if parent_on_frame is None:
            self._dongle.on_frame = _route
        else:
            def _chained(frame, _prev=parent_on_frame):
                _prev(frame)
                _route(frame)
            self._dongle.on_frame = _chained
        self._installed = True

    # ---- BaseComm 抽象方法 ----
    def scan(self) -> None:
        self._dongle.scan()

    def connect(self, target) -> None:
        # 连接动作已在 DeviceManager 层完成（attach_subdevice）
        pass

    def disconnect(self) -> None:
        # 不 detach：程序退出 / 被动清理时不应解除子设备与 dongle 的配对，
        # 否则每次关工具都会把键鼠从接收器上解绑。
        # 用户主动断开走 DonglePanel 的 detach_subdevice()。
        pass

    def send_raw(self, frame: bytes) -> int:
        return self._dongle.send_to_subdevice(self._mac, frame)

    @property
    def is_connected(self) -> bool:
        return self._dongle.is_connected and self._mac in self._dongle._sub_macs


class DeviceManager:
    """设备管理器。统一管理 BLE / USB 直连 / USB dongle 三类通道上的设备。"""

    def __init__(self) -> None:
        self._ble_comm: Optional[BaseComm] = None
        self._usb_direct: Optional[UsbComm] = None
        self._dongle: Optional[UsbComm] = None
        # 持有所有活跃的 comm 引用，防止 GC 回收导致句柄释放
        self._all_comms: list = []
        self._devices: Dict[str, BaseDevice] = {}  # key = address
        self._lock = threading.Lock()
        self.on_scan_result: Optional[Callable[[DeviceInfo], None]] = None
        # dongle 子设备被动掉线回调：(mac_str) -> None（GUI 据此把设备从列表移除）
        self.on_dongle_sub_disconnected: Optional[Callable[[str], None]] = None
        # dongle 被动拔出回调：() -> None
        self.on_dongle_removed: Optional[Callable[[], None]] = None
        # 设备被动断开回调：(address) -> None（USB 拔出 / BLE 掉线等，
        # GUI 据此自动刷新设备列表，无需手动刷新）
        self.on_device_disconnected: Optional[Callable[[str], None]] = None

    # ---- 扫描（验收 3.4 首条：BLE/USB 独立扫描入口） ----
    def scan_ble(self, scan_timeout: float = 5.0) -> List[DeviceInfo]:
        """触发 BLE 广播扫描。结果通过 on_scan_result 回调 + 返回值。"""
        from comm_lib.channels.ble_comm import BleComm

        comm = BleComm(scan_timeout=scan_timeout)
        results: List[DeviceInfo] = []
        comm.on_device_found = lambda info: (results.append(info), self._notify_scan(info))
        comm.scan()
        return results

    def scan_ble_bonded(self) -> List[DeviceInfo]:
        """列出 Windows 已配对已连接的 BLE 设备（非广播扫描）。

        用 PowerShell 查询系统设备树，返回已配对的 BLE 设备列表。
        比 scan_ble 更精准——只返回已配对的设备，不包含周围无关设备。
        """
        from comm_lib.channels.ble_device_enumeration import list_paired_ble_devices
        results = list_paired_ble_devices()
        for info in results:
            self._notify_scan(info)
        return results

    def scan_usb(self) -> List[DeviceInfo]:
        """扫描直连的 USB 设备。

        已连接的 USB 设备被 claim 后 usb.core.find 可能枚举不到，
        所以扫描结果为空时，把已连接的 USB 设备也包含进去（避免丢失）。
        """
        comm = UsbComm()
        results: List[DeviceInfo] = []
        comm.on_device_found = lambda info: (results.append(info), self._notify_scan(info))
        comm.scan()

        # find 枚举不到已 claim 的设备，补充已连接的 USB 设备
        if not results:
            for dev in self._devices.values():
                if dev.transport == "usb":
                    # 已连接的设备直接作为扫描结果返回（用已缓存的 info）
                    addr = dev.address
                    dtype = 3  # 默认直连
                    if dev.connection_type in ("usb_dongle", "usb_single_dongle"):
                        dtype = 2 if dev.connection_type == "usb_dongle" else 1
                    results.append(UsbDeviceInfo(
                        address=addr,
                        name=dev.name or "",
                        device_type=dtype,
                        via_dongle=False,
                    ))
        return results

    def _notify_scan(self, info: DeviceInfo):
        if self.on_scan_result:
            self.on_scan_result(info)

    # ---- 连接（验收 3.4 第3条：根据 Mac/ProductNo 精准连接） ----
    def connect_dongle_only(self, info: UsbDeviceInfo) -> UsbComm:
        """仅建立多联接收器（dongle）连接，不挂任何子设备。

        用于 GUI 把接收器本身作为设备显示：插着多联接收器但还没连子设备时，
        先按扫描到的 info（bus/usb_address）精确建立 dongle 通道，
        后续通过 dongle_scan/attach 管理子设备。
        返回已连接的 UsbComm（DONGLE 模式）。
        """
        if self._dongle is not None and self._dongle.is_connected:
            return self._dongle
        dongle = UsbComm()
        dongle.on_disconnected = lambda: self._on_dongle_removed()
        # 用扫描到的 info 精确打开 type=2 那个设备（避免多设备时 find 撞错）
        dongle.connect(info)
        self._dongle = dongle
        return dongle

    def connect_dongle_sub(self, mac: bytes) -> BaseDevice:
        """连接 dongle 上已挂载的子设备（幂等），返回 BaseDevice。

        由 GUI 在 dongle 报告子设备上线（GET_CONNECTED_DEVICE / 连接事件）时调用。
        已在池中则直接返回现有实例。
        """
        address = mac.hex(":")
        with self._lock:
            existing = self._devices.get(address)
        if existing is not None:
            return existing
        info = UsbDeviceInfo(
            address=address,
            name=f"sub-{address}",
            device_type=USB_DEVICE_TYPE_MULTI_DONGLE,
            via_dongle=True,
            extra={"mac": mac},
        )
        return self._connect_dongle_sub(info)

    def drop_dongle_sub(self, mac: bytes) -> None:
        """dongle 子设备掉线：清理池中实例并通知 GUI。

        子设备本就不在池中时不回调（GUI 无需处理）。
        """
        address = mac.hex(":")
        with self._lock:
            device = self._devices.pop(address, None)
        if device is None:
            return
        try:
            self._all_comms.remove(device.comm)
        except ValueError:
            pass
        if self.on_dongle_sub_disconnected:
            self.on_dongle_sub_disconnected(address)

    def connect(self, info: DeviceInfo) -> BaseDevice:
        """连接指定设备并返回 device 实例。

        USB 设备按 device_type（bcdDevice>>8）区分：
          1 = 单通道接收器，2 = 多联接收器 → 走 dongle 子设备路径
          3 = USB 直连 → 走直连路径
        """
        if info.transport == "ble":
            return self._connect_ble(info)
        if info.transport == "usb":
            # device_type: 1=单通道接收器, 2=多联接收器, 3=USB直连
            # 单通道接收器和直连一样走直连路径（直接私有协议通信）
            # 多联接收器走 dongle 子设备路径
            if info.device_type == USB_DEVICE_TYPE_MULTI_DONGLE:
                return self._connect_dongle_sub(info)
            return self._connect_usb_direct(info)
        raise ValueError(f"未知 transport: {info.transport}")

    def connect_ble_bonded(self, mac: str, device_type: int = DEVICE_TYPE_KEYBOARD,
                           uuid_set: str = "telink", name: str = "") -> BaseDevice:
        """连接 Windows 已配对已连接的 BLE 设备（复用系统 bonding）。

        适用场景：设备已和 Windows 配对连接（如 T8 键盘正在使用中），
        用 WinrtBleComm 走系统已建立的 GATT session，不需要重新扫描/配对。
        mac 来自 Windows 设备管理器 BTHLE\\DEV_xxx 条目。
        """
        from comm_lib.channels.winrt_ble_comm import WinrtBleComm

        comm = WinrtBleComm(uuid_set=uuid_set)
        comm.on_disconnected = lambda: self._on_device_disconnected(mac)
        comm.connect(mac)
        self._all_comms.append(comm)  # 持有引用防 GC
        info = DeviceInfo("ble", mac, name or "", device_type=device_type)
        device = self._make_device(comm, info)
        # 验证 BLE 协议通道真正可用：尝试一次 handshake/product_no 读取。
        # 若失败则断开连接并抛出异常，避免 GUI 出现“已连接但无信息”的状态。
        try:
            device.handshake()
        except Exception as e:
            try:
                comm.disconnect()
                self._all_comms.remove(comm)
            except Exception:
                pass
            raise CommError(f"BLE 协议握手失败，设备可能未就绪: {e}") from e
        self._register(device, mac)
        return device

    def _connect_ble(self, info: DeviceInfo) -> BaseDevice:
        from comm_lib.channels.ble_comm import BleComm

        comm = BleComm()
        comm.on_disconnected = lambda: self._on_device_disconnected(info.address)
        comm.connect(info.address)
        self._all_comms.append(comm)  # 持有引用防 GC
        device = self._make_device(comm, info)
        self._register(device, info.address)
        return device

    def _connect_usb_direct(self, info: DeviceInfo) -> Optional[BaseDevice]:
        comm = UsbComm()
        comm.on_disconnected = lambda: self._on_device_disconnected(info.address)
        comm.connect(info)
        self._all_comms.append(comm)  # 持有引用防 GC
        device = self._make_device(comm, info)

        # 单联接收器（device_type=1）：连上后验证是否真的有子设备
        # 读不到 ProductNo 说明接收器没连子设备，不显示
        if info.device_type == USB_DEVICE_TYPE_SINGLE_DONGLE:
            try:
                product_no = device.get_product_no()
                if not product_no:
                    comm.disconnect()
                    self._all_comms.remove(comm)
                    return None
                device.product_no = product_no
            except Exception:
                comm.disconnect()
                self._all_comms.remove(comm)
                return None

        self._register(device, info.address)
        return device

    def _connect_dongle_sub(self, info: UsbDeviceInfo) -> BaseDevice:
        mac = info.extra.get("mac")
        if mac is None:
            # info.address 是 mac 字符串
            mac = bytes(int(x, 16) for x in info.address.split(":"))
        # 已在池中（如 GET_CONNECTED_DEVICE 上报的旧设备）：直接复用，避免重复 attach
        with self._lock:
            existing = self._devices.get(info.address)
        if existing is not None:
            return existing
        # 首次连接 dongle
        if self._dongle is None or not self._dongle.is_connected:
            dongle = UsbComm()
            dongle.on_disconnected = lambda: self._on_dongle_removed()
            dongle.connect(UsbComm.Mode.DONGLE)
            self._dongle = dongle
        # dongle 已挂载该子设备（GET_CONNECTED_DEVICE 同步过）就不再重复发 attach
        if mac not in self._dongle._sub_macs:
            self._dongle.attach_subdevice(mac)
        adapter = _DongleSubCommAdapter(self._dongle, mac)
        adapter.on_disconnected = lambda: self._on_device_disconnected(info.address)
        adapter.install_frame_router()
        device = self._make_device(adapter, info)
        self._register(device, info.address)
        return device

    def _connect_single_dongle(self, info: UsbDeviceInfo) -> Optional[BaseDevice]:
        """连接单通道接收器（device_type=1）。

        单联接收器走 DIRECT 模式（直接私有协议通信，和 USB 直连一样）。
        连上后尝试读 ProductNo：
          - 读到 → 接收器已连子设备，创建设备实例
          - 读不到 → 接收器没连子设备，返回 None（不显示）
        """
        comm = UsbComm()
        comm.on_disconnected = lambda: self._on_device_disconnected(info.address)
        comm.connect(info)
        self._all_comms.append(comm)

        # 尝试读 ProductNo 确认设备可用（8 秒超时）
        try:
            device = self._make_device(comm, info)
            product_no = device.get_product_no(timeout=8.0)
            if not product_no:
                comm.disconnect()
                self._all_comms.remove(comm)
                return None
            device.product_no = product_no
            self._register(device, info.address)
            return device
        except Exception:
            try:
                comm.disconnect()
                self._all_comms.remove(comm)
            except Exception:
                pass
            return None

    def _make_device(self, comm: BaseComm, info: DeviceInfo) -> BaseDevice:
        """创建设备子类实例。

        优先按 info.product_no（4 位数字字符串）推断类型；
        缺失时回退到 device_type 字段（0x31=mouse / 0x32=keyboard）；
        都没有时默认按鼠标处理。
        同时根据 comm 类型设置 device.connection_type（区分 USB 直连/USB 接收器/BLE）。
        """
        name = info.name or ""
        address = info.address
        transport = info.transport or ""
        dtype = info.device_type
        # 区分连接方式：按 USB device_type 或 comm 类型判断
        if transport == "usb":
            dt = info.device_type
            if dt == USB_DEVICE_TYPE_MULTI_DONGLE:
                connection_type = "usb_dongle"
            elif dt == USB_DEVICE_TYPE_SINGLE_DONGLE:
                connection_type = "usb_single_dongle"
            elif dt == USB_DEVICE_TYPE_DIRECT:
                connection_type = "usb_direct"
            elif isinstance(comm, _DongleSubCommAdapter):
                # 扫描阶段没拿到 device_type 时回退：按 comm 类型判断
                connection_type = "usb_dongle"
            else:
                connection_type = "usb_direct"
        elif transport == "ble":
            connection_type = "ble"
        else:
            connection_type = transport or ""
        # 优先用 product_no 推断（比 device_type 更可靠：bcdDevice>>8 无法区分键鼠）
        if info.product_no:
            pn_str = self._product_no_str(info.product_no)
            dev_type = infer_device_type(pn_str)
            if dev_type == "keyboard":
                return KbDevice(comm, address=address, name=name, transport=transport, connection_type=connection_type)
            if dev_type == "mouse":
                return MouseDevice(comm, address=address, name=name, transport=transport, connection_type=connection_type)
        # 回退：用 device_type 字段
        if dtype == DEVICE_TYPE_KEYBOARD:
            return KbDevice(comm, address=address, name=name, transport=transport, connection_type=connection_type)
        # 默认按鼠标处理（最常见的 M 前缀 / 1xxx）
        return MouseDevice(comm, address=address, name=name, transport=transport, connection_type=connection_type)

    def _refine_device_type(self, device: BaseDevice) -> BaseDevice:
        """连上设备后读取真实 product_no，按配置切换实例类型。

        场景：扫描阶段拿不到 product_no，只能按默认类型（mouse）创建。
        连接握手后调本方法：读 product_no -> infer_device_type ->
        若当前实例类型与推断不符，重建为对应子类并迁移状态。

        返回新的设备实例（若切换则返回新实例，否则原样返回）。
        """
        try:
            product_no_bytes = device.get_product_no()
        except Exception:
            # 取不到 product_no，保留原实例（不阻塞使用）
            return device
        pn_str = self._product_no_str(product_no_bytes)
        cfg = get_device_config(pn_str)
        dev_type = infer_device_type(pn_str)

        # 当前实例类型与推断一致：补全 name（配置可能比扫描名更准）后原样返回
        if dev_type == "mouse" and isinstance(device, MouseDevice):
            if cfg and not device.name:
                device.name = cfg["name"]
            return device
        if dev_type == "keyboard" and isinstance(device, KbDevice):
            if cfg and not device.name:
                device.name = cfg["name"]
            return device
        if dev_type == "other":
            # other / special 暂不专门处理，保留原实例
            return device

        # 类型不符：重建为正确子类，迁移必要状态
        if dev_type == "keyboard":
            new_device = KbDevice(device.comm, address=device.address,
                                  name=cfg["name"] if cfg else device.name,
                                  transport=device.transport,
                                  connection_type=device.connection_type)
        else:
            new_device = MouseDevice(device.comm, address=device.address,
                                     name=cfg["name"] if cfg else device.name,
                                     transport=device.transport,
                                     connection_type=device.connection_type)
        # 迁移已缓存的状态（comm.on_frame 会被新实例构造函数重置到新实例）
        new_device.product_no = product_no_bytes
        new_device.device_mac = device.device_mac
        new_device.device_version = device.device_version
        # 更新设备池
        with self._lock:
            if device.address in self._devices and self._devices[device.address] is device:
                self._devices[device.address] = new_device
        return new_device

    @staticmethod
    def _product_no_str(product_no) -> str:
        """product_no 可能是 bytes（4 字节）或 str，统一转成 4 位数字字符串。"""
        if isinstance(product_no, (bytes, bytearray)):
            try:
                return bytes(product_no).decode("ascii", errors="ignore").strip("\x00")
            except Exception:
                return ""
        return str(product_no) if product_no else ""

    def _register(self, device: BaseDevice, address: str) -> None:
        with self._lock:
            self._devices[address] = device

    # ---- 断开（验收 3.3 末条：单独断开 / 全部断开） ----
    def disconnect(self, address: str) -> bool:
        with self._lock:
            device = self._devices.pop(address, None)
        if device is None:
            return False
        try:
            device.comm.disconnect()
        except Exception:
            pass
        # 从 comm 列表移除
        try:
            self._all_comms.remove(device.comm)
        except ValueError:
            pass
        return True

    def disconnect_all(self) -> None:
        with self._lock:
            addresses = list(self._devices.keys())
        for addr in addresses:
            self.disconnect(addr)
        # 释放 dongle
        if self._dongle is not None:
            try:
                self._dongle.disconnect()
            except Exception:
                pass
            self._dongle = None
        self._all_comms.clear()

    def _on_device_disconnected(self, address: str) -> None:
        """设备被动断开（拔出/掉线）时清理池。验收 3.4 末条：无句柄泄漏。"""
        with self._lock:
            device = self._devices.pop(address, None)
        if device is None:
            return
        try:
            # 底层已断开，仅清引用；dongle 子设备也走 detach
            if hasattr(device.comm, "disconnect"):
                device.comm.disconnect()
        except Exception:
            pass
        # 通知上层（GUI 自动刷新列表）；回调异常不影响清理流程
        if self.on_device_disconnected is not None:
            try:
                self.on_device_disconnected(address)
            except Exception:
                logger.debug("on_device_disconnected callback failed", exc_info=True)

    def _on_dongle_removed(self) -> None:
        """dongle 被拔出：池中所有 dongle 子设备一并清理。"""
        with self._lock:
            addrs = [a for a, d in self._devices.items() if isinstance(d.comm, _DongleSubCommAdapter)]
            for a in addrs:
                self._devices.pop(a, None)
        self._dongle = None
        if self.on_dongle_removed:
            self.on_dongle_removed()

    # ---- 设备池遍历（验收 3.3 末条） ----
    def devices(self) -> List[BaseDevice]:
        with self._lock:
            return list(self._devices.values())

    def get(self, address: str) -> Optional[BaseDevice]:
        with self._lock:
            return self._devices.get(address)

    def __iter__(self) -> Iterator[BaseDevice]:
        return iter(self.devices())

    def __len__(self) -> int:
        with self._lock:
            return len(self._devices)

    # ---- 资源释放 ----
    def close(self) -> None:
        self.disconnect_all()
