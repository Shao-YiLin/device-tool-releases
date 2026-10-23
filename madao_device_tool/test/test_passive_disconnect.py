# -*- coding: utf-8 -*-
"""设备被动断开自动刷新回归测试。

覆盖：
1. DeviceManager：设备被动断开（拔出/掉线）时触发 on_device_disconnected 回调
2. DeviceManager：未知地址断开不误触发回调
3. GUI：主窗口收到被动断开后自动刷新设备列表（无需手动"刷新设备池"）
4. GUI：「刷新设备池」按钮已删除

运行：python madao_device_tool/test/test_passive_disconnect.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from unittest.mock import MagicMock

from device_mgr import DeviceManager
from device_model import MouseDevice


# ---------------------------------------------------------------------------
# 1. 管理层：被动断开触发回调
# ---------------------------------------------------------------------------

def test_passive_disconnect_fires_callback():
    """设备在池中时被动断开：清池 + 触发 on_device_disconnected。"""
    mgr = DeviceManager()
    dev = MouseDevice(MagicMock(), address="04:aa", name="t",
                      transport="usb", connection_type="usb_direct")
    mgr._register(dev, "04:aa")
    got = []
    mgr.on_device_disconnected = got.append

    mgr._on_device_disconnected("04:aa")

    assert got == ["04:aa"], "应触发一次被动断开回调"
    assert mgr.get("04:aa") is None, "设备应从池中移除"


def test_passive_disconnect_unknown_address_silent():
    """未知地址断开（重复通知/竞态）：不触发回调。"""
    mgr = DeviceManager()
    got = []
    mgr.on_device_disconnected = got.append

    mgr._on_device_disconnected("unknown")

    assert got == [], "未知地址不应触发回调"


def test_passive_disconnect_callback_exception_swallowed():
    """回调抛异常不影响设备池清理。"""
    mgr = DeviceManager()
    dev = MouseDevice(MagicMock(), address="04:bb", name="t",
                      transport="usb", connection_type="usb_direct")
    mgr._register(dev, "04:bb")

    def boom(_address):
        raise RuntimeError("callback error")

    mgr.on_device_disconnected = boom
    mgr._on_device_disconnected("04:bb")  # 不应抛异常
    assert mgr.get("04:bb") is None


# ---------------------------------------------------------------------------
# 2. GUI 层：主窗口自动刷新（需要 PySide6，缺失则跳过）
# ---------------------------------------------------------------------------

def test_main_window_auto_refresh_on_passive_disconnect():
    os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
    try:
        from PySide6.QtWidgets import QApplication
    except ImportError:
        print("SKIP: PySide6 未安装，跳过 GUI 用例")
        return

    # main_window 及其面板是 gui 目录内的平铺绝对导入，需把 gui 目录入 path
    gui_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "gui")
    if gui_dir not in sys.path:
        sys.path.insert(0, gui_dir)

    app = QApplication.instance() or QApplication([])
    from main_window import MainWindow

    class FakeDev:
        def __init__(s, address, name):
            s.address = address
            s.name = name
            s.transport = "usb"
            s.connection_type = "usb_direct"

    w = MainWindow()
    try:
        dev_a = FakeDev("04:aa", "设备A")
        dev_b = FakeDev("04:bb", "设备B")
        w._connected[dev_a.address] = dev_a
        w._connected[dev_b.address] = dev_b
        w._tool._mgr._register(MagicMock(), dev_a.address)
        w._tool._mgr._register(MagicMock(), dev_b.address)
        w._rebuild_list()
        assert w.lst_devices.count() == 2

        # 模拟底层线程触发被动断开（DeviceManager 清池并回调 GUI）
        w._tool._mgr._on_device_disconnected(dev_a.address)
        app.processEvents()  # QTimer.singleShot(0) 需要事件循环分发

        assert dev_a.address not in w._connected, "断开的设备应从窗口列表移除"
        assert dev_b.address in w._connected, "其他设备应保留"
        names = [w.lst_devices.item(i).data(0x0100)  # Qt.UserRole
                 for i in range(w.lst_devices.count())]
        assert dev_a.address not in names, "设备列表应自动刷新"
        assert dev_b.address in names

        # 「刷新设备池」按钮已删除
        assert not hasattr(w, "btn_refresh"), "刷新设备池按钮应已删除"
    finally:
        w.close()


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
