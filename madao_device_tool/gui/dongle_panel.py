"""多联接收器管理面板（内嵌 QWidget 版）。

由 MultiLinkDialog 改造而来，作为主窗口右侧详情区的面板之一。
功能：显示 dongle 已挂载子设备、扫描周围 BLE 子设备、连接/断开、手动连接。

底层使用 UsbComm 的 dongle API：
- dongle_scan_subdevices(scan_ms, rssi)
- dongle_query_connected()
- attach_subdevice(mac) / detach_subdevice(mac)
扫描结果通过 on_device_found 回调上抛。
"""

from __future__ import annotations

import threading
import time
from typing import Optional

from PySide6.QtCore import Qt, Signal, QTimer
from PySide6.QtWidgets import (
    QFormLayout,
    QGroupBox,
    QHBoxLayout,
    QInputDialog,
    QLabel,
    QMessageBox,
    QPushButton,
    QSpinBox,
    QTreeWidget,
    QTreeWidgetItem,
    QVBoxLayout,
    QWidget,
)

from comm_lib.channels.usb_comm import UsbComm


def _format_mac(mac_bytes: bytes) -> str:
    if mac_bytes is None:
        return "N/A"
    return ":".join(f"{b:02x}" for b in mac_bytes)


def _parse_mac(mac_str: str) -> Optional[bytes]:
    s = mac_str.strip().lower().replace(":", "").replace("-", "").replace(".", "")
    if len(s) != 12:
        return None
    try:
        return bytes.fromhex(s)
    except ValueError:
        return None


