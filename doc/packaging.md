# 打包注意事项（PyInstaller）

本文档总结 `madao_device_tool` GUI 用 PyInstaller 打包成 Windows 可执行文件的所有关键点和踩过的坑。
打包配置在 `madao_device_tool/gui/gui.spec`。

---

## 1. 环境准备（最重要）

### 1.1 必须用 python.org 官方版 Python，不能用 Microsoft Store 版

**这是头号坑**：Microsoft Store 沙箱版 Python（路径在 `C:\Program Files\WindowsApps\...`）打包出来的 exe，
在双击运行时会报 `python311.dll 找不到指定模块`。

**根因**：Store 版 Python 的 `python311.dll` 依赖沙箱运行时的特殊 DLL 重定向，PyInstaller 无法捕获这些间接依赖，
拷贝到普通目录后子依赖找不到。

**正确做法**：
- 安装官方版：`winget install Python.Python.3.11`（或去 python.org 下载）
- 安装路径应为：`C:\Users\<用户名>\AppData\Local\Programs\Python\Python311\`
- **不要**用 `py -3.11`，它指向 Store 版

**验证当前用的是哪个 Python**：
```powershell
& "$env:LOCALAPPDATA\Programs\Python\Python311\python.exe" -c "import sys; print(sys.executable)"
# 应输出 C:\Users\...\AppData\Local\Programs\Python\Python311\python.exe
```

### 1.2 在官方版 Python 里装依赖

```powershell
$py = "$env:LOCALAPPDATA\Programs\Python\Python311\python.exe"
& $py -m pip install --upgrade pip
& $py -m pip install -r madao_device_tool/requirements.txt
& $py -m pip install pyinstaller libusb
```

依赖清单（`requirements.txt`）：
- `bleak` — BLE 蓝牙
- `pyusb` — USB
- `pycryptodome` — 加密
- `pynput` — 回报率测量
- `PySide6` — GUI
- 额外：`pyinstaller`、`libusb`（提供 libusb-1.0.dll）

---

## 2. 打包命令

```powershell
$py = "$env:LOCALAPPDATA\Programs\Python\Python311\python.exe"
Set-Location "E:\pythonProjects\newMouseTool\madao_device_tool\gui"
& $py -m PyInstaller gui.spec --noconfirm `
    --distpath "E:\pythonProjects\newMouseTool\dist" `
    --workpath "E:\pythonProjects\newMouseTool\build"
```

**注意**：工作目录必须在 `madao_device_tool/gui/`（spec 所在目录），因为 spec 里用相对路径 `main.py` 作为入口。

### 产物结构（onedir 模式）

```
dist/MadaoDeviceTool/
├── MadaoDeviceTool.exe       ← 双击运行
└── _internal/                ← 依赖目录（必须和 exe 一起分发）
    ├── python311.dll
    ├── libusb-1.0.dll         ← 必须是 64 位（见第 4 节）
    ├── PySide6/               ← GUI 框架
    ├── assets/images/         ← 53 张设备图片（见第 5 节）
    ├── libusb_dll/            ← libusb 其他 DLL
    └── ...
