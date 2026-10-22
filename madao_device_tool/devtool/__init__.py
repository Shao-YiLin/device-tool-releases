"""devtool：上层业务入口。

组装通信层与设备模型，对外暴露简洁 API。
不直接导入 ble/usb 底层实现（验收 3.1 首条），仅通过 DeviceManager 间接使用。
支持鼠标（MouseDevice）和键盘（KbDevice）等全设备统一管理。
"""

from __future__ import annotations

from typing import List, Optional

from comm_lib.channels import DeviceInfo
from device_mgr import DeviceManager
from device_model import BaseDevice, MouseDevice


class DeviceTool:
    """上层业务入口。统一管理 BLE/USB 通道上的鼠标、键盘等设备。"""

    def __init__(self) -> None:
        self._mgr = DeviceManager()

    @property
    def manager(self) -> DeviceManager:
        return self._mgr

    # ---- 扫描 ----
    def scan_ble(self, timeout: float = 5.0) -> List[DeviceInfo]:
        return self._mgr.scan_ble(timeout)

    def scan_ble_bonded(self) -> List[DeviceInfo]:
        """列出 Windows 已配对已连接的 BLE 设备（非广播扫描）。"""
        return self._mgr.scan_ble_bonded()

    def scan_usb(self) -> List[DeviceInfo]:
        return self._mgr.scan_usb()

    # ---- 连接 ----
    def connect(self, info: DeviceInfo) -> Optional[BaseDevice]:
        """连接设备。单联接收器没连子设备时返回 None。"""
        return self._mgr.connect(info)

    def connect_dongle_only(self, info):
        """仅建立多联接收器连接（不挂子设备），返回 UsbComm。"""
        return self._mgr.connect_dongle_only(info)

    def disconnect(self, address: str) -> bool:
        return self._mgr.disconnect(address)

    def disconnect_all(self) -> None:
        self._mgr.disconnect_all()

    # ---- 设备池 ----
    def devices(self) -> List[BaseDevice]:
        return self._mgr.devices()

    def get_device(self, address: str) -> Optional[BaseDevice]:
        return self._mgr.get(address)

    def close(self) -> None:
        self._mgr.close()

    def __len__(self) -> int:
        return len(self._mgr)

    def __iter__(self):
        return iter(self._mgr)

    # ---- 便捷：按 mac 精准连接鼠标并返回 MouseDevice（验收 3.4 第3条） ----
    def connect_mouse_by_mac(self, mac: str, transport: str = "ble") -> MouseDevice:
        if transport == "ble":
            infos = self.scan_ble()
        else:
            infos = self.scan_usb()
        for info in infos:
            if info.address.lower().replace(":", "") == mac.lower().replace(":", ""):
                dev = self.connect(info)
                if isinstance(dev, MouseDevice):
                    return dev
                raise TypeError(f"设备 {mac} 不是鼠标")
        raise LookupError(f"未找到 mac={mac} 的设备")

    def connect_ble_bonded(self, mac: str, device_type: int = 0x32, name: str = "") -> BaseDevice:
        """连接 Windows 已配对的 BLE 设备（复用系统 bonding，无需重新扫描）。

        默认 device_type=0x32（键盘）；连鼠标传 device_type=0x31。
        """
        return self._mgr.connect_ble_bonded(mac, device_type=device_type, name=name)
