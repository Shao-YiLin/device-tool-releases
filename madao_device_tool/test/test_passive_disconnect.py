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
        app.processEvents()  # Signal 需要事件循环分发到主线程

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


def test_dongle_sub_disconnect_event_notifies():
    """dongle 上报子设备掉线事件（0x02）：从 _sub_macs 移除并触发回调。"""
    from comm_lib.channels.usb_comm import UsbComm

    comm = UsbComm()
    mac = bytes.fromhex("aabbccddeeff")
    comm._sub_macs = [mac]
    got = []
    comm.on_subdevice_disconnected = got.append

    comm._dispatch_dongle_event(0x02, 0x00, mac, b"")

    assert mac not in comm._sub_macs
    assert got == [mac], "子设备掉线应触发 on_subdevice_disconnected"


def test_device_mgr_wires_sub_disconnect_to_drop():
    """connect_dongle_only 建立的 dongle 应把子设备掉线接到 drop_dongle_sub。"""
    from unittest.mock import patch

    mgr = DeviceManager()
    with patch("device_mgr.UsbComm") as _:
        info = type("I", (), {
            "transport": "usb", "address": "1:2", "name": "",
            "product_no": None, "device_type": 2, "extra": {},
        })()
        mgr.connect_dongle_only(info)

    assert mgr._dongle is not None
    assert mgr._dongle.on_subdevice_disconnected == mgr.drop_dongle_sub, \
        "dongle 的子设备掉线回调应指向 drop_dongle_sub"


def test_winrt_watchdog_detects_ble_disconnect():
    """WinrtBleComm 看门狗：先见过 Connected，之后连续判定 Disconnected 才通知。"""
    try:
        from comm_lib.channels.winrt_ble_comm import WinrtBleComm
    except Exception as e:
        print(f"SKIP: winrt 后端不可用（{e}），跳过")
        return

    comm = WinrtBleComm()
    fired = []
    # BaseComm.on_disconnected 是无参回调（设备层用 lambda 包装地址）
    comm.on_disconnected = lambda: fired.append(1)

    # 先观察到 Connected（BluetoothConnectionStatus.Connected = 0）
    comm._device = MagicMock()
    comm._device.connection_status = 0
    assert comm._check_connection_alive() is True
    assert fired == []

    # 之后变 Disconnected（= 1）：需连续两次才判定掉线
    comm._device.connection_status = 1
    assert comm._check_connection_alive() is True, "单次抖动不应判定掉线"
    assert fired == []
    assert comm._check_connection_alive() is False, "连续判定掉线后看门狗应退出"
    assert len(fired) == 1, "掉线应触发一次断开通知"

    # 设备已被主动关闭（_device=None）：静默退出，不误触发
    fired.clear()
    comm._device = None
    assert comm._check_connection_alive() is False
    assert fired == []


def test_winrt_watchdog_ignores_disconnected_without_connected():
    """CACHED 模式下 connection_status 常年 Disconnected：从未见过 Connected 时不误判掉线。"""
    try:
        from comm_lib.channels.winrt_ble_comm import WinrtBleComm
    except Exception as e:
        print(f"SKIP: winrt 后端不可用（{e}），跳过")
        return

    comm = WinrtBleComm()
    fired = []
    comm.on_disconnected = lambda: fired.append(1)

    comm._device = MagicMock()
    comm._device.connection_status = 1  # Disconnected，但从未见过 Connected
    for _ in range(5):
        assert comm._check_connection_alive() is True, "状态不可靠时不应判定掉线"
    assert fired == [], "未见过 Connected 时不应触发断开通知"


def test_usb_rx_pipe_error_keeps_connection():
    """USB 瞬时错误（PIPE=-9）：不能拆掉连接（否则 OTA 途中会判为"未连接"）。"""
    from comm_lib.channels.usb_comm import UsbComm

    comm = UsbComm()
    fired = []
    comm.on_disconnected = lambda: fired.append(1)

    class _FakeDev:
        def __init__(self):
            self.calls = 0

        def read(self, ep, size, timeout=None):
            self.calls += 1
            if self.calls == 1:
                raise _usb_error(32, -9, "Pipe error")
            comm._running.clear()  # 第二次读时退出循环
            raise _usb_timeout()

        def clear_halt(self, ep):
            self.cleared = True

    comm._dev = _FakeDev()
    comm._mode = UsbComm.Mode.DIRECT
    comm._running.set()
    comm._rx_loop()

    assert fired == [], "Pipe error 不应触发断开通知"
    assert comm.is_connected, "Pipe error 后连接应仍然有效"


