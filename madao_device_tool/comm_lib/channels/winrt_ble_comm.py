"""Windows WinRT BLE 通道：复用系统已配对已连接的 BLE 设备。

与 BleComm（bleak，需要设备主动广播、自己建立连接）互补：
本类用 BluetoothLEDevice.FromBluetoothAddressAsync 拿到 Windows 已知设备对象，
复用系统建立的 bonded GATT session，**不需要设备重新广播，也不做新的服务发现**
（用 BluetoothCacheMode.CACHED 读已缓存的 services/characteristics）。

适用场景：设备已和 Windows 配对连接（如 T8 键盘正在被系统使用），

UUID（对齐 T8_light/AM50 C 源码 AAA_app_att.c）：
    service: 0000EAE0
    write:   0000EAE1 (主机->设备)
    notify:  0000EAE2 (设备->主机)
"""

from __future__ import annotations

import asyncio
import logging
import threading
from typing import Optional

from .base import BaseComm, CommError
from .ble_comm import (
    TELINK_PROTOCOL_SERVICE_UUID,
    TELINK_WRITE_CHAR_UUID,
    TELINK_NOTIFY_CHAR_UUID,
    FFE0_SERVICE_UUID,
    FFE0_WRITE_CHAR_UUID,
    FFE0_NOTIFY_CHAR_UUID,
)

logger = logging.getLogger(__name__)


def _mac_str_to_int(address: str) -> int:
    """'FF:49:B0:A7:81:2C' -> 0xFF49B0A7812C"""
    return int(address.replace(":", "").replace("-", ""), 16)


