"""运行时引导：加载 libusb DLL，让 pyusb 能找到后端。

Python 3.11 WindowsApps 沙箱版无法在 exe 目录放 DLL，
此模块从 libusb 包提取 DLL 路径并注册。
所有入口（gui/cli/demo）应尽早 import 本模块。
"""

import os
import platform
import sys


def setup_libusb():
    """注册 libusb DLL 路径，让 pyusb 的 ctypes 查找能命中。"""
    bits = platform.architecture()[0]
    subdir = "x86_64" if "64" in bits else "x86"
    candidates = []

    if getattr(sys, "frozen", False):
        # PyInstaller onedir：DLL 放在 _internal/ 和 _internal/libusb_dll/
        base = os.path.dirname(sys.executable)
        meipass = getattr(sys, "_MEIPASS", os.path.join(base, "_internal"))
        candidates += [
            meipass,
            os.path.join(meipass, "libusb_dll"),
            os.path.join(base, "_internal"),
            os.path.join(base, "_internal", "libusb_dll"),
            base,
        ]
    else:
        # 开发模式：从 libusb 包提取
        try:
            import libusb
            candidates.append(os.path.join(
                os.path.dirname(libusb.__file__), "_platform", "windows", subdir))
        except ImportError:
            pass
        # 项目根目录（可能手动放了 libusb-1.0.dll）
        candidates.append(os.path.dirname(os.path.abspath(__file__)))

    # 把存在的目录加入 DLL 搜索路径 + PATH
    for dll_dir in candidates:
        if not os.path.isdir(dll_dir):
            continue
        if hasattr(os, "add_dll_directory"):
            try:
                os.add_dll_directory(dll_dir)
            except (OSError, FileNotFoundError):
                pass
        os.environ["PATH"] = dll_dir + os.pathsep + os.environ.get("PATH", "")

    # 关键：主动用 ctypes 预加载一次 libusb-1.0.dll，
    # 让 pyusb 的 usb.backend.libusb1._liblookup 命中已加载的 DLL。
    import ctypes
    for dll_dir in candidates:
        if not os.path.isdir(dll_dir):
            continue
        dll_path = os.path.join(dll_dir, "libusb-1.0.dll")
        if os.path.isfile(dll_path):
            try:
                ctypes.CDLL(dll_path)
                return
            except OSError:
                continue


def resource_path(*parts: str) -> str:
    """获取打包/开发模式下资源文件的绝对路径。

    开发模式：资源相对源码包根（madao_device_tool/）解析。
    打包模式（PyInstaller onedir）：资源在 <exe目录>/_internal/ 下，
    其中 sys._MEIPASS 指向 _internal，资源按 datas 中的目标子目录分布。
    """
    if getattr(sys, "frozen", False):
        base = getattr(sys, "_MEIPASS", os.path.dirname(sys.executable))
    else:
        base = os.path.dirname(os.path.abspath(__file__))
    return os.path.join(base, *parts)


# import 时自动执行
setup_libusb()