class DonglePanel(QWidget):
    """多联接收器管理面板。

    Args:
        dongle: 已连接的 UsbComm（DONGLE 模式）实例
        parent: 父窗口
    """

    _device_found = Signal(object)  # UsbDeviceInfo
    # 子设备上线（mac bytes, 名称 str）：GUI 据此把子设备加进设备列表
    sub_device_connected = Signal(object, str)
    # 子设备离线（mac bytes）
    sub_device_disconnected = Signal(object)

    def __init__(self, dongle: UsbComm, parent: Optional[QWidget] = None) -> None:
        super().__init__(parent)
        self._dongle = dongle
        self._scanning = False
        self._scan_results: list = []
        self._connected_macs: list[bytes] = []

        self._build_ui()

        # 拦截 dongle 的扫描/设备发现回调
        self._dongle.on_device_found = self._on_device_found_raw
        # 启动定时刷新
        self._timer = QTimer(self)
        self._timer.timeout.connect(self._refresh)
        self._timer.start(2000)
        self._refresh()

    def _build_ui(self) -> None:
        root = QVBoxLayout(self)

        # dongle 信息
        info_group = QGroupBox("多联接收器")
        info_layout = QFormLayout(info_group)
        self.lbl_dongle_status = QLabel("已连接")
        info_layout.addRow("状态:", self.lbl_dongle_status)
        root.addWidget(info_group)

        # 上半：子设备列表 + 扫描结果
        top = QHBoxLayout()

        # 左：已挂载子设备
        sub_group = QGroupBox("已挂载子设备")
        sub_layout = QVBoxLayout(sub_group)
        self.tree_sub = QTreeWidget()
        self.tree_sub.setHeaderLabels(["MAC 地址", "状态"])
        self.tree_sub.setColumnWidth(0, 160)
        sub_layout.addWidget(self.tree_sub)
        btn_sub_row = QHBoxLayout()
        self.btn_disconnect = QPushButton("断开选中")
        self.btn_disconnect.clicked.connect(self._disconnect_selected)
        self.btn_clear_all = QPushButton("清除所有")
        self.btn_clear_all.clicked.connect(self._clear_all)
        btn_sub_row.addWidget(self.btn_disconnect)
        btn_sub_row.addWidget(self.btn_clear_all)
        btn_sub_row.addStretch()
        sub_layout.addLayout(btn_sub_row)
        top.addWidget(sub_group, 1)

        # 右：扫描结果
        scan_group = QGroupBox("扫描结果（双击连接）")
        scan_layout = QVBoxLayout(scan_group)
        self.tree_scan = QTreeWidget()
        self.tree_scan.setHeaderLabels(["MAC 地址", "名称", "RSSI"])
        self.tree_scan.setColumnWidth(0, 160)
        self.tree_scan.setColumnWidth(1, 200)
        self.tree_scan.itemDoubleClicked.connect(self._on_scan_double_click)
        scan_layout.addWidget(self.tree_scan)
        self.lbl_scan_status = QLabel("")
        self.lbl_scan_status.setStyleSheet("color: blue;")
        scan_layout.addWidget(self.lbl_scan_status)
        top.addWidget(scan_group, 1)

        root.addLayout(top, 2)

        # 扫描参数
        param_row = QHBoxLayout()
        param_row.addWidget(QLabel("扫描时间(ms):"))
        self.sp_scan_ms = QSpinBox()
        self.sp_scan_ms.setRange(100, 60000)
        self.sp_scan_ms.setValue(6000)
        param_row.addWidget(self.sp_scan_ms)
        param_row.addWidget(QLabel("RSSI 阈值:"))
        self.sp_rssi = QSpinBox()
        self.sp_rssi.setRange(0, 127)
        self.sp_rssi.setValue(60)
        param_row.addWidget(self.sp_rssi)
        param_row.addStretch()
        root.addLayout(param_row)

        # 操作按钮
        btn_row = QHBoxLayout()
        self.btn_scan = QPushButton("开始扫描")
        self.btn_scan.clicked.connect(self._start_scan)
        self.btn_manual = QPushButton("手动连接")
        self.btn_manual.clicked.connect(self._manual_connect)
        self.btn_refresh = QPushButton("刷新")
        self.btn_refresh.clicked.connect(self._refresh)
        btn_row.addWidget(self.btn_scan)
        btn_row.addWidget(self.btn_manual)
        btn_row.addWidget(self.btn_refresh)
        btn_row.addStretch()
        root.addLayout(btn_row)

        # 信号连接
        self._device_found.connect(self._on_device_found_main)

    # ---- 刷新 ----
    def _refresh(self) -> None:
        try:
            self._dongle.dongle_query_connected()
        except Exception:
            pass
        # _sub_macs 由 usb_comm 在收到 CONNECTED/DISCONNECT 事件时同步维护，
        # 这里只读快照刷新树（不覆盖本地推断状态）。
        self._connected_macs = list(getattr(self._dongle, "_sub_macs", []))
        self._refresh_sub_tree()

    def _refresh_sub_tree(self) -> None:
        self.tree_sub.clear()
        for mac in self._connected_macs:
            item = QTreeWidgetItem([_format_mac(mac), "已连接"])
            item.setData(0, Qt.UserRole, mac)
            self.tree_sub.addTopLevelItem(item)

    # ---- 扫描 ----
    def _start_scan(self) -> None:
        if self._scanning:
            QMessageBox.information(self, "提示", "正在扫描中，请稍候...")
            return
        scan_ms = self.sp_scan_ms.value()
        rssi = self.sp_rssi.value()
        self.tree_scan.clear()
        self._scan_results.clear()
        self._scanning = True
        self.btn_scan.setEnabled(False)
        self.lbl_scan_status.setText(f"扫描中... ({scan_ms / 1000:.1f}s)")

        def scan_thread():
            try:
                self._dongle.dongle_scan_subdevices(scan_ms, rssi)
                time.sleep(scan_ms / 1000)
            except Exception as e:
                import traceback
                tb = traceback.format_exc()
                self._last_scan_error = f"{type(e).__name__}: {e}\n{tb}"
                self._device_found.emit(None)
            finally:
                self._scanning = False

        threading.Thread(target=scan_thread, daemon=True).start()
        QTimer.singleShot(scan_ms + 500, self._on_scan_done)

    def _on_scan_done(self) -> None:
        self.btn_scan.setEnabled(True)
        count = self.tree_scan.topLevelItemCount()
        if count > 0:
            self.lbl_scan_status.setText(f"扫描完成，找到 {count} 个设备")
        else:
            self.lbl_scan_status.setText("扫描完成，未发现新设备")

    # ---- 设备发现回调（dongle RX 线程 -> 主线程）----
    def _on_device_found_raw(self, info) -> None:
        self._device_found.emit(info)

    def _on_device_found_main(self, info) -> None:
        if info is None:
            err = getattr(self, "_last_scan_error", "未知错误")
            self.lbl_scan_status.setText("扫描出错（详见弹窗）")
            self.lbl_scan_status.setStyleSheet("color: red;")
            QMessageBox.critical(self, "扫描失败", err)
            return
        mac = info.extra.get("mac") if info.extra else None
        if mac is None:
            return
        # 已挂载子设备事件（GET_CONNECTED_DEVICE 查询 / 真实上线）：
        # 与扫描结果（SCAN_DEVICE cmd）区分——更新"已挂载子设备"树并通知 GUI 连接。
        if info.extra.get("dongle_state") is not None:
            self._handle_sub_event(mac, info.extra["dongle_state"])
            return
        rssi = info.extra.get("rssi", 0) if info.extra else 0
        name = info.name or f"scan-{mac.hex()}"
        for i in range(self.tree_scan.topLevelItemCount()):
            existing = self.tree_scan.topLevelItem(i).data(0, Qt.UserRole)
            if existing == mac:
                self.tree_scan.topLevelItem(i).setText(2, str(rssi))
                return
        item = QTreeWidgetItem([_format_mac(mac), name, str(rssi)])
        item.setData(0, Qt.UserRole, mac)
        self.tree_scan.addTopLevelItem(item)
        self.lbl_scan_status.setText(f"发现设备: {_format_mac(mac)}")

    def _handle_sub_event(self, mac: bytes, state: int) -> None:
        """处理已挂载子设备事件：state=1 已连接，其它=已配对未连接。"""
        if state == 1:
            if mac not in self._connected_macs:
                self._connected_macs.append(mac)
            self._refresh_sub_tree()
            self.sub_device_connected.emit(mac, f"sub-{mac.hex(':')}")
        else:
            if mac in self._connected_macs:
                self._connected_macs = [m for m in self._connected_macs if m != mac]
                self._refresh_sub_tree()
                self.sub_device_disconnected.emit(mac)

    # ---- 连接/断开 ----
    def _on_scan_double_click(self, item, column) -> None:
        self._connect_mac(item.data(0, Qt.UserRole))

    def _connect_mac(self, mac: bytes) -> None:
        if mac is None:
            return
        try:
            self._dongle.attach_subdevice(mac)
        except Exception as e:
            QMessageBox.warning(self, "错误", f"连接请求失败: {e}")
            return
        QMessageBox.information(self, "提示", f"连接请求已发送\nMAC: {_format_mac(mac)}")
        QTimer.singleShot(2000, self._refresh)

    def _disconnect_selected(self) -> None:
        item = self.tree_sub.currentItem()
        if item is None:
            QMessageBox.warning(self, "警告", "请先选择要断开的子设备")
            return
        mac = item.data(0, Qt.UserRole)
        if mac is None:
            return
        reply = QMessageBox.question(
            self, "确认", f"确定要断开子设备?\nMAC: {_format_mac(mac)}",
            QMessageBox.Yes | QMessageBox.No, QMessageBox.No)
        if reply != QMessageBox.Yes:
            return
        try:
            self._dongle.detach_subdevice(mac)
        except Exception as e:
            QMessageBox.warning(self, "错误", f"断开请求失败: {e}")
            return
        QMessageBox.information(self, "提示", "断开请求已发送")
        QTimer.singleShot(1000, self._refresh)

    def _clear_all(self) -> None:
        if not self._connected_macs:
            QMessageBox.information(self, "提示", "没有已连接的子设备")
            return
        reply = QMessageBox.question(
            self, "确认",
            f"确定要断开所有 {len(self._connected_macs)} 个子设备吗?",
            QMessageBox.Yes | QMessageBox.No, QMessageBox.No)
        if reply != QMessageBox.Yes:
            return
        for mac in list(self._connected_macs):
            try:
                self._dongle.detach_subdevice(mac)
            except Exception:
                pass
        QMessageBox.information(self, "提示", "清除请求已发送")
        QTimer.singleShot(2000, self._refresh)

    def _manual_connect(self) -> None:
        mac_str, ok = QInputDialog.getText(
            self, "手动连接设备", "请输入设备 MAC 地址:\n(格式: aa:bb:cc:dd:ee:ff)")
        if not ok or not mac_str:
            return
        mac = _parse_mac(mac_str)
        if mac is None:
            QMessageBox.warning(self, "错误", "MAC 地址格式错误\n支持格式: aa:bb:cc:dd:ee:ff")
            return
        self._connect_mac(mac)

    # ---- 清理 ----
    def teardown(self) -> None:
        """面板被移除前调用，停止定时器。"""
        self._timer.stop()
