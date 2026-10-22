"""comm_lib 包：BLE/USB 双通道私有通信库。

分层：
- comm_lib.protocol：私有协议帧打包/解包/校验，与硬件通道解耦
- comm_lib.channels：BaseComm 抽象基类 + BleComm / UsbComm 实现
"""

from .protocol.frame import (
    COMMAND_MAGIC,
    ProtocolFrame,
    build_frame,
    parse_frames,
)
from .protocol import cmd as cmd
from .protocol import prop_key as prop_key

__all__ = [
    "COMMAND_MAGIC",
    "ProtocolFrame",
    "build_frame",
    "parse_frames",
    "cmd",
    "prop_key",
]
