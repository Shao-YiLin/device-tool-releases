"""设备模型层。

- BaseDevice：全设备通用能力（基础信息、录音、OTA 框架）
- MouseDevice：鼠标专属（CPI、回报率）
- KbDevice：键盘子类（占位，差异化功能后续扩展）

设备模型只依赖 comm_lib.BaseComm，不感知 BLE/USB 底层实现（验收 3.1 首条）。
"""

from .base import BaseDevice, DeviceError
from .mouse import MouseDevice
from .keyboard import KbDevice

__all__ = ["BaseDevice", "DeviceError", "MouseDevice", "KbDevice"]
