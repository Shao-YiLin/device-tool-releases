# -*- coding: utf-8 -*-
"""从机版本号接口测试（AM60 鼠标 / AT80 键盘）。

覆盖：
1. MouseDevice.get_slave_device_version：GET_SLAVE_PROP[0x1F] 请求-应答解析
2. KbDevice.get_slave_device_version：同一接口对键盘（AT80）可用
3. 应答长度非法时抛 DeviceError
4. GUI：键盘面板有「AT80 专属」从机版本号读取按钮，鼠标面板组名改为「AM60 专属」
5. device_config：1042 名称改为 AM60

运行：python madao_device_tool/test/test_slave_version.py
"""
import os
import sys
import threading

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from comm_lib.channels import BaseComm
from comm_lib.protocol.cmd import Command, ErrCode
from comm_lib.protocol.frame import ProtocolFrame, u32_to_bytes
from comm_lib.protocol.prop_key import PropKey
from device_model import DeviceError, KbDevice, MouseDevice


class MockComm(BaseComm):
    """内存通道：暂存发送的帧，测试代码主动注入 ACK。"""

    def __init__(self):
        super().__init__()
        self.sent_frames = []
        self._connected = True

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

    def inject_frame(self, cmd: int, sequence: int, payload: bytes):
        self.on_frame(ProtocolFrame(0xFE, sequence, cmd, payload))


def _last_sent_frame(comm: MockComm) -> ProtocolFrame:
    from comm_lib.protocol.frame import parse_frames
    frames = parse_frames(comm.sent_frames[-1])
    assert frames, "no frame sent"
    return frames[0]


def _slave_record(dev_id: int, ver_bytes: bytes) -> bytes:
    """构造一条从机记录：2 字节设备号 + 4 字节版本号（小端）。"""
    return dev_id.to_bytes(2, "little") + ver_bytes


def _respond(comm: MockComm, data: bytes, err: int = ErrCode.SUCCESS):
    def _resp():
        f = _last_sent_frame(comm)
        comm.inject_frame(Command.ACK, f.sequence, u32_to_bytes(err) + data)
    threading.Timer(0.05, _resp).start()


# ---------------------------------------------------------------------------
# 设备模型
# ---------------------------------------------------------------------------

def test_slave_version_mouse_roundtrip():
    """AM60 鼠标：应答 n*(2 字节设备号 + 4 字节版本号) 解析为 [(device_id, version)]。"""
    comm = MockComm()
    dev = MouseDevice(comm)
    data = _slave_record(2, bytes([0x01, 0x00, 0x00, 0x0D])) + \
        _slave_record(3, bytes([0x02, 0x00, 0x00, 0x00]))
    _respond(comm, data)

    assert dev.get_slave_device_version() == [(2, 0x0D000001), (3, 2)]

    req = _last_sent_frame(comm)
    assert req.cmd == Command.GET_SLAVE_PROP
    assert req.payload == bytes([PropKey.SLAVE_DEVICE_VERSION])


def test_slave_version_keyboard_roundtrip():
    """AT80 键盘同样支持获取从机版本号（接口定义在 BaseDevice）。"""
    comm = MockComm()
    kb = KbDevice(comm)
    _respond(comm, _slave_record(1, bytes([0x05, 0x00, 0x00, 0x00])))

    assert kb.get_slave_device_version() == [(1, 5)]


def test_slave_version_bad_length_raises():
    """应答长度不是 6 的倍数：抛 DeviceError（附带原始数据便于定位）。"""
    comm = MockComm()
    dev = MouseDevice(comm)
    _respond(comm, _slave_record(2, bytes([0x01, 0x00, 0x00, 0x0D])) + b"\x07")

    try:
        dev.get_slave_device_version()
        assert False, "长度异常时应抛 DeviceError"
    except DeviceError as e:
        assert "长度异常" in str(e)


def test_slave_version_err_code_raises():
    """设备返回错误码：抛 DeviceError。"""
    comm = MockComm()
    dev = MouseDevice(comm)
    _respond(comm, b"", err=ErrCode.FAIL)

    try:
        dev.get_slave_device_version()
        assert False, "错误码时应抛 DeviceError"
    except DeviceError as e:
        assert "从机版本号" in str(e)


# ---------------------------------------------------------------------------
# 配置改名
# ---------------------------------------------------------------------------

def test_device_config_am60_renamed():
    """1042 设备名由 M610 Ultra 改为 AM60。"""
    from device_config import get_device_config
    cfg = get_device_config("1042")
    assert cfg is not None
    assert cfg["name"] == "AM60"
    assert cfg["type"] == "mouse"


# ---------------------------------------------------------------------------
# GUI
# ---------------------------------------------------------------------------

def _gui_env():
    os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
    gui_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "gui")
    if gui_dir not in sys.path:
        sys.path.insert(0, gui_dir)


def test_keyboard_panel_has_at80_slave_version():
    """键盘面板：存在「AT80 专属」分组与从机版本号读取按钮。"""
    try:
        from PySide6.QtWidgets import QApplication
    except ImportError:
        print("SKIP: PySide6 未安装，跳过 GUI 用例")
        return
    _gui_env()
    app = QApplication.instance() or QApplication([])

    from keyboard_panel import KeyboardPanel

    class _StubWorker:
        def async_op(self, op_id, address, method_name, *args):
            pass

    dev = KbDevice(MockComm(), address="04:aa", name="AT80")
    panel = KeyboardPanel(dev, _StubWorker())
    try:
        assert panel._at80_group.title() == "AT80 专属"
        assert panel.btn_slave_ver_read.text() == "读取"
        # 触发读取并回填结果，校验标签内容
        panel._read_slave_device_version()
        panel.handle_result(f"{dev.address}:get_slave_device_version", [(2, 0x0D000001)])
        assert "1.0.0.13" in panel.lbl_slave_version.text(), panel.lbl_slave_version.text()
    finally:
        panel.close()


def test_mouse_panel_group_renamed_to_am60():
    """鼠标面板：专属分组名由「M610 Ultra 专属」改为「AM60 专属」。"""
    try:
        from PySide6.QtWidgets import QApplication
    except ImportError:
        print("SKIP: PySide6 未安装，跳过 GUI 用例")
        return
    _gui_env()
    app = QApplication.instance() or QApplication([])

    from mouse_panel import MousePanel

    class _StubWorker:
        def async_op(self, op_id, address, method_name, *args):
            pass

    dev = MouseDevice(MockComm(), address="04:bb", name="AM60")
    panel = MousePanel(dev, _StubWorker())
    try:
        assert panel._am60_group.title() == "AM60 专属"
    finally:
        panel.close()


def _run_all():
    fns = [v for k, v in sorted(globals().items()) if k.startswith("test_")]
    failed = 0
    for fn in fns:
        try:
            fn()
            print(f"PASS: {fn.__name__}")
        except AssertionError as e:
            failed += 1
            print(f"FAIL: {fn.__name__}: {e}")
        except Exception as e:
            failed += 1
            print(f"ERROR: {fn.__name__}: {type(e).__name__}: {e}")
    print(f"\n{'=' * 40}\n通过: {len(fns) - failed}, 失败: {failed}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(_run_all())
