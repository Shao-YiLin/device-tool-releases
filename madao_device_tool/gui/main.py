"""GUI 主入口。

运行：
    py -3.11 madao_device_tool/gui/main.py
"""

from __future__ import annotations

import logging
import os
import sys

# 把 madao_device_tool/ 和 madao_device_tool/gui/ 加入 sys.path：
# - PKG_ROOT：devtool / device_model / comm_lib 等业务模块可被 import
# - GUI_DIR：gui 内部用绝对 import（device_worker / main_window 等）
_HERE = os.path.dirname(os.path.abspath(__file__))
_PKG_ROOT = os.path.dirname(_HERE)
for p in (_PKG_ROOT, _HERE):
    if p not in sys.path:
        sys.path.insert(0, p)

# 加载 libusb DLL（pyusb 后端依赖）
import _bootstrap  # noqa: F401·

from logging_config import setup_logging
setup_logging()  # 初始化日志系统（控制台+文件）

from PySide6.QtCore import Qt
from PySide6.QtGui import QIcon
from PySide6.QtWidgets import QApplication

from main_window import MainWindow


def _app_icon() -> QIcon:
    """应用 LOGO：assets/app.ico（开发/打包模式均由 resource_path 解析）。"""
    try:
        from _bootstrap import resource_path
        path = resource_path("assets", "app.ico")
        if os.path.isfile(path):
            return QIcon(path)
    except Exception:
        pass
    return QIcon()


def main() -> int:
    app = QApplication(sys.argv)
    app.setApplicationName("Madao Device Tool")
    # Windows 任务栏：显式设置 AppUserModelID，防止任务栏显示默认 Python 图标
    if sys.platform == "win32":
        try:
            import ctypes
            ctypes.windll.shell32.SetCurrentProcessExplicitAppUserModelID(
                "Madao.DeviceTool.1")
        except Exception:
            pass
    icon = _app_icon()
    if not icon.isNull():
        app.setWindowIcon(icon)  # 主窗口/对话框标题栏 + 任务栏
    win = MainWindow()
    win.show()
    code = app.exec()
    # 关窗后强制结束进程：残留的 QThread/RX 线程（非守护）会吊住进程，
    # 变成无窗口僵尸并继续占住 USB 句柄，导致下次启动
    # claim 报 Access denied（表现为"扫描得到但全部连接失败"，极难排查）
    logging.shutdown()
    os._exit(code if isinstance(code, int) else 0)


if __name__ == "__main__":
    sys.exit(main())
