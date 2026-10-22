"""键盘设备模型。

对齐 src_archive/T8_light/source/vendor/8258_dual_kb/logic_protocol_manager.c。
T8 键盘专属功能：
  - get_rgb_mode / set_rgb_mode        灯效模式（0~17）
  - get_rgb_brightness / set_rgb_brightness  灯光亮度（0/1/2/3，设备端映射到 0/85/170/255）
  - get_rgb_speed / set_rgb_speed      灯效速度（0~3）
  - get_custom_button / set_custom_button  按键自定义
  - get_wheel_function / set_wheel_function  滚轮功能

公共能力（基础信息、录音、OTA、鉴权）继承自 BaseDevice，无需重复实现。
"""

from __future__ import annotations

from typing import Tuple

from comm_lib.protocol.cmd import Command, ErrCode, EventType
from comm_lib.protocol.frame import bytes_to_u32, u32_to_bytes
from comm_lib.protocol.prop_key import PropKey

from .base import BaseDevice, DeviceError

# T8 RGB 子类型（对齐 T8_light protocol_define.h::PROTOCOL_RGB_STATUS）
RGB_MODE = 0x00
RGB_BRIGHTNESS = 0x01
RGB_SPEED = 0x02


class KbDevice(BaseDevice):
    """键盘设备。"""

    DEVICE_TYPE = 0x32  # 对齐 lib_device_define.h::LIBMOUSE_DEVICE_TYPE_KEYBOARD

    def __init__(self, comm, address: str = "", name: str = "",
                 transport: str = "", connection_type: str = "") -> None:
        super().__init__(comm, address=address, name=name,
                         transport=transport, connection_type=connection_type)

    # ---- RGB 灯光 ----
    # T8 键盘的 RGB 属性通过 EVENT(TYPE_RGB_STATUS) 应答（见 T8 logic_protocol_manager.c::_prop_resp）。
    # EVENT payload = [type][value(4 LE)]，value 高字节是数据、低字节是子类型。

    def _get_rgb(self, sub_type: int) -> int:
        """GET_SLAVE_PROP [RGB_STATUS][sub_type]，等待 EVENT(TYPE_RGB_STATUS) 取值。"""
        import threading
        sequence = self._seq.next()
        # 挂 EVENT 等待
        ev = threading.Event()
        waiter = [ev, None]
        with self._lock:
            self._event_waiters[EventType.RGB_STATUS] = waiter
        try:
            self._send_frame(Command.GET_SLAVE_PROP, bytes([PropKey.BACKLIGHT, sub_type]), sequence)
        except Exception as e:
            with self._lock:
                self._event_waiters.pop(EventType.RGB_STATUS, None)
            raise DeviceError(f"发送失败: {e}") from e
        if not ev.wait(timeout=3.0):
            with self._lock:
                self._event_waiters.pop(EventType.RGB_STATUS, None)
            raise DeviceError(f"等待 RGB EVENT 超时 sub_type={sub_type}")
        # waiter[1] 是 EVENT 的 value，高字节是数据
        return (waiter[1] >> 8) if waiter[1] else 0

    def _set_rgb(self, sub_type: int, param: int) -> int:
        """SET_SLAVE_PROP [RGB_STATUS][sub_type][param]。"""
        err, _ = self.send_and_wait(
            Command.SET_SLAVE_PROP, bytes([PropKey.BACKLIGHT, sub_type, param & 0xFF])
        )
        if err != ErrCode.SUCCESS:
            raise DeviceError(f"设置 RGB sub_type={sub_type} 失败 err={err}")
        return err

    def get_rgb_mode(self) -> int:
        """获取当前灯效模式（0~17）。"""
        return self._get_rgb(RGB_MODE)

    def set_rgb_mode(self, mode: int) -> int:
        """设置灯效模式。"""
        return self._set_rgb(RGB_MODE, mode)

    def get_rgb_brightness(self) -> int:
        """获取灯光亮度等级（0/1/2/3）。"""
        return self._get_rgb(RGB_BRIGHTNESS)

    def set_rgb_brightness(self, level: int) -> int:
        """设置灯光亮度等级。"""
        return self._set_rgb(RGB_BRIGHTNESS, level)

    def get_rgb_speed(self) -> int:
        """获取灯效速度（0~3）。"""
        return self._get_rgb(RGB_SPEED)

    def set_rgb_speed(self, speed: int) -> int:
        """设置灯效速度。"""
        return self._set_rgb(RGB_SPEED, speed)

    # ---- 按键自定义 ----
    # 对齐 T8 logic_protocol_manager.c 的 CUSTOM_BUTTON 分支：
    #   GET: GET_SLAVE_PROP [CUSTOM_BUTTON][key_index] -> ACK(err, [flag][type][data(8B)])
    #   SET: SET_SLAVE_PROP [CUSTOM_BUTTON][key_index][flag][type][data(8B)]  (11B payload after key)

    def get_custom_button(self, key_index: int) -> Tuple[int, int, bytes]:
        """获取指定按键的自定义配置。

        返回 (flag, key_type, data(8字节))。
        flag=1 标准按键，2 自定义按键；key_type 0=normal，1=consume。
        """
        err, data = self._get_slave_prop_with_payload(
            bytes([PropKey.CUSTOM_KEY, key_index & 0xFF])
        )
        if err != ErrCode.SUCCESS or len(data) < 10:
            raise DeviceError(f"获取 custom_button key={key_index} 失败 err={err}")
        return data[0], data[1], bytes(data[2:10])

    def set_custom_button(self, key_index: int, flag: int, key_type: int, data: bytes) -> int:
        """设置按键自定义。

        data 是 8 字节的键盘协议数据（normal key 模式）或 2 字节（consume key 模式，补齐到8）。
        """
        if len(data) > 8:
            data = data[:8]
        data = data + b"\x00" * (8 - len(data))
        payload = bytes([PropKey.CUSTOM_KEY, key_index & 0xFF, flag & 0xFF, key_type & 0xFF]) + data
        err, _ = self.send_and_wait(Command.SET_SLAVE_PROP, payload)
        if err != ErrCode.SUCCESS:
            raise DeviceError(f"设置 custom_button key={key_index} 失败 err={err}")
        return err

    # ---- 滚轮功能 ----
    # 对齐 T8 logic_protocol_manager.c 的 WHEEL_FUNCTION 分支：
    #   GET: GET_SLAVE_PROP [WHEEL_FUNCTION] -> ACK(err, u32)
    #   SET: SET_SLAVE_PROP [WHEEL_FUNCTION][value(4 LE)]
    # 注：T8 prop_key.h 中 WHEEL_FUNCTION=0x20，对应 prop_key.SET_DEVICE_ACTION？需要确认。
    # T8 protocol_define.h: WHEEL_FUNCTION = 0x20

    def get_wheel_function(self) -> int:
        """获取滚轮功能模式。"""
        # WHEEL_FUNCTION=0x20 在 prop_key.py 没有直接对应，用字面值
        err, data = self._get_slave_prop(0x20)
        if err != ErrCode.SUCCESS or len(data) < 4:
            raise DeviceError(f"获取 wheel_function 失败 err={err}")
        return bytes_to_u32(data[:4])

    def set_wheel_function(self, value: int) -> int:
        """设置滚轮功能模式。"""
        err, _ = self._set_slave_prop(0x20, u32_to_bytes(value))
        if err != ErrCode.SUCCESS:
            raise DeviceError(f"设置 wheel_function 失败 err={err}")
        return err

    # ---- 辅助 ----
    def _get_slave_prop_with_payload(self, payload: bytes, timeout: float = 3.0) -> Tuple[int, bytes]:
        """GET_SLAVE_PROP 带多字节 payload（如 CUSTOM_BUTTON 需要 [key][index]）。"""
        return self.send_and_wait(Command.GET_SLAVE_PROP, payload, timeout)
