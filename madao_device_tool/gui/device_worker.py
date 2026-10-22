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
    disconnected = Signal(str)         # address
    operation_result = Signal(str, object)  # (op_id, result)
    error = Signal(str, str)           # (op_id, message)

    # ---- 触发信号（主线程 -> worker 线程）----
    # 用 Signal + @Slot 确保 async_* 在 worker 线程执行（queued 自动跨线程）
    do_scan_usb = Signal()
    do_scan_ble = Signal(float)
    do_scan_ble_bonded = Signal()
    do_connect = Signal(object)        # DeviceInfo
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


def make_worker_thread(parent: Optional[QObject] = None) -> tuple[QThread, DeviceWorker]:
    """创建后台线程 + worker，并 moveToThread。返回 (thread, worker)。

    调用方负责 thread.start() / thread.quit() + wait()。
    """
    thread = QThread(parent=parent)
    worker = DeviceWorker()
    worker.moveToThread(thread)
    thread.start()
    return thread, worker
