"""KbDevice 键盘专属功能单元测试。

用 MockComm 验证 RGB/按键自定义/滚轮功能的指令构造和应答解析。
不依赖真实硬件。

运行：python madao_device_tool/test/test_keyboard.py
"""

import os
import sys
import threading

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from comm_lib.channels import BaseComm
from comm_lib.protocol.cmd import Command, ErrCode, EventType
from comm_lib.protocol.frame import (
    ProtocolFrame,
    u32_to_bytes,
)
from comm_lib.protocol.prop_key import PropKey
from device_model import KbDevice
from device_model.keyboard import RGB_MODE, RGB_BRIGHTNESS, RGB_SPEED


class MockComm(BaseComm):
    def __init__(self):
        super().__init__()
        self.sent_frames = []
        self._connected = True

    def scan(self): pass
    def connect(self, target): self._connected = True
    def disconnect(self): self._connected = False
    def send_raw(self, frame):
        self.sent_frames.append(frame)
        return len(frame)
    @property
    def is_connected(self): return self._connected

    def inject_frame(self, cmd, sequence, payload):
        self.on_frame(ProtocolFrame(0xFE, sequence, cmd, payload))

    def inject_event(self, event_type, value):
        # EVENT 帧 payload = [type][value(4 LE)]
        self.on_frame(ProtocolFrame(0xFE, 0, Command.EVENT, bytes([event_type]) + u32_to_bytes(value)))


def _last_frame(comm):
    from comm_lib.protocol.frame import parse_frames
    frames = parse_frames(comm.sent_frames[-1])
    return frames[0]


def test_get_rgb_mode():
    comm = MockComm()
    kb = KbDevice(comm)
    def _resp():
        f = _last_frame(comm)
        # 设备用 EVENT 上报: value = (mode<<8) + RGB_MODE
        comm.inject_event(EventType.RGB_STATUS, (5 << 8) + RGB_MODE)
    threading.Timer(0.05, _resp).start()
    assert kb.get_rgb_mode() == 5
    # 校验请求帧
    req = _last_frame(comm)
    assert req.cmd == Command.GET_SLAVE_PROP
    assert req.payload == bytes([PropKey.BACKLIGHT, RGB_MODE])


def test_get_rgb_brightness():
    comm = MockComm()
    kb = KbDevice(comm)
    def _resp():
        f = _last_frame(comm)
        comm.inject_event(EventType.RGB_STATUS, (2 << 8) + RGB_BRIGHTNESS)
    threading.Timer(0.05, _resp).start()
    assert kb.get_rgb_brightness() == 2


def test_get_rgb_speed():
    comm = MockComm()
    kb = KbDevice(comm)
    def _resp():
        comm.inject_event(EventType.RGB_STATUS, (1 << 8) + RGB_SPEED)
    threading.Timer(0.05, _resp).start()
    assert kb.get_rgb_speed() == 1


def test_set_rgb_mode():
    comm = MockComm()
    kb = KbDevice(comm)
    def _resp():
        f = _last_frame(comm)
        comm.inject_frame(Command.ACK, f.sequence, u32_to_bytes(ErrCode.SUCCESS))
    threading.Timer(0.05, _resp).start()
    kb.set_rgb_mode(3)
    req = _last_frame(comm)
    assert req.cmd == Command.SET_SLAVE_PROP
    assert req.payload == bytes([PropKey.BACKLIGHT, RGB_MODE, 3])


def test_set_rgb_brightness():
    comm = MockComm()
    kb = KbDevice(comm)
    def _resp():
        f = _last_frame(comm)
        comm.inject_frame(Command.ACK, f.sequence, u32_to_bytes(ErrCode.SUCCESS))
    threading.Timer(0.05, _resp).start()
    kb.set_rgb_brightness(2)
    req = _last_frame(comm)
    assert req.payload == bytes([PropKey.BACKLIGHT, RGB_BRIGHTNESS, 2])


def test_get_custom_button():
    comm = MockComm()
    kb = KbDevice(comm)
    # 设备应答 ACK: err + [flag][type][data(8)]
    def _resp():
        f = _last_frame(comm)
        data = bytes([1, 0]) + b"\x05" * 8  # flag=1, type=0, data=05*8
        comm.inject_frame(Command.ACK, f.sequence, u32_to_bytes(ErrCode.SUCCESS) + data)
    threading.Timer(0.05, _resp).start()
    flag, ktype, data = kb.get_custom_button(1)
    assert flag == 1
    assert ktype == 0
    assert data == b"\x05" * 8
    req = _last_frame(comm)
    assert req.payload == bytes([PropKey.CUSTOM_KEY, 1])


def test_set_custom_button():
    comm = MockComm()
    kb = KbDevice(comm)
    def _resp():
        f = _last_frame(comm)
        comm.inject_frame(Command.ACK, f.sequence, u32_to_bytes(ErrCode.SUCCESS))
    threading.Timer(0.05, _resp).start()
    kb.set_custom_button(2, 1, 0, b"\x01\x02")
    req = _last_frame(comm)
    # payload = [CUSTOM_KEY][key_index][flag][type][data(8)]
    assert req.payload == bytes([PropKey.CUSTOM_KEY, 2, 1, 0]) + b"\x01\x02" + b"\x00" * 6


def test_get_wheel_function():
    comm = MockComm()
    kb = KbDevice(comm)
    def _resp():
        f = _last_frame(comm)
        comm.inject_frame(Command.ACK, f.sequence, u32_to_bytes(ErrCode.SUCCESS) + u32_to_bytes(3))
    threading.Timer(0.05, _resp).start()
    assert kb.get_wheel_function() == 3


def test_set_wheel_function():
    comm = MockComm()
    kb = KbDevice(comm)
    def _resp():
        f = _last_frame(comm)
        comm.inject_frame(Command.ACK, f.sequence, u32_to_bytes(ErrCode.SUCCESS))
    threading.Timer(0.05, _resp).start()
    kb.set_wheel_function(1)
    req = _last_frame(comm)
    assert req.cmd == Command.SET_SLAVE_PROP
    assert req.payload == bytes([0x20]) + u32_to_bytes(1)


def test_kb_inherits_auth_and_ota():
    # 键盘继承基类的鉴权和 OTA 能力
    comm = MockComm()
    kb = KbDevice(comm)
    assert hasattr(kb, "authenticate")
    assert hasattr(kb, "perform_ota")
    assert hasattr(kb, "get_battery")


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
