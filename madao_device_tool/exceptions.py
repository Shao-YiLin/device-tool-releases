"""设备工具统一异常体系。

所有设备工具相关异常的根：DeviceToolError。
子类按错误来源分类。现有 CommError / DeviceError / OtaError 继承自本体系，
保持原有 except 语义（except CommError 仍捕获通信层异常）。

注意：OtaError 定义在 device_model/ota.py（历史位置），已改为继承 DeviceToolError，
此处不再重复定义，避免出现两个 OtaError 类造成 isinstance 歧义。
"""

from __future__ import annotations


class DeviceToolError(Exception):
    """设备工具所有异常的基类。"""


class DeviceConnectionError(DeviceToolError):
    """连接相关错误（建立/断开/重连失败）。"""


class ProtocolError(DeviceToolError):
    """协议解析错误（帧格式、CRC、未知命令等）。"""


class AuthenticationError(DeviceToolError):
    """鉴权失败（phase 校验失败、指纹不匹配、超时等）。"""


class DeviceTimeoutError(DeviceToolError):
    """操作超时（等待 ACK / EVENT / 鉴权 phase 超时等）。"""
