"""BLE 通信通道，基于 bleak 实现。

对齐 src_archive/mouseTool/mspeech_ble.py 的外部行为：
扫描 -> 连接 -> write/write-without-response 收发私有协议帧。
私有协议帧结构与 USB 完全一致，仅传输通道不同（验收 3.2 末条）。

bleak 是异步库，本类内部跑一个事件循环线程，对外暴露同步接口，
与 UsbComm 保持调用方式一致，便于上层 BaseComm 统一使用。
"""

from __future__ import annotations

import threading
from typing import List, Optional

from .base import BaseComm, CommError, DeviceInfo

# 设备端 GATT 服务/特征 UUID。
# 对齐 src_archive/T8_light/.../AAA_app_att.c 与 AM50/.../AAA_app_att.c：
#   protocol service:  PROTOCOL_SERVICE_UUID = 0xEAE0
#   write (主机->设备): UUID 0xEAE1 (CHAR_PROP_WRITE|WRITE_WITHOUT_RSP)
#   notify (设备->主机): UUID 0xEAE2 (CHAR_PROP_READ|NOTIFY)
# 这套 UUID 对 T8 键盘和 M610 鼠标都适用（同厂 Telink 方案）。
# 另有 TELINK_SPP_DATA_*(1910/1911) 是 SPP/OTA 服务，非私有协议通道。
TELINK_PROTOCOL_SERVICE_UUID = "0000eae0-0000-1000-8000-00805f9b34fb"
TELINK_WRITE_CHAR_UUID = "0000eae1-0000-1000-8000-00805f9b34fb"
TELINK_NOTIFY_CHAR_UUID = "0000eae2-0000-1000-8000-00805f9b34fb"

# 备用：ffe0/ffe1（部分其他厂商设备如 UD24 走这套）
FFE0_SERVICE_UUID = "0000ffe0-0000-1000-8000-00805f9b34fb"
FFE0_WRITE_CHAR_UUID = "0000ffe1-0000-1000-8000-00805f9b34fb"
FFE0_NOTIFY_CHAR_UUID = "0000ffe1-0000-1000-8000-00805f9b34fb"

# 默认 UUID 集合（Telink 私有协议方案）
BLE_SERVICE_UUID = TELINK_PROTOCOL_SERVICE_UUID
BLE_WRITE_CHAR_UUID = TELINK_WRITE_CHAR_UUID
BLE_NOTIFY_CHAR_UUID = TELINK_NOTIFY_CHAR_UUID


