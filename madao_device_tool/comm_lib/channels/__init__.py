"""comm_lib.channels 子包入口。"""

from .base import BaseComm, CommError, DeviceInfo

__all__ = ["BaseComm", "CommError", "DeviceInfo"]


def __getattr__(name):
    # 延迟导入，避免无 bleak/winrt 环境下 import comm_lib 报错
    if name == "BleComm":
        from .ble_comm import BleComm
        return BleComm
    if name == "WinrtBleComm":
        from .winrt_ble_comm import WinrtBleComm
        return WinrtBleComm
    if name == "UsbComm":
        from .usb_comm import UsbComm
        return UsbComm
    raise AttributeError(f"module {__name__!r} has no attribute {name!r}")