```

**分发方式**：把整个 `dist/MadaoDeviceTool/` 文件夹打包成 zip 发给别人，对方双击 `MadaoDeviceTool.exe` 即可，无需安装 Python。

---

## 3. 常见坑及解决方案

### 3.1 黑窗口闪现（点扫描设备时）

**症状**：GUI 启动正常，但点击"扫描设备"或鉴权时闪过黑色控制台窗口。

**根因**：代码里用 `subprocess` 调用 PowerShell（查 BLE 设备）和 `wmic`（读主板序列号）。
打包成 GUI 程序后，子进程会默认分配新控制台窗口。

**已修复的位置**（都加了 `creationflags=_NO_WINDOW`）：
- `comm_lib/channels/ble_device_enumeration.py` — PowerShell 查 BLE
- `device_model/auth.py` — wmic 读序列号

**跨平台写法**（`CREATE_NO_WINDOW` 只在 Windows 存在）：
```python
_NO_WINDOW = getattr(subprocess, "CREATE_NO_WINDOW", 0)
subprocess.run([...], creationflags=_NO_WINDOW)
```

### 3.2 USB 扫描报 "No backend available"

**这是最隐蔽的坑**，分两个层次：

#### 层次一：libusb DLL 架构错误（最常见）

**症状**：双击 exe 启动正常，但点扫描设备报：
```
[ERROR] scan_usb: 扫描 USB 失败: No backend available
```
而源码运行正常。

**根因**：PyInstaller 的 libusb hook 会错误地把 **32 位的 `libusb-1.0.dll`** 收集到 `_internal\`。
64 位 Python 加载 32 位 DLL 会失败，pyusb 就报 "No backend available"。

**验证方法**——检查打包产物里的 DLL 架构：
```powershell
$f = "dist\MadaoDeviceTool\_internal\libusb-1.0.dll"
$bytes = [System.IO.File]::ReadAllBytes($f)
$peOff = [System.BitConverter]::ToInt32($bytes, 0x3C)
$machine = [System.BitConverter]::ToUInt16($bytes, $peOff + 4)
# 0x8664 = x64 (正确) / 0x14c = x86 (错误)
```

正确 DLL 规格：
| 架构 | DLL 大小 | machine 值 |
|---|---|---|
| 64 位 ✅ | 158720 字节 | 0x8664 |
| 32 位 ❌ | 129536 字节 | 0x14c |

**修复**（已在 `gui.spec`）：把正确架构的 DLL 作为 `binaries` 显式声明，**覆盖** PyInstaller 错误收集的版本：
```python
binaries = [
    (libusb_dll_path, "."),  # 强制 64 位 DLL 放到 _internal/，覆盖 32 位
]
```

#### 层次二：pyusb 找不到 DLL

即便 DLL 在 `_internal\`，pyusb 用 `ctypes.util.find_library("libusb-1.0")` 查找，它只搜系统目录/PATH，
不搜 `_internal\`。

**修复**（已在 `_bootstrap.py` 的 `setup_libusb()`）：主动用 `ctypes.CDLL(dll_path)` 预加载 DLL 到进程，
后续 `find_library` 就能命中：
```python
import ctypes
for dll_dir in candidates:
    dll_path = os.path.join(dll_dir, "libusb-1.0.dll")
    if os.path.isfile(dll_path):
        try:
            ctypes.CDLL(dll_path)  # 预加载
            return
        except OSError:
            continue
```

### 3.3 打包后图片资源丢失

**症状**：GUI 能启动但设备图片不显示。

**根因**：代码用 `__file__` 推算图片路径：
```python
_IMG_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "assets", "images")
```
打包后 `__file__` 指向 PYZ 内部虚拟路径，推算失败。

**修复**（已在 `_bootstrap.py` 加 `resource_path()` 函数）：
```python
def resource_path(*parts):
    if getattr(sys, "frozen", False):
        base = getattr(sys, "_MEIPASS", os.path.dirname(sys.executable))
    else:
        base = os.path.dirname(os.path.abspath(__file__))
    return os.path.join(base, *parts)
```
`mouse_panel.py` 和 `keyboard_panel.py` 都改用 `resource_path("assets", "images")`。

同时 spec 的 `datas` 必须包含图片目录：
```python
datas.append((assets_dir, "assets"))
```

### 3.4 双击 exe 报 python311.dll 找不到（Store 版 Python 症状）

见第 1.1 节——必须用官方版 Python 重新打包。

### 3.5 双击的是 build 目录里的 exe

**症状**：exe 启动报各种 DLL 找不到。

**根因**：`build\` 是 PyInstaller 的**中间产物**，只有 bootloader 拷贝的临时 exe，依赖还没放进去。
最终产物在 `dist\`。

**解决**：永远只运行 `dist\MadaoDeviceTool\MadaoDeviceTool.exe`。

---

## 4. spec 文件关键配置说明

`madao_device_tool/gui/gui.spec`：

### 4.1 路径推算（易错点）

spec 里的 `SPECPATH` 变量是 **spec 文件所在目录**（即 `madao_device_tool/gui/`），不是仓库根：
```python
SPEC_DIR = os.path.abspath(SPECPATH)      # madao_device_tool/gui/
TOOL_DIR = os.path.dirname(SPEC_DIR)      # madao_device_tool/
PKG_ROOT = os.path.dirname(TOOL_DIR)      # 仓库根
GUI_DIR = SPEC_DIR
```
入口必须用相对 spec 目录的路径：`["main.py"]`，不能用绝对路径（PyInstaller 会按 SPECPATH 拼接）。

### 4.2 libusb DLL 强制覆盖（见 3.2）

```python
# 正确架构(64位)的 DLL 作为 binary 放到 _internal/，覆盖 PyInstaller 错误收集的 32 位版本
binaries = [(libusb_dll_path, ".")]
```

### 4.3 资源文件

```python
datas = [
    (libusb_dll_dir, "libusb_dll"),   # libusb 其他 DLL
    (assets_dir, "assets"),           # 设备图片
]
```

### 4.4 隐式导入（hiddenimports）

GUI 用 `sys.path.insert` 做包外导入，PyInstaller 静态分析会漏掉，必须显式声明：
```python
hiddenimports += collect_submodules("comm_lib")
hiddenimports += collect_submodules("device_model")
hiddenimports += collect_submodules("device_mgr")
hiddenimports += collect_submodules("devtool")
hiddenimports += ["device_config", "record_config", "logging_config", "exceptions", "_bootstrap"]
# gui 内模块（包外导入）
hiddenimports += ["main_window", "device_worker", "mouse_panel", "keyboard_panel",
                  "dongle_panel", "ota_dialog", "key_custom_dialog", "key_codes",
                  "multi_link_dialog", "mouse_listener"]
