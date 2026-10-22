"""复现 GUI 线程模型的 BLE 写入诊断脚本。

WinrtBleComm 实际用法：
- connect() 在独立线程里启动常驻 asyncio 事件循环
- send_raw() 通过 asyncio.run_coroutine_threadsafe 提交 write 协程到该循环

本脚本模拟该模型，验证是否会复现 Invalid parameter count。
"""
from __future__ import annotations

import asyncio
import threading
import traceback

MAC = "FF:49:B0:A7:81:2C"  # 可改成你的设备 MAC


def mac_to_int(mac: str) -> int:
    return int(mac.replace(":", "").replace("-", ""), 16)


async def connect(addr: int):
    from bleak_winrt.windows.devices.bluetooth import BluetoothLEDevice, BluetoothCacheMode
    from bleak_winrt.windows.devices.bluetooth.genericattributeprofile import (
        GattCommunicationStatus,
        GattClientCharacteristicConfigurationDescriptorValue,
    )
    from bleak_winrt.windows.storage.streams import DataReader, DataWriter

    device = await BluetoothLEDevice.from_bluetooth_address_async(addr)
    if device is None:
        raise RuntimeError("device=None")
    svc_result = await device.get_gatt_services_async(BluetoothCacheMode.CACHED)
    if svc_result.status != GattCommunicationStatus.SUCCESS:
        raise RuntimeError(f"services failed {svc_result.status}")
    target_svc = None
    for svc in svc_result.services:
        if "0000eae0" in str(svc.uuid).lower():
            target_svc = svc
            break
    if target_svc is None:
        raise RuntimeError("no eae0 service")
    char_result = await target_svc.get_characteristics_async(BluetoothCacheMode.CACHED)
    write_char = notify_char = None
    for ch in char_result.characteristics:
        u = str(ch.uuid).lower()
        if "0000eae1" in u:
            write_char = ch
        if "0000eae2" in u:
            notify_char = ch
    if not write_char or not notify_char:
        raise RuntimeError("chars not found")

    def on_notify(sender, args):
        print("  [notify] length:", len(args.characteristic_value) if args.characteristic_value else 0)
    token = notify_char.add_value_changed(on_notify)
    status = await notify_char.write_client_characteristic_configuration_descriptor_async(
        GattClientCharacteristicConfigurationDescriptorValue.NOTIFY
    )
    print("notify status:", status)
    return device, write_char, notify_char, token


async def write_with_datawriter(write_char, frame: bytes):
    from bleak_winrt.windows.devices.bluetooth.genericattributeprofile import GattWriteOption
    from bleak_winrt.windows.storage.streams import DataWriter
    writer = DataWriter()
    for b in frame:
        writer.write_byte(b)
    buf = writer.detach_buffer()
    print("write_with_datawriter buf type:", type(buf), "len:", buf.length)
    result = await write_char.write_value_with_result_async(buf, GattWriteOption.WRITE_WITHOUT_RESPONSE)
    print("write_with_datawriter result:", result.status)


async def write_with_buffer(write_char, frame: bytes):
    from bleak_winrt.windows.devices.bluetooth.genericattributeprofile import GattWriteOption
    from bleak_winrt.windows.storage.streams import Buffer
    buf = Buffer(len(frame))
    buf.length = buf.capacity
    with memoryview(buf) as mv:
        mv[:] = frame
    print("write_with_buffer buf type:", type(buf), "len:", buf.length)
    result = await write_char.write_value_with_result_async(buf, GattWriteOption.WRITE_WITHOUT_RESPONSE)
    print("write_with_buffer result:", result.status)


def loop_thread(loop, ready):
    asyncio.set_event_loop(loop)
    ready.set()
    loop.run_forever()


def main():
    addr = mac_to_int(MAC)
    loop = asyncio.new_event_loop()
    ready = threading.Event()
    t = threading.Thread(target=loop_thread, args=(loop, ready), daemon=True)
    t.start()
    ready.wait(timeout=5.0)

    # 在事件循环线程里 connect
    print("=== connect in event loop thread ===")
    fut = asyncio.run_coroutine_threadsafe(connect(addr), loop)
    try:
        device, write_char, notify_char, token = fut.result(timeout=20.0)
    except Exception as e:
        print("connect failed:", e)
        traceback.print_exc()
        return

    frame = bytes([0xFE, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00])

    # 从主线程提交 write 协程（模拟 send_raw）
    print("\n=== DataWriter write submitted from main thread ===")
    try:
        fut1 = asyncio.run_coroutine_threadsafe(write_with_datawriter(write_char, frame), loop)
        fut1.result(timeout=5.0)
    except Exception as e:
        print("DataWriter write failed:", type(e).__name__, e)
        traceback.print_exc()

    print("\n=== Buffer write submitted from main thread ===")
    try:
        fut2 = asyncio.run_coroutine_threadsafe(write_with_buffer(write_char, frame), loop)
        fut2.result(timeout=5.0)
    except Exception as e:
        print("Buffer write failed:", type(e).__name__, e)
        traceback.print_exc()

    print("\n=== cleanup ===")
    try:
        notify_char.remove_value_changed(token)
        device.close()
    except Exception as e:
        print("cleanup error:", e)
    loop.call_soon_threadsafe(loop.stop)
    t.join(timeout=3.0)


if __name__ == "__main__":
    main()
