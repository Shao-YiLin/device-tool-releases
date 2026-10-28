"""GUI 后台工作线程。

所有设备操作（scan/connect/get/set 等）都在后台 QThread 执行，
通过 Signal 把结果回传主线程，避免阻塞 UI。

关键：async_* 方法通过 Signal 触发（do_scan_usb / do_scan_ble / do_connect /
do_disconnect / do_op），确保在 worker 线程执行（Qt 的 queued 连接自动跨线程）。
"""

from __future__ import annotations

import os
import sys
from typing import Any, Optional

from PySide6.QtCore import QObject, QThread, Signal, Slot

from devtool import DeviceTool
from device_model import BaseDevice, DeviceError
from comm_lib.channels import CommError, DeviceInfo


class DeviceWorker(QObject):
    """在后台线程执行设备操作的 worker。"""

    # ---- 回传信号（worker 线程 -> 主线程）----
    device_found = Signal(list)        # List[DeviceInfo]
    connected = Signal(object)         # BaseDevice
    dongle_connected = Signal(object)  # UsbComm（多联接收器通道）
    disconnected = Signal(str)         # address
    operation_result = Signal(str, object)  # (op_id, result)
    error = Signal(str, str)           # (op_id, message)

    # ---- 触发信号（主线程 -> worker 线程）----
    # 用 Signal + @Slot 确保 async_* 在 worker 线程执行（queued 自动跨线程）
    do_scan_usb = Signal()
    do_scan_ble = Signal(float)
    do_scan_ble_bonded = Signal()
    do_connect = Signal(object)        # DeviceInfo
    do_connect_dongle = Signal(object)  # DeviceInfo（多联接收器通道）
    do_disconnect = Signal(str)
    do_op = Signal(str, str, tuple, dict)  # (op_id, address, args, kwargs)

    def __init__(self, tool: Optional[DeviceTool] = None) -> None:
        super().__init__()
        self._tool: Optional[DeviceTool] = tool
        # 连接触发信号到执行槽（queued 连接自动跨线程）
        self.do_scan_usb.connect(self._exec_scan_usb)
        self.do_scan_ble.connect(self._exec_scan_ble)
        self.do_scan_ble_bonded.connect(self._exec_scan_ble_bonded)
        self.do_connect.connect(self._exec_connect)
        self.do_connect_dongle.connect(self._exec_connect_dongle)
        self.do_disconnect.connect(self._exec_disconnect)
        self.do_op.connect(self._exec_op)

    def set_tool(self, tool: DeviceTool) -> None:
        self._tool = tool

    @property
    def tool(self) -> DeviceTool:
        if self._tool is None:
            self._tool = DeviceTool()
        return self._tool

    # ---- 主线程调用的便捷方法（emit 触发信号）----
    def async_scan_usb(self, op_id: str = "scan_usb") -> None:
        self.do_scan_usb.emit()

    def async_scan_ble(self, timeout: float = 5.0, op_id: str = "scan_ble") -> None:
        self.do_scan_ble.emit(timeout)

    def async_scan_ble_bonded(self, op_id: str = "scan_ble_bonded") -> None:
        self.do_scan_ble_bonded.emit()

    def async_connect(self, info: DeviceInfo, op_id: str = "connect") -> None:
        self.do_connect.emit(info)

    def async_connect_dongle(self, info: DeviceInfo) -> None:
        """连接多联接收器（只建通道，不作为子设备）。"""
        self.do_connect_dongle.emit(info)

    def async_connect_ble_bonded(self, mac: str, name: str = "", device_type: int = 0x32,
                                 op_id: str = "connect_ble") -> None:
        """连接已配对 BLE 设备（走 WinrtBleComm，复用系统 bonding）。"""
        import threading
        def _do():
            try:
                dev = self.tool.connect_ble_bonded(mac, name=name, device_type=device_type)
                self.connected.emit(dev)
            except Exception as e:
                self.error.emit(op_id, f"BLE 连接失败: {e}")
        threading.Thread(target=_do, daemon=True).start()

    def async_disconnect(self, address: str, op_id: str = "disconnect") -> None:
        self.do_disconnect.emit(address)

    def async_op(self, op_id: str, address: str,
                 method_name: str, *args, **kwargs) -> None:
        self.do_op.emit(op_id, address, args, {"_method": method_name, "_kw": kwargs})

    # ---- worker 线程执行的槽 ----
    @Slot()
    def _exec_scan_usb(self) -> None:
        try:
            infos = self.tool.scan_usb()
            self.device_found.emit(infos)
        except Exception as e:
            self.error.emit("scan_usb", f"扫描 USB 失败: {e}")

    @Slot(float)
    def _exec_scan_ble(self, timeout: float) -> None:
        # BLE 扫描内部用 asyncio.run，在 QThread 的事件循环里可能冲突，
        # 放到独立线程执行，结果通过 signal 回传（线程安全的 queued emit）。
        import threading
        def _do():
            try:
                infos = self.tool.scan_ble(timeout)
                self.device_found.emit(infos)
            except Exception as e:
                self.error.emit("scan_ble", f"扫描 BLE 失败: {e}")
        threading.Thread(target=_do, daemon=True).start()

    @Slot()
    def _exec_scan_ble_bonded(self) -> None:
        """列出已配对 BLE 设备（用 PowerShell，同步快，不需要独立线程）。"""
        try:
            infos = self.tool.scan_ble_bonded()
            self.device_found.emit(infos)
        except Exception as e:
            self.error.emit("scan_ble_bonded", f"扫描已配对 BLE 失败: {e}")

    @Slot(object)
    def _exec_connect(self, info: DeviceInfo) -> None:
        """执行连接。

        USB 设备在 worker 线程串行处理（claim interface 不能并行）；
        BLE 设备在独立线程并行（互不干扰）。
        """
        if info.transport == "ble":
            self._exec_connect_async(info)
        else:
            self._exec_connect_sync(info)

    def _exec_connect_async(self, info: DeviceInfo) -> None:
        """BLE 连接：独立线程并行执行。"""
        import threading

        def _do():
            try:
                dev = self.tool.connect(info)
                if dev is None:
                    self.error.emit("connect", "接收器未连接子设备")
                    return
                self.connected.emit(dev)
            except Exception as e:
                self.error.emit("connect", f"连接失败: {e}")

        threading.Thread(target=_do, daemon=True).start()

    def _exec_connect_sync(self, info: DeviceInfo) -> None:
        """USB 连接：在 worker 线程串行执行（避免 claim interface 冲突）。"""
        try:
            dev = self.tool.connect(info)
            if dev is None:
                self.error.emit("connect", "接收器未连接子设备")
                return
            self.connected.emit(dev)
        except Exception as e:
            self.error.emit("connect", f"连接失败: {e}")

    @Slot(str)
    def _exec_disconnect(self, address: str) -> None:
        try:
            ok = self.tool.disconnect(address)
            if ok:
                self.disconnected.emit(address)
        except Exception as e:
            self.error.emit("disconnect", f"断开失败: {e}")

    @Slot(object)
    def _exec_connect_dongle(self, info: DeviceInfo) -> None:
        try:
            dongle = self.tool.connect_dongle_only(info)
            self.dongle_connected.emit(dongle)
        except Exception as e:
            self.error.emit("connect_dongle", f"连接多联接收器失败: {e}")

    @Slot(str, str, tuple, dict)
    def _exec_op(self, op_id: str, address: str, args: tuple, payload: dict) -> None:
        method_name = payload.get("_method", op_id)
        kwargs = payload.get("_kw", {})
        try:
            dev = self.tool.get_device(address)
            if dev is None:
                self.error.emit(op_id, f"设备未连接: {address}")
                return
            fn = getattr(dev, method_name, None)
            if fn is None or not callable(fn):
                self.error.emit(op_id, f"不支持的方法: {method_name}")
                return
            result = fn(*args, **kwargs)
            self.operation_result.emit(op_id, result)
        except (DeviceError, CommError) as e:
            self.error.emit(op_id, f"{method_name} 失败: {e}")
        except Exception as e:
            self.error.emit(op_id, f"{method_name} 异常: {e}")


