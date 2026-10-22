"""进一步复现：从 connect 所在线程（独立线程）提交 write 协程。

WinrtBleComm 实际路径：
- connect() 在独立 threading.Thread 里启动事件循环线程
- 同一个独立线程里继续调用 device.handshake() -> send_raw() -> run_coroutine_threadsafe
"""
from __future__ import annotations

import asyncio
import threading
import traceback

MAC = "FF:49:B0:A7:81:2C"


def mac_to_int(mac: str) -> int:
    return int(mac.replace(":", "").replace("-", ""), 16)


async def connect(addr: int):
    from bleak_winrt.windows.devices.bluetooth import BluetoothLEDevice, BluetoothCacheMode
    from bleak_winrt.windows.devices.bluetooth.genericattributeprofile import (
        GattCommunicationStatus,
        GattClientCharacteristicConfigurationDescriptorValue,
    )
    from bleak_winrt.windows.storage.streams import DataWriter

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
    await notify_char.write_client_characteristic_configuration_descriptor_async(
        GattClientCharacteristicConfigurationDescriptorValue.NOTIFY
    )
    return device, write_char, notify_char, token


async def write_frame(write_char, frame: bytes):
    from bleak_winrt.windows.devices.bluetooth.genericattributeprofile import GattWriteOption
    from bleak_winrt.windows.storage.streams import DataWriter
    writer = DataWriter()
    for b in frame:
        writer.write_byte(b)
    buf = writer.detach_buffer()
    result = await write_char.write_value_with_result_async(buf, GattWriteOption.WRITE_WITHOUT_RESPONSE)
    return result.status


class FakeComm:
    def __init__(self):
        self._loop = None
        self._loop_thread = None
        self._loop_ready = threading.Event()
        self._write_char = None
        self._DataWriter = None

    def _start_loop(self):
        self._loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self._loop)
        self._loop_ready.set()
        self._loop.run_forever()

    def _run_async(self, coro, timeout=15.0):
        fut = asyncio.run_coroutine_threadsafe(coro, self._loop)
        return fut.result(timeout=timeout)

    def connect(self, mac):
        addr = mac_to_int(mac)
        self._loop_thread = threading.Thread(target=self._start_loop, daemon=True)
        self._loop_thread.start()
        self._loop_ready.wait(timeout=5.0)
        from bleak_winrt.windows.storage.streams import DataWriter
        self._DataWriter = DataWriter
        device, write_char, notify_char, token = self._run_async(connect(addr), timeout=20.0)
        self._device = device
        self._write_char = write_char
        self._notify_char = notify_char
        self._notify_token = token

    def send_raw(self, frame: bytes):
        async def _async_write():
            from bleak_winrt.windows.devices.bluetooth.genericattributeprofile import GattWriteOption
            writer = self._DataWriter()
            for b in frame:
                writer.write_byte(b)
            buf = writer.detach_buffer()
            print("  send_raw: buf type", type(buf), "len", buf.length)
            result = await self._write_char.write_value_with_result_async(buf, GattWriteOption.WRITE_WITHOUT_RESPONSE)
            return result.status
        return self._run_async(_async_write(), timeout=5.0)


def worker(comm, mac, ready, result):
    try:
        comm.connect(mac)
        frame = bytes([0xFE, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00])
        status = comm.send_raw(frame)
        result["status"] = status
    except Exception as e:
        result["error"] = e
        traceback.print_exc()
    finally:
        ready.set()


def main():
    comm = FakeComm()
    ready = threading.Event()
    result = {}
    t = threading.Thread(target=worker, args=(comm, MAC, ready, result), daemon=True)
    t.start()
    ready.wait(timeout=30.0)
    print("result:", result)


if __name__ == "__main__":
    main()
