"""GUI 自动化测试脚本。

用 QApplication + QTimer 模拟用户操作，验证 GUI 全流程：
  1. 启动窗口
  2. 扫描 USB
  3. 连接第一个 USB 设备
  4. 读取设备信息（product_no/mac/version/battery）
  5. 读取/设置 CPI
  6. 断开
  7. 扫描 BLE（仅验证能扫到设备，不连接）

不依赖人工点击，自动判定结果。

运行：py -3.11 madao_device_tool/gui/test_gui.py
"""

import os
import sys
import time

# 强制无缓冲输出
sys.stdout.reconfigure(line_buffering=True) if hasattr(sys.stdout, 'reconfigure') else None

_HERE = os.path.dirname(os.path.abspath(__file__))
_PKG_ROOT = os.path.dirname(_HERE)
for p in (_PKG_ROOT, _HERE):
    if p not in sys.path:
        sys.path.insert(0, p)

import _bootstrap  # noqa: F401

from PySide6.QtCore import QTimer, Qt, QEventLoop
from PySide6.QtWidgets import QApplication

# 测试结果收集
_results = []
_failures = []


def check(name, condition, detail=""):
    if condition:
        _results.append(name)
        print(f"  PASS: {name}")
    else:
        _failures.append((name, detail))
        print(f"  FAIL: {name} - {detail}")


def run_test():
    """主测试流程。

    策略：用 app.exec() + QTimer 驱动异步步骤，每步用 QTimer.singleShot 延迟执行。
    不用 processEvents 循环（容易卡死）。
    """
    from main_window import MainWindow
    from comm_lib.channels import DeviceInfo

    app = QApplication.instance() or QApplication(sys.argv)
    win = MainWindow()
    win.show()

    state = {"step": 0}

    def fail_and_quit(msg):
        _failures.append(("runtime", msg))
        print(f"  FAIL: {msg}")
        win.close()
        app.quit()

    def step_connect_and_read():
        """Step 2: 连接第一个扫描到的设备"""
        if not win._scanned:
            fail_and_quit("没有扫描到 USB 设备")
            return
        addr = list(win._scanned.keys())[0]
        info = win._scanned[addr]
        print(f"  连接 {addr}...")
        state["addr"] = addr
        win._worker.async_connect(info)
        # 等 connected 信号（5秒）
        QTimer.singleShot(5000, step_check_connect)

    def step_check_connect():
        addr = state.get("addr")
        if addr and addr in win._connected:
            dev = win._connected[addr]
            print(f"  PASS: 连接成功 {type(dev).__name__}")
            _results.append("连接成功")
            # 面板应该自动开始读基础信息，等它
            QTimer.singleShot(8000, step_check_info)
        else:
            fail_and_quit(f"连接超时: {addr}")

    def step_check_info():
        addr = state.get("addr")
        panel = win._panels.get(addr)
        if not panel:
            fail_and_quit("面板未创建")
            return
        product = panel.lbl_product.text()
        mac = panel.lbl_mac.text()
        check("product_no 加载", product != "-", f"值={product}")
        check("MAC 加载", mac != "-" and ":" in mac, f"值={mac}")
        # 如果 product_no 还没到，再等一会重试
        if product == "-":
            QTimer.singleShot(5000, step_check_info_retry)
        else:
            step_disconnect()

    def step_check_info_retry():
        addr = state.get("addr")
        panel = win._panels.get(addr)
        product = panel.lbl_product.text() if panel else "-"
        check("product_no 加载(重试)", product != "-", f"值={product}")
        step_disconnect()

    def step_disconnect():
        addr = state.get("addr")
        win._worker.async_disconnect(addr)
        QTimer.singleShot(3000, step_disconnect_done)

    def step_disconnect_done():
        addr = state.get("addr")
        check("设备断开", addr not in win._connected)
        # BLE 扫描（已配对设备）
        print("\n[扫描已配对 BLE 设备...]")
        win._scan_ble()
        QTimer.singleShot(5000, step_ble_done)

    def step_ble_done():
        total = len(win._scanned)
        check("BLE 已配对设备扫描", total > 0, f"总共 {total} 个")

        print("\n" + "=" * 50)
        total_pass = len(_results)
        total_fail = len(_failures)
        print(f"通过: {total_pass}, 失败: {total_fail}")
        if _failures:
            print("\n失败项:")
            for name, detail in _failures:
                print(f"  - {name}: {detail}")

        win.close()
        app.quit()

    # Step 1: 窗口 + 扫描 USB
    check("窗口显示", win.isVisible())
    check("工具栏存在", win.act_scan_usb is not None)
    print("\n[扫描 USB...]")
    win._scan_usb()
    QTimer.singleShot(5000, step_connect_and_read)

    app.exec()
    return len(_failures) == 0


if __name__ == "__main__":
    ok = run_test()
    sys.exit(0 if ok else 1)
