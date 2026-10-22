"""枚举 Windows 已配对已连接的 BLE 设备。

用 PowerShell 查询 Windows 设备树（比 winrt 绑定更可靠），
返回 DeviceInfo 列表（含 name/mac/transport）。
"""

from __future__ import annotations

import re
import subprocess
from typing import List

from comm_lib.channels import DeviceInfo

# Windows 下 GUI 程序调子进程时不弹黑窗；其他平台无此常量则置 0
_NO_WINDOW = getattr(subprocess, "CREATE_NO_WINDOW", 0)


def list_paired_ble_devices() -> List[DeviceInfo]:
    """列出 Windows 已配对已连接的 BLE 设备。

    通过 PowerShell Get-PnpDevice 查询 BTHLE 设备节点。
    返回 DeviceInfo 列表，address 为 MAC 字符串。
    """
    # PowerShell 查询所有 BTHLE\DEV_ 设备（已配对的 BLE 设备节点）
    ps_script = (
        "Get-PnpDevice | "
        "Where-Object { $_.InstanceId -match 'BTHLE\\\\DEV_' -and $_.Status -eq 'OK' } | "
        "Select-Object FriendlyName, InstanceId | "
        "Format-List"
    )
    try:
        result = subprocess.run(
            ["powershell", "-NoProfile", "-Command", ps_script],
            capture_output=True, text=True, timeout=10,
            creationflags=_NO_WINDOW,
        )
    except Exception:
        return []

    devices = []
    # 解析输出：FriendlyName 和 InstanceId 交替
    blocks = result.stdout.strip().split("\n\n")
    for block in blocks:
        name_match = re.search(r"FriendlyName\s*:\s*(.+)", block)
        id_match = re.search(r"InstanceId\s*:\s*(BTHLE\\DEV_([0-9A-Fa-f]+))", block)
        if not id_match:
            continue
        mac_raw = id_match.group(2)  # 12 位 hex 如 FF497524FD03
        mac = ":".join(mac_raw[i:i+2] for i in range(0, 12, 2)).upper()
        name = name_match.group(1).strip() if name_match else ""
        devices.append(DeviceInfo(
            transport="ble",
            address=mac,
            name=name,
            extra={"mac_raw": mac_raw, "paired": True},
        ))
    return devices


def filter_target_devices(devices: List[DeviceInfo]) -> List[DeviceInfo]:
    """从已配对设备列表中筛选目标设备（支持私有协议的）。

    判断依据：设备名包含已知关键字（M610/FeiShu/T8/AT/AM/D1/K/N/S/X 等），
    或通过 device_config 的 product_no 判定（需连接后读取）。
    """
    KNOWN_KEYWORDS = ("M610", "FeiShu", "T8", "T9", "AT", "AM", "D1", "K1", "N3", "S1", "S2", "X1")
    targets = []
    for d in devices:
        for kw in KNOWN_KEYWORDS:
            if kw.lower() in d.name.lower():
                targets.append(d)
                break
    return targets


# 蓝牙名称中可识别为键盘的关键字；其余默认按鼠标处理。
_KEYBOARD_NAME_KEYWORDS = ("t8", "t9", "t6", "d1", "n2", "n3", "n8", "k1", "at80")


def infer_device_type_from_ble_name(name: str) -> int:
    """根据蓝牙设备名称推断设备类型（0x31=鼠标 / 0x32=键盘）。

    扫描阶段无法读取 product_no，只能按名称关键字推断。
    键盘关键字明确则返回键盘；否则默认返回鼠标（目前鼠标型号更多）。
    """
    name_lower = (name or "").lower()
    for kw in _KEYBOARD_NAME_KEYWORDS:
        if kw in name_lower:
            return 0x32
    return 0x31