# 第三方动态导入
hiddenimports += collect_submodules("Crypto")
hiddenimports += collect_submodules("usb")
hiddenimports += collect_submodules("pynput")
hiddenimports += collect_submodules("bleak")
hiddenimports += collect_submodules("winrt")
```

### 4.5 打包模式

- `console=False` — windowed 模式，无控制台（正式发布）
- `console=True` — 保留控制台（调试用，能看 stderr）
- onedir 模式（生成文件夹，启动快）

---

## 5. 打包后验证清单

每次打包后按这个清单检查：

1. **DLL 架构正确**：
   ```powershell
   $f = "dist\MadaoDeviceTool\_internal\libusb-1.0.dll"
   # 应为 158720 字节 / x64
   ```

2. **图片资源齐全**：`dist\MadaoDeviceTool\_internal\assets\images\` 应有 53 张 png

3. **启动正常**：双击 exe，进程稳定存活不退出

4. **USB 扫描正常**：点扫描设备，不报 "No backend available"

5. **无黑窗口**：点扫描设备时不闪黑色控制台窗口

6. **图片显示正常**：连接设备后右侧面板显示对应产品图

---

## 6. 临时调试技巧

### 6.1 把 exe 改成 console 模式看报错

复制 spec，改 `console=True`，重新打包：
```powershell
$spec = "madao_device_tool/gui/gui_debug.spec"
Copy-Item madao_device_tool/gui/gui.spec $spec
(Get-Content $spec -Raw) -replace 'console=False','console=True' |
    Set-Content $spec -Encoding UTF8
& $py -m PyInstaller $spec --noconfirm --distpath dist_debug
```
console 版 exe 的报错会直接打到黑窗口。

### 6.2 诊断 DLL 加载

在 `_bootstrap.py` 临时加日志，把 `setup_libusb()` 的执行情况写到文件：
```python
def _diag_log(*msgs):
    import time
    path = os.path.join(os.path.dirname(sys.executable), "_bootstrap_diag.log")
    with open(path, "a", encoding="utf-8") as f:
        f.write("[%s] %s\n" % (time.strftime("%H:%M:%S"), " | ".join(str(m) for m in msgs)))
```
启动 exe 后查看 `dist\MadaoDeviceTool\_bootstrap_diag.log`。

---

## 7. 目标机器要求

- **64 位 Windows**（打包用的是 64 位 Python 和 64 位 DLL）
- 无需安装 Python
- USB 功能需用 [Zadig](https://zadig.akeo.ie/) 把设备 interface 1 的驱动替换成 WinUSB（见项目 README）
- 如杀毒软件拦截，把 `MadaoDeviceTool.exe` 加白名单

---

## 8. 相关文件

| 文件 | 作用 |
|---|---|
| `madao_device_tool/gui/gui.spec` | PyInstaller 打包配置 |
| `madao_device_tool/_bootstrap.py` | 运行时引导：加载 libusb DLL + 资源路径推算 |
| `madao_device_tool/gui/main.py` | GUI 入口 |
| `madao_device_tool/requirements.txt` | 依赖清单 |