class WinrtBleComm(BaseComm):
    """复用 Windows 已建立 BLE 连接的通道。

    构造后用 connect(mac_str_or_int) 指定设备蓝牙地址，
    地址可从 Windows 设备管理器 BTHLE\\DEV_xxxxx 条目获取。
    """

    # 掉线看门狗轮询间隔（秒）
    WATCHDOG_INTERVAL_SEC = 1.0

    def __init__(self, uuid_set: str = "telink") -> None:
        super().__init__()
        # 兼容 bleak 0.22（bleak_winrt）和 bleak 3.0+（winrt_windows_devices_bluetooth）
        try:
            import bleak_winrt  # noqa: F401
            self._winrt_flavor = "bleak_winrt"
        except ImportError:
            try:
                import winrt.windows.devices.bluetooth  # noqa: F401
                self._winrt_flavor = "winrt"
            except ImportError as e:
                raise CommError("缺少 winrt BLE 后端，请 pip install bleak") from e
        if uuid_set == "ffe0":
            self._service_uuid = FFE0_SERVICE_UUID
            self._write_uuid = FFE0_WRITE_CHAR_UUID
            self._notify_uuid = FFE0_NOTIFY_CHAR_UUID
        else:
            self._service_uuid = TELINK_PROTOCOL_SERVICE_UUID
            self._write_uuid = TELINK_WRITE_CHAR_UUID
            self._notify_uuid = TELINK_NOTIFY_CHAR_UUID
        self._loop: Optional[asyncio.AbstractEventLoop] = None
        self._loop_thread: Optional[threading.Thread] = None
        self._loop_ready = threading.Event()
        self._device = None
        self._write_char = None
        self._notify_char = None
        self._notify_token = None
        self._running = threading.Event()
        # 掉线看门狗：设备被动掉线（关机/离开范围）时 notify 静默停止，
        # 若无人检测，上层永远收不到断开通知（GUI 也就不会刷新）。
        self._watchdog_stop = threading.Event()

    # ---- 后台事件循环 ----
    def _start_loop(self):
        self._loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self._loop)
        self._loop_ready.set()
        self._loop.run_forever()

    def _stop_loop(self):
        if self._loop:
            self._loop.call_soon_threadsafe(self._loop.stop)
        if self._loop_thread:
            self._loop_thread.join(timeout=3.0)
        self._loop_thread = None
        self._loop = None
        self._loop_ready.clear()

    def _run_async(self, coro, timeout: float = 15.0):
        """在事件循环里执行协程。

        bleak 0.22（bleak_winrt）：用常驻线程事件循环。
        bleak 3.0+（winrt）：用 asyncio.run（每次新建循环，兼容性更好）。
        """
        if self._winrt_flavor == "bleak_winrt":
            assert self._loop is not None
            fut = asyncio.run_coroutine_threadsafe(coro, self._loop)
            return fut.result(timeout=timeout)
        else:
            # bleak 3.0+：winrt 绑定在线程循环里有问题，用 asyncio.run
            return asyncio.run(asyncio.wait_for(coro, timeout=timeout))

    # ---- BaseComm 实现 ----
    def scan(self) -> None:
        """本通道不主动扫描（依赖 Windows 已配对设备）。空实现。"""
        pass

    def connect(self, target) -> None:
        """连接 Windows 已知的 BLE 设备。

        target：mac 字符串（'FF:49:...'）或 int。
        """
        if self.is_connected:
            return
        addr = _mac_str_to_int(target) if isinstance(target, str) else int(target)

        # bleak 0.22 需要常驻线程事件循环；bleak 3.0+ 用 asyncio.run
        if self._winrt_flavor == "bleak_winrt":
            self._loop_thread = threading.Thread(target=self._start_loop, name="winrt-ble", daemon=True)
            self._loop_thread.start()
            self._loop_ready.wait(timeout=5.0)

        try:
            self._run_async(self._async_connect(addr), timeout=20.0)
        except Exception as e:
            self._stop_loop()
            raise CommError(f"BLE 连接失败: {e}") from e
        # 连接成功后启动掉线看门狗
        self._start_watchdog()

    async def _async_connect(self, addr: int):
        # 兼容 bleak 0.22（bleak_winrt.*）和 bleak 3.0+（winrt.windows.*）
        if self._winrt_flavor == "bleak_winrt":
            from bleak_winrt.windows.devices.bluetooth import BluetoothLEDevice, BluetoothCacheMode
            from bleak_winrt.windows.devices.bluetooth.genericattributeprofile import (
                GattClientCharacteristicConfigurationDescriptorValue,
                GattCommunicationStatus,
            )
            from bleak_winrt.windows.storage.streams import DataReader, DataWriter
        else:
            from winrt.windows.devices.bluetooth import BluetoothLEDevice, BluetoothCacheMode
            from winrt.windows.devices.bluetooth.genericattributeprofile import (
                GattClientCharacteristicConfigurationDescriptorValue,
                GattCommunicationStatus,
            )
            from winrt.windows.storage.streams import DataReader, DataWriter
        self._DataReader = DataReader
        self._DataWriter = DataWriter

        device = await BluetoothLEDevice.from_bluetooth_address_async(addr)
        if device is None:
            raise CommError("Windows 找不到该 BLE 设备（确认已配对且地址正确）")
        self._device = device

        # 用 CACHED 模式拿 services/characteristics（不做新 discovery）
        # 注意：bleak_winrt（0.22）和 winrt（3.0+）的方法名不同
        if self._winrt_flavor == "bleak_winrt":
            svc_result = await device.get_gatt_services_async(BluetoothCacheMode.CACHED)
        else:
            svc_result = await device.get_gatt_services_with_cache_mode_async(BluetoothCacheMode.CACHED)
        if svc_result.status != GattCommunicationStatus.SUCCESS:
            raise CommError(f"GetGattServices 失败: {svc_result.status}")
        target_service = None
        for svc in svc_result.services:
            if self._service_uuid in str(svc.uuid).lower():
                target_service = svc
                break
        if target_service is None:
            raise CommError(f"设备未暴露 service {self._service_uuid}")

        if self._winrt_flavor == "bleak_winrt":
            char_result = await target_service.get_characteristics_async(BluetoothCacheMode.CACHED)
        else:
            char_result = await target_service.get_characteristics_with_cache_mode_async(BluetoothCacheMode.CACHED)
        for ch in char_result.characteristics:
            uuid = str(ch.uuid).lower()
            if self._write_uuid in uuid:
                self._write_char = ch
            if self._notify_uuid in uuid:
                self._notify_char = ch
        if self._write_char is None or self._notify_char is None:
            raise CommError(
                f"特征未找全 write={self._write_char is not None} "
                f"notify={self._notify_char is not None}"
            )

        # 订阅 notify（带重试：先 NONE 清残留 CCCD，再 NOTIFY）
        self._notify_token = self._notify_char.add_value_changed(self._on_value_changed)
        status = await self._try_enable_notify()
        if status != GattCommunicationStatus.SUCCESS:
            raise CommError(f"订阅 notify 失败: {status}")

    async def _try_enable_notify(self, retries: int = 3):
        """重试订阅 notify，处理 GATT session CCCD 残留。"""
        if self._winrt_flavor == "bleak_winrt":
            from bleak_winrt.windows.devices.bluetooth.genericattributeprofile import (
                GattClientCharacteristicConfigurationDescriptorValue,
                GattCommunicationStatus,
            )
        else:
            from winrt.windows.devices.bluetooth.genericattributeprofile import (
                GattClientCharacteristicConfigurationDescriptorValue,
                GattCommunicationStatus,
            )
        for attempt in range(retries):
            # 先清掉可能残留的 CCCD
            try:
                await self._notify_char.write_client_characteristic_configuration_descriptor_async(
                    GattClientCharacteristicConfigurationDescriptorValue.NONE
                )
            except Exception:
                pass
            await asyncio.sleep(0.3)
            status = await self._notify_char.write_client_characteristic_configuration_descriptor_async(
                GattClientCharacteristicConfigurationDescriptorValue.NOTIFY
            )
            if status == GattCommunicationStatus.SUCCESS:
                return status
            await asyncio.sleep(0.5)
        return status

    def _on_value_changed(self, sender, args):
        """winrt notify 回调（在 winrt 线程触发）。"""
        try:
            reader = self._DataReader.from_buffer(args.characteristic_value)
            n = reader.unconsumed_buffer_length
            data = bytes(reader.read_byte() for _ in range(n))
            self._feed_rx(data)
        except Exception:
            pass

    # ---- 掉线看门狗 ----
    def _start_watchdog(self) -> None:
        self._watchdog_stop.clear()
        t = threading.Thread(target=self._watchdog_loop, name="ble-watchdog", daemon=True)
        t.start()

    def _stop_watchdog(self) -> None:
        self._watchdog_stop.set()

    def _watchdog_loop(self) -> None:
        while not self._watchdog_stop.wait(self.WATCHDOG_INTERVAL_SEC):
            if not self._check_connection_alive():
                return

    def _check_connection_alive(self) -> bool:
        """检查 GATT 连接是否存活；发现被动掉线则触发断开通知。

        返回 False 表示看门狗应退出（已掉线并通知 / 设备已被主动关闭）。
        注意：只捕获"读状态"的异常；断开通知的回调异常不在此吞掉。
        """
        if self._device is None:
            return False  # 已被主动 disconnect 清理，无需通知
        try:
            status = getattr(self._device, "connection_status", None)
            if status is None:
                return True  # 后端不暴露该属性，无法检测
            name = getattr(status, "name", None) or str(status)
            # BluetoothConnectionStatus: Connected=0, Disconnected=1
            disconnected = name.endswith("Disconnected") or int(status) == 1
        except Exception:
            return True  # 属性偶发不可用，下一轮再查
        if disconnected:
            logger.info("BLE 连接已断开（看门狗检测到设备掉线）")
            self._notify_disconnected()
            return False
        return True

    def disconnect(self) -> None:
        self._watchdog_stop.set()  # 先停看门狗，避免与主动断开互相触发
        if self._notify_char is not None and self._notify_token is not None:
            try:
                self._run_async(self._async_unsubscribe(), timeout=5.0)
            except Exception:
                pass
        if self._device is not None:
            try:
                self._device.close()
            except Exception:
                pass
        self._write_char = None
        self._notify_char = None
        self._notify_token = None
        self._device = None
        self._stop_loop()
        self._notify_disconnected()

    async def _async_unsubscribe(self):
        if self._notify_char is not None and self._notify_token is not None:
            self._notify_char.remove_value_changed(self._notify_token)

    @property
    def is_connected(self) -> bool:
        return self._write_char is not None and self._notify_char is not None

    def send_raw(self, frame: bytes) -> int:
        if not self.is_connected:
            raise CommError("BLE 未连接")
        try:
            self._run_async(self._async_write(frame), timeout=5.0)
            return len(frame)
        except Exception as e:
            raise CommError(f"BLE 发送失败: {e}") from e

    async def _async_write(self, frame: bytes):
        if self._winrt_flavor == "bleak_winrt":
            from bleak_winrt.windows.devices.bluetooth.genericattributeprofile import GattWriteOption
            from bleak_winrt.windows.storage.streams import Buffer as WinBuffer
            buf = WinBuffer(len(frame))
            buf.length = buf.capacity
            with memoryview(buf) as mv:
                mv[:] = frame
            result = await self._write_char.write_value_with_result_async(buf, GattWriteOption.WRITE_WITHOUT_RESPONSE)
        else:
            # winrt 3.0+ 绑定签名不同：write_value_with_result_async 只接受 buffer，
            # 带 option 需用 write_value_with_result_and_option_async。
            from winrt.windows.devices.bluetooth.genericattributeprofile import GattWriteOption
            from winrt.windows.storage.streams import DataWriter
            writer = DataWriter()
            for b in frame:
                writer.write_byte(b)
            buf = writer.detach_buffer()
            result = await self._write_char.write_value_with_result_and_option_async(
                buf, GattWriteOption.WRITE_WITHOUT_RESPONSE
            )
        return result.status