def test_usb_rx_no_device_triggers_disconnect():
    """USB 设备真的不在了（errno=19 / backend_code=-4）：触发断开通知。"""
    from comm_lib.channels.usb_comm import UsbComm

    comm = UsbComm()
    fired = []
    comm.on_disconnected = lambda: fired.append(1)

    class _FakeDev:
        def read(self, ep, size, timeout=None):
            raise _usb_error(19, -4, "No such device")

    comm._dev = _FakeDev()
    comm._mode = UsbComm.Mode.DIRECT
    comm._running.set()
    comm._rx_loop()

    assert len(fired) == 1, "设备拔出应触发一次断开通知"


def _usb_error(errno_, backend_code, msg):
    import usb.core
    # pyusb: USBError(strerror, error_code->backend_error_code, errno)
    return usb.core.USBError(msg, backend_code, errno_)


def _usb_timeout():
    import usb.core
    return usb.core.USBTimeoutError("timeout")


def test_gui_refresh_via_comm_notify_chain():
    """端到端：comm._notify_disconnected -> mgr 清池 -> GUI 自动刷新。"""
    os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
    try:
        from PySide6.QtWidgets import QApplication
    except ImportError:
        print("SKIP: PySide6 未安装，跳过 GUI 用例")
        return

    gui_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "gui")
    if gui_dir not in sys.path:
        sys.path.insert(0, gui_dir)

    app = QApplication.instance() or QApplication([])
    from main_window import MainWindow
    from comm_lib.channels.usb_comm import UsbComm

    class FakeDev:
        def __init__(s, address, name):
            s.address = address
            s.name = name
            s.transport = "usb"
            s.connection_type = "usb_direct"

    w = MainWindow()
    try:
        dev = FakeDev("04:cc", "设备C")
        w._connected[dev.address] = dev
        w._rebuild_list()
        assert w.lst_devices.count() == 1

        # 模拟通道层检测到掉线（USB 拔出 / BLE 看门狗触发的同一条入口）
        comm = UsbComm()
        comm.on_disconnected = lambda: w._tool._mgr._on_device_disconnected(dev.address)
        w._tool._mgr._register(MagicMock(), dev.address)
        comm._notify_disconnected()
        app.processEvents()

        assert dev.address not in w._connected, "GUI 应自动移除断开的设备"
        names = [w.lst_devices.item(i).data(0x0100)  # Qt.UserRole
                 for i in range(w.lst_devices.count())]
        assert dev.address not in names, "设备列表应已刷新"
    finally:
        w.close()


def test_gui_refresh_from_plain_thread_updates_list():
    """回归：断开回调在普通（无事件循环）线程里触发时，GUI 仍必须刷新。

    真机场景：USB RX 线程检测到拔出 → _notify_disconnected → GUI。
    旧实现用 QTimer.singleShot(0) 转投主线程，但 RX 线程没有 Qt 事件循环，
    定时器永远不会触发 → 界面不刷新（设备列表里设备还在，但池已清空）。
    新实现改用 Signal（自动排队到主线程）。
    """
    os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
    try:
        from PySide6.QtWidgets import QApplication
    except ImportError:
        print("SKIP: PySide6 未安装，跳过 GUI 用例")
        return

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
            s.connection_type = "usb_single_dongle"

    w = MainWindow()
    try:
        dev = FakeDev("04:dd", "设备D")
        w._connected[dev.address] = dev
        w._tool._mgr._register(MagicMock(), dev.address)
        w._rebuild_list()
        assert w.lst_devices.count() == 1

        # 真正的非 Qt 线程里触发（等价 USB RX 线程）
        import threading

        th = threading.Thread(
            target=w._tool._mgr._on_device_disconnected, args=(dev.address,)
        )
        th.start()
        th.join()
        app.processEvents()  # 分发排队到主线程的 Signal

        assert dev.address not in w._connected, "非 Qt 线程触发时 GUI 也应自动移除设备"
        names = [w.lst_devices.item(i).data(0x0100)  # Qt.UserRole
                 for i in range(w.lst_devices.count())]
        assert dev.address not in names, "设备列表应已刷新"
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