class BleComm(BaseComm):
    """基于 bleak 的 BLE 通信通道。

    支持两套厂商 UUID 方案（设备固件不同用的特征 UUID 不同）：
        - "telink"（默认）：T8 键盘等 Telink 方案设备，服务 0xEAE0，特征 1910/1911
        - "ffe0"：部分鼠标（UD24/M610 等），服务 0xFFE0，特征 ffe1
    构造时通过 uuid_set 选择，也可传入自定义 (service, write, notify) UUID 三元组。
    """

    def __init__(self, scan_timeout: float = 5.0,
                 uuid_set: str = "telink",
                 uuids: Optional[tuple] = None) -> None:
        super().__init__()
        if uuids is not None:
            self._service_uuid, self._write_uuid, self._notify_uuid = uuids
        elif uuid_set == "ffe0":
            self._service_uuid = FFE0_SERVICE_UUID
            self._write_uuid = FFE0_WRITE_CHAR_UUID
            self._notify_uuid = FFE0_NOTIFY_CHAR_UUID
        else:
            self._service_uuid = TELINK_PROTOCOL_SERVICE_UUID
            self._write_uuid = TELINK_WRITE_CHAR_UUID
            self._notify_uuid = TELINK_NOTIFY_CHAR_UUID
        try:
            from bleak import BleakClient, BleakScanner  # noqa: F401
        except ImportError as e:
            raise CommError("缺少 bleak 依赖，请 pip install bleak") from e
        self._scan_timeout = scan_timeout
        self._loop_thread: Optional[threading.Thread] = None
        self._client = None  # BleakClient
        self._loop = None    # asyncio loop
        self._scan_results: List[DeviceInfo] = []
        self._connected_event = threading.Event()
        self._stop_event = threading.Event()

    # ---- 扫描 ----
    def scan(self) -> None:
        """同步阻塞扫描 scan_timeout 秒，结果逐个回调 on_device_found。"""
        self._scan_results.clear()
        self._run_async(self._async_scan())

    async def _async_scan(self):
        from bleak import BleakScanner

        detection_cb = self._on_detection if self.on_device_found else None
        scanner = BleakScanner(detection_callback=detection_cb)
        await scanner.start()
        await asyncio_sleep(self._scan_timeout)
        await scanner.stop()
        # detection_callback 已实时上报，这里无需再处理 discovered_devices

    def _on_detection(self, device, advertisement_data):
        # BleakScanner detection_callback 同步触发
        info = DeviceInfo(
            transport="ble",
            address=device.address,
            name=device.name or "",
            extra={"rssi": getattr(advertisement_data, "rssi", None)},
        )
        # 去重：同一地址只上报一次
        if not any(i.address == info.address for i in self._scan_results):
            self._scan_results.append(info)
            if self.on_device_found:
                self.on_device_found(info)

    # ---- 连接 / 断开 ----
    def connect(self, target) -> None:
        """target 可以是 mac 字符串或 DeviceInfo。"""
        if self.is_connected:
            return
        address = target.address if isinstance(target, DeviceInfo) else str(target)
        self._stop_event.clear()
        self._connected_event.clear()
        self._loop_thread = threading.Thread(
            target=self._loop_target, args=(address,), name="ble-loop", daemon=True
        )
        self._loop_thread.start()
        # 等待连接结果
        if not self._connected_event.wait(timeout=15.0):
            raise CommError(f"BLE 连接超时: {address}")

    def _loop_target(self, address: str):
        import asyncio

        async def _run():
            from bleak import BleakClient

            self._loop = asyncio.get_running_loop()
            try:
                async with BleakClient(address, timeout=15.0) as client:
                    self._client = client
                    await client.start_notify(self._notify_uuid, self._notify_handler)
                    self._connected_event.set()
                    while not self._stop_event.is_set():
                        # 维持连接；断开由 disconnect() 或异常触发
                        if not client.is_connected:
                            break
                        await asyncio.sleep(0.2)
            except Exception as e:
                self._connected_event.set()  # 解除 connect() 等待，让其抛错
                raise CommError(f"BLE 连接失败: {address}: {e}")
            finally:
                self._client = None
                self._notify_disconnected()

        try:
            asyncio.run(_run())
        except CommError:
            pass

    def _notify_handler(self, sender, data: bytearray):
        # bleak notify 回调（在事件循环线程中触发）
        self._feed_rx(bytes(data))

    def disconnect(self) -> None:
        self._stop_event.set()
        client = self._client
        if client is not None and self._loop is not None:
            try:
                import asyncio

                fut = asyncio.run_coroutine_threadsafe(client.disconnect(), self._loop)
                fut.result(timeout=5.0)
            except Exception:
                pass
        if self._loop_thread:
            self._loop_thread.join(timeout=5.0)
        self._client = None
        self._loop = None
        self._loop_thread = None

    @property
    def is_connected(self) -> bool:
        client = self._client
        try:
            return bool(client and client.is_connected)
        except Exception:
            return False

    # ---- 发送 ----
    def send_raw(self, frame: bytes) -> int:
        if not self.is_connected or self._client is None or self._loop is None:
            raise CommError("BLE 未连接")
        import asyncio

        async def _write():
            await self._client.write_gatt_char(self._write_uuid, frame, response=False)

        try:
            fut = asyncio.run_coroutine_threadsafe(_write(), self._loop)
            fut.result(timeout=5.0)
            return len(frame)
        except Exception as e:
            raise CommError(f"BLE 发送失败: {e}") from e

    # ---- 工具 ----
    def _run_async(self, coro):
        import asyncio

        # 扫描等一次性操作在临时事件循环中跑
        async def _wrap():
            await coro

        asyncio.run(_wrap())


def asyncio_sleep(t: float):
    import asyncio

    return asyncio.sleep(t)
