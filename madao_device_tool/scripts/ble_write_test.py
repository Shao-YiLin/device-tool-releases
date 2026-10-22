"""BLE 写入诊断脚本。

从最底层开始验证 WinRT BLE 写入：
1. 列出 Windows 已配对 BLE 设备
2. 连接指定 MAC（或第一个）
3. 查找 Telink 服务/特征
4. 分别用 DataWriter 和 Buffer 两种方式写一帧测试数据
5. 打印异常堆栈

运行：python madao_device_tool/scripts/ble_write_test.py [MAC]
"""
from __future__ import annotations

import asyncio
import subprocess
import sys
import traceback


def list_paired_ble():
    ps = (
        "Get-PnpDevice | "
        "Where-Object { $_.InstanceId -match 'BTHLE\\\\DEV_' -and $_.Status -eq 'OK' } | "
        "Select-Object FriendlyName, InstanceId | Format-List"
    )
    r = subprocess.run(
        ["powershell", "-NoProfile", "-Command", ps],
        capture_output=True, text=True, timeout=10,
    )
    print("=== PowerShell 已配对 BLE 设备 ===")
    print(r.stdout if r.stdout else "(无)")
    if r.returncode != 0:
        print("PowerShell stderr:", r.stderr)
    # 解析第一个
    import re
    blocks = r.stdout.strip().split("\n\n")
    for b in blocks:
        m = re.search(r"InstanceId\s*:\s*BTHLE\\DEV_([0-9A-Fa-f]+)", b)
        if m:
            raw = m.group(1)
            mac = ":".join(raw[i:i+2] for i in range(0, 12, 2)).upper()
            name = re.search(r"FriendlyName\s*:\s*(.+)", b)
            name = name.group(1).strip() if name else ""
            return mac, name
    return None, None


def mac_to_int(mac: str) -> int:
    return int(mac.replace(":", "").replace("-", ""), 16)


async def main(mac: str):
    from bleak_winrt.windows.devices.bluetooth import BluetoothLEDevice, BluetoothCacheMode
    from bleak_winrt.windows.devices.bluetooth.genericattributeprofile import (
        GattCommunicationStatus,
        GattWriteOption,
        GattClientCharacteristicConfigurationDescriptorValue,
    )
    from bleak_winrt.windows.storage.streams import DataWriter, Buffer

    print(f"\n=== 连接设备 {mac} ===")
    addr = mac_to_int(mac)
    device = await BluetoothLEDevice.from_bluetooth_address_async(addr)
    if device is None:
        print("ERROR: Windows 返回 device=None，确认设备已配对且当前已连接")
        return
    print("device obtained:", device.name)

    print("\n=== 获取 GATT 服务 (CACHED) ===")
    svc_result = await device.get_gatt_services_async(BluetoothCacheMode.CACHED)
    print("services status:", svc_result.status)
    if svc_result.status != GattCommunicationStatus.SUCCESS:
        print("ERROR: 获取服务失败")
        return

    svc_uuid = "0000eae0"
    write_uuid = "0000eae1"
    notify_uuid = "0000eae2"
    target_svc = None
    for svc in svc_result.services:
        print("  service:", svc.uuid)
        if svc_uuid in str(svc.uuid).lower():
            target_svc = svc
    if target_svc is None:
        print("ERROR: 未找到 Telink service (eae0)")
        return
    print("target service:", target_svc.uuid)

    print("\n=== 获取特征 ===")
    char_result = await target_svc.get_characteristics_async(BluetoothCacheMode.CACHED)
    write_char = None
    notify_char = None
    for ch in char_result.characteristics:
        u = str(ch.uuid).lower()
        print("  characteristic:", u)
        if write_uuid in u:
            write_char = ch
        if notify_uuid in u:
            notify_char = ch
    print("write_char:", write_char is not None, "notify_char:", notify_char is not None)
    if not write_char or not notify_char:
        print("ERROR: 特征未找全")
        return

    print("\n=== 订阅 notify ===")
    def on_notify(sender, args):
        print("  [notify] received bytes, length:", len(args.characteristic_value) if args.characteristic_value else 0)
    token = notify_char.add_value_changed(on_notify)
    status = await notify_char.write_client_characteristic_configuration_descriptor_async(
        GattClientCharacteristicConfigurationDescriptorValue.NOTIFY
    )
    print("notify subscribe status:", status)

    # 构造一个 harmless 的测试帧：handshake 帧
    # build_frame(cmd=HANDSHAKE=0x01, payload=b'', seq=0)
    from comm_lib.protocol.frame import build_frame
    frame = build_frame(0x01, b"", 0)
    print(f"\n=== 测试写入 frame ({len(frame)} bytes) ===")
    print("frame hex:", frame.hex())

    # 方式 1: DataWriter.detach_buffer()
    print("\n--- 方式 1: DataWriter.detach_buffer() ---")
    try:
        writer = DataWriter()
        for b in frame:
            writer.write_byte(b)
        buf1 = writer.detach_buffer()
        print("buf1 type:", type(buf1), "length:", buf1.length)
        result1 = await write_char.write_value_with_result_async(buf1, GattWriteOption.WRITE_WITHOUT_RESPONSE)
        print("write result status:", result1.status)
    except Exception as e:
        print("ERROR:", type(e).__name__, e)
        traceback.print_exc()

    # 方式 2: Buffer + memoryview
    print("\n--- 方式 2: Buffer + memoryview ---")
    try:
        buf2 = Buffer(len(frame))
        buf2.length = buf2.capacity
        with memoryview(buf2) as mv:
            mv[:] = frame
        print("buf2 type:", type(buf2), "length:", buf2.length)
        result2 = await write_char.write_value_with_result_async(buf2, GattWriteOption.WRITE_WITHOUT_RESPONSE)
        print("write result status:", result2.status)
    except Exception as e:
        print("ERROR:", type(e).__name__, e)
        traceback.print_exc()

    # 方式 3: write_value_async (无 result)
    print("\n--- 方式 3: write_value_async (无 option) ---")
    try:
        writer = DataWriter()
        for b in frame:
            writer.write_byte(b)
        buf3 = writer.detach_buffer()
        await write_char.write_value_async(buf3)
        print("write_value_async ok")
    except Exception as e:
        print("ERROR:", type(e).__name__, e)
        traceback.print_exc()

    print("\n=== 清理 ===")
    try:
        notify_char.remove_value_changed(token)
        device.close()
    except Exception as e:
        print("cleanup error:", e)


if __name__ == "__main__":
    mac = sys.argv[1] if len(sys.argv) > 1 else None
    if mac is None:
        mac, name = list_paired_ble()
        if mac is None:
            print("未找到已配对 BLE 设备，请提供 MAC 参数")
            sys.exit(1)
        print(f"自动选择第一个设备: {name} ({mac})")
    print("using python:", sys.executable, sys.version)
    asyncio.run(main(mac))
