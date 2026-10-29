# -*- coding: utf-8 -*-
"""OTA 窗口随设备断开移除对应行（GUI 层）。

覆盖：
1. OtaDialog.remove_device：移除行、取消进行中的升级、行号重排
2. 移除后表格行数与设备数一致
3. 断开不存在的设备返回 False（幂等）
4. 主窗口被动断开时通知 OTA 窗口移除设备

运行：python madao_device_tool/test/test_ota_disconnect.py
"""
import os
import sys

_PKG_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_GUI_DIR = os.path.join(_PKG_ROOT, "gui")
sys.path.insert(0, _PKG_ROOT)
sys.path.insert(0, _GUI_DIR)
os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from PySide6.QtWidgets import QApplication

app = QApplication.instance() or QApplication([])

from ota_dialog import OtaDialog


class _FakeDev:
    def __init__(self, address, name="dev"):
        self.address = address
        self.name = name
        self.product_no = b"1042"


def _make_dialog(addresses):
    devs = [_FakeDev(a, f"设备{i+1}") for i, a in enumerate(addresses)]
    dlg = OtaDialog(devs, None)
    dlg.show()
    return dlg


def test_remove_device_removes_row():
    dlg = _make_dialog(["4:1", "4:2", "4:3"])
    try:
        assert dlg.table.rowCount() == 3
        assert dlg.remove_device("4:2") is True
        assert dlg.table.rowCount() == 2
        assert len(dlg._rows) == 2
        # 被移除的设备不在列表里
        assert all(r.address != "4:2" for r in dlg._rows)
    finally:
        dlg.close()


def test_remove_device_reindexes_rows():
    """移除中间行后行号必须连续（行号用于定位表格控件）。"""
    dlg = _make_dialog(["4:1", "4:2", "4:3"])
    try:
        dlg.remove_device("4:1")
        assert [r.index for r in dlg._rows] == [0, 1]
        assert dlg.table.rowCount() == 2
        # 行控件仍在正确的行上
        for r in dlg._rows:
            assert dlg.table.cellWidget(r.index, 1) is r.name_label
    finally:
        dlg.close()


def test_remove_unknown_device_is_noop():
    dlg = _make_dialog(["4:1"])
    try:
        assert dlg.remove_device("9:9") is False
        assert dlg.table.rowCount() == 1
    finally:
        dlg.close()


def test_remove_device_cancels_running_task():
    """正在升级的设备断开：请求取消并清理地址占用。"""
    from ota_dialog import _ACTIVE_ADDRESSES, _ACTIVE_LOCK

    dlg = _make_dialog(["4:1"])
    try:
        row = dlg._rows[0]
        row.cancel["cancel"] = False
        row.task = object()      # 占位：非 None 且未 finished
        row.finished = False
        with _ACTIVE_LOCK:
            _ACTIVE_ADDRESSES.add("4:1")
        assert dlg.remove_device("4:1") is True
        assert row.cancel["cancel"] is True
        with _ACTIVE_LOCK:
            assert "4:1" not in _ACTIVE_ADDRESSES
    finally:
        dlg.close()


def test_main_window_notifies_ota_dialog():
    """主窗口被动断开 → OTA 窗口移除该设备行。"""
    from main_window import MainWindow

    win = MainWindow()
    try:
        dlg = _make_dialog(["4:aa", "4:bb"])
        win._notify_ota_device_gone("4:aa")
        assert dlg.table.rowCount() == 1
        assert dlg._rows[0].address == "4:bb"
    finally:
        win.close()


if __name__ == "__main__":
    fns = [v for k, v in sorted(globals().items()) if k.startswith("test_")]
    passed = 0
    for fn in fns:
        try:
            fn()
            print(f"PASS: {fn.__name__}")
            passed += 1
        except AssertionError as e:
            print(f"FAIL: {fn.__name__}: {e}")
        except Exception as e:
            print(f"ERROR: {fn.__name__}: {type(e).__name__}: {e}")
    print(f"\n{'=' * 40}\n通过: {passed}, 失败: {len(fns) - passed}")
    sys.exit(0 if passed == len(fns) else 1)