class DeviceWorkerPool(QObject):
    """按桶并行的 worker 池：同桶内串行，跨桶并行（多设备场景核心优化）。

    桶划分：
      - "connect"  连接/断开/接收器通道建立，全局串行：
        某个连接慢（如空载接收器探针超时）不会阻塞已连设备的读写
      - "scan"     USB/BLE 扫描
      - 设备地址   每个设备一桶，读写操作互不阻塞；
        多联接收器子设备共享 dongle 通道，统一归 "__dongle__" 桶串行
    """

    device_found = Signal(list)
    connected = Signal(object)
    dongle_connected = Signal(object)
    disconnected = Signal(str)
    operation_result = Signal(str, object)
    error = Signal(str, str)

    def __init__(self, tool: Optional[DeviceTool] = None,
                 parent: Optional[QObject] = None) -> None:
        super().__init__(parent)
        self._tool = tool
        self._buckets = {}  # key -> (QThread, DeviceWorker)
        for key in ("connect", "scan"):
            self._ensure_bucket(key)

    # ---- 桶管理 ----
    def _ensure_bucket(self, key: str) -> DeviceWorker:
        if key in self._buckets:
            return self._buckets[key][1]
        thread = QThread(self)
        worker = DeviceWorker(self._tool)
        worker.moveToThread(thread)
        # 桶内 worker 的信号统一汇聚到池信号
        worker.device_found.connect(self.device_found)
        worker.connected.connect(self.connected)
        worker.dongle_connected.connect(self.dongle_connected)
        worker.disconnected.connect(self.disconnected)
        worker.operation_result.connect(self.operation_result)
        worker.error.connect(self.error)
        thread.start()
        self._buckets[key] = (thread, worker)
        return worker

    def _bucket_key_for_address(self, address: str) -> str:
        if not address:
            return "misc"
        try:
            dev = self.tool.get_device(address)
        except Exception:
            dev = None
        if dev is not None and getattr(dev, "connection_type", "") == "usb_dongle":
            return "__dongle__"  # 子设备共享 dongle 通道，必须串行
        return address

    @property
    def tool(self) -> DeviceTool:
        if self._tool is None:
            self._tool = DeviceTool()
        return self._tool

    def set_tool(self, tool: DeviceTool) -> None:
        self._tool = tool
        for _thread, worker in self._buckets.values():
            worker.set_tool(tool)

    def shutdown(self) -> None:
        for thread, _worker in self._buckets.values():
            thread.quit()
            thread.wait(2000)

    # ---- 与 DeviceWorker 相同的调用面（按桶路由）----
    def async_scan_usb(self, op_id: str = "scan_usb") -> None:
        self._ensure_bucket("scan").async_scan_usb(op_id)

    def async_scan_ble(self, timeout: float = 5.0, op_id: str = "scan_ble") -> None:
        self._ensure_bucket("scan").async_scan_ble(timeout, op_id)

    def async_scan_ble_bonded(self, op_id: str = "scan_ble_bonded") -> None:
        self._ensure_bucket("scan").async_scan_ble_bonded(op_id)

    def async_connect(self, info: DeviceInfo, op_id: str = "connect") -> None:
        self._ensure_bucket("connect").async_connect(info, op_id)

    def async_connect_dongle(self, info: DeviceInfo) -> None:
        self._ensure_bucket("connect").async_connect_dongle(info)

    def async_connect_ble_bonded(self, mac: str, name: str = "",
                                 device_type: int = 0x32,
                                 op_id: str = "connect_ble") -> None:
        # 内部自起线程不占队列，挂在 connect 桶的 worker 上
        self._ensure_bucket("connect").async_connect_ble_bonded(
            mac, name=name, device_type=device_type, op_id=op_id)

    def async_disconnect(self, address: str, op_id: str = "disconnect") -> None:
        self._ensure_bucket("connect").async_disconnect(address, op_id)

    def async_op(self, op_id: str, address: str,
                 method_name: str, *args, **kwargs) -> None:
        worker = self._ensure_bucket(self._bucket_key_for_address(address))
        worker.async_op(op_id, address, method_name, *args, **kwargs)


def make_worker_pool(parent: Optional[QObject] = None) -> DeviceWorkerPool:
    """创建按桶并行的 worker 池。调用方负责 shutdown()。"""
    return DeviceWorkerPool(parent=parent)
