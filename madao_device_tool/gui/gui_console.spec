# -*- mode: python ; coding: utf-8 -*-
import os
import platform
from PyInstaller.utils.hooks import collect_submodules

block_cipher = None
PKG_ROOT = os.path.abspath(SPECPATH)
TOOL_DIR = os.path.join(PKG_ROOT, "madao_device_tool")
GUI_DIR = os.path.join(TOOL_DIR, "gui")

bits = platform.architecture()[0]
subdir = "x86_64" if "64" in bits else "x86"
try:
    import libusb as _libusb_pkg
    libusb_dll_dir = os.path.join(os.path.dirname(_libusb_pkg.__file__), "_platform", "windows", subdir)
except ImportError:
    libusb_dll_dir = None

datas = []
if libusb_dll_dir and os.path.isdir(libusb_dll_dir):
    datas.append((libusb_dll_dir, "libusb_dll"))

hiddenimports = []
hiddenimports += collect_submodules("comm_lib")
hiddenimports += collect_submodules("device_model")
hiddenimports += collect_submodules("device_mgr")
hiddenimports += collect_submodules("devtool")
hiddenimports += ["device_config", "record_config", "logging_config", "exceptions", "_bootstrap"]
hiddenimports += ["main_window", "device_worker", "mouse_panel", "keyboard_panel",
                  "dongle_panel", "ota_dialog", "key_custom_dialog", "key_codes",
                  "multi_link_dialog", "mouse_listener"]
hiddenimports += collect_submodules("Crypto")
hiddenimports += collect_submodules("usb")
hiddenimports += collect_submodules("pynput")
hiddenimports += collect_submodules("bleak")
try:
    import winrt
    hiddenimports += collect_submodules("winrt")
except ImportError:
    pass

a = Analysis(
    ["main.py"],
    pathex=[PKG_ROOT, TOOL_DIR, GUI_DIR],
    binaries=[],
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
    pyz, a.scripts, [],
    exclude_binaries=True,
    name="MadaoDeviceToolConsole",
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=False,
    console=True,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
)
coll = COLLECT(
    exe, a.binaries, a.zipfiles, a.datas,
    strip=False, upx=False, upx_exclude=[],
    name="MadaoDeviceToolConsole",
)
