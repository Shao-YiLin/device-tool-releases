# -*- mode: python ; coding: utf-8 -*-
"""PyInstaller spec for Madao Device Tool GUI.

构建：
    py -3.11 -m PyInstaller madao_device_tool/gui/gui.spec --noconfirm

产物：dist/MadaoDeviceTool/ 目录，运行 MadaoDeviceTool.exe
"""

import os
import platform
import sys

from PyInstaller.utils.hooks import collect_submodules, collect_data_files

block_cipher = None

# ---- 路径 ----
# SPECPATH = spec 所在目录 = madao_device_tool/gui/
# 包根 = madao_device_tool/ (spec 的上一级)
SPEC_DIR = os.path.abspath(SPECPATH)
TOOL_DIR = os.path.dirname(SPEC_DIR)
PKG_ROOT = os.path.dirname(TOOL_DIR)
GUI_DIR = SPEC_DIR

# libusb DLL（与打包 Python 同架构）
bits = platform.architecture()[0]
subdir = "x86_64" if "64" in bits else "x86"
try:
    import libusb as _libusb_pkg
    libusb_root = os.path.dirname(_libusb_pkg.__file__)
except ImportError:
    libusb_root = None
libusb_dll_dir = os.path.join(libusb_root, "_platform", "windows", subdir) if libusb_root else None

# ---- 数据文件 ----
datas = []
# libusb DLL 放到 exe 同级，_bootstrap.py 会把 exe 目录加进 DLL 搜索路径
if libusb_dll_dir and os.path.isdir(libusb_dll_dir):
    datas.append((libusb_dll_dir, "libusb_dll"))
# 设备图片资源
assets_dir = os.path.join(TOOL_DIR, "assets")
if os.path.isdir(assets_dir):
    datas.append((assets_dir, "assets"))
# PySide6 plugins / translations 由 collect 自动处理

# ---- 二进制 ----
binaries = []

# ---- 隐式导入 ----
hiddenimports = []
# GUI 通过 sys.path.insert 导入 gui 内模块，PyInstaller 静态分析可能漏掉
hiddenimports += collect_submodules("comm_lib")
hiddenimports += collect_submodules("device_model")
hiddenimports += collect_submodules("device_mgr")
hiddenimports += collect_submodules("devtool")
hiddenimports += [
    "device_config", "record_config", "logging_config", "exceptions", "_bootstrap",
]
# gui 内模块（包外导入风格）
hiddenimports += [
    "main_window", "device_worker", "mouse_panel", "keyboard_panel",
    "dongle_panel", "ota_dialog", "key_custom_dialog", "key_codes",
    "multi_link_dialog", "mouse_listener",
]
# 第三方动态导入
hiddenimports += collect_submodules("Crypto")
hiddenimports += collect_submodules("usb")
hiddenimports += collect_submodules("pynput")
# BLE：兼容 bleak 0.22（bleak_winrt）和 3.0+（winrt）
hiddenimports += collect_submodules("bleak")
try:
    import bleak_winrt  # noqa: F401
    hiddenimports += collect_submodules("bleak_winrt")
except ImportError:
    pass
try:
    import winrt  # noqa: F401
    hiddenimports += collect_submodules("winrt")
    hiddenimports += collect_submodules("winrt.windows.devices.bluetooth")
except ImportError:
    pass


a = Analysis(
    ["main.py"],
    pathex=[PKG_ROOT, TOOL_DIR, GUI_DIR],
    binaries=binaries,
    datas=datas,
    hiddenimports=hiddenimports,
    hookspath=[],
    runtime_hooks=[],
    excludes=["tkinter", "test", "pytest"],
    win_no_prefer_redirects=False,
    win_private_assemblies=False,
    cipher=block_cipher,
    noarchive=False,
)

pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

exe = EXE(
    pyz,
    a.scripts,
    [],
    exclude_binaries=True,
    name="MadaoDeviceToolDebug",
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=False,
    console=True,           # windowed
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon=os.path.join(GUI_DIR, "assets", "app.ico") if os.path.exists(os.path.join(GUI_DIR, "assets", "app.ico")) else None,
)

coll = COLLECT(
    exe,
    a.binaries,
    a.zipfiles,
    a.datas,
    strip=False,
    upx=False,
    upx_exclude=[],
    name="MadaoDeviceToolDebug",
)
