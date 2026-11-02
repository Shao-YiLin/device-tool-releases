"""主窗口。

布局：顶部工具栏 + 左侧设备列表 + 右侧详情(QStackedWidget) + 底部状态栏。
设备操作通过 DeviceWorker 在后台线程执行，结果经 Signal 回调主线程。
"""

from __future__ import annotations

import logging
import time
from typing import Dict, List, Optional

from PySide6.QtCore import Qt, QTimer, Signal
from PySide6.QtGui import QAction, QIcon
from PySide6.QtWidgets import (
    QFileDialog,
    QHBoxLayout,
    QLabel,
    QListWidget,
    QListWidgetItem,
    QMainWindow,
    QMenu,
    QMenuBar,
    QMessageBox,
    QPlainTextEdit,
    QPushButton,
    QScrollArea,
    QSplitter,
    QStackedWidget,
    QStatusBar,
    QVBoxLayout,
    QWidget,
)

from comm_lib.channels import DeviceInfo
from comm_lib.channels.ble_device_enumeration import infer_device_type_from_ble_name
from device_config import is_known_device_name
from device_model import BaseDevice, KbDevice, MouseDevice
from devtool import DeviceTool
from device_worker import make_worker_pool
from keyboard_panel import KeyboardPanel
from mouse_panel import MousePanel
import record_config

logger = logging.getLogger(__name__)

_DEVICE_TYPE_NAME = {0x31: "鼠标", 0x32: "键盘"}

# 多联接收器在设备列表中的虚拟 address（dongle 本身不是 BaseDevice）
_DONGLE_ADDR = "__dongle__"


class MainWindow(QMainWindow):
    # 底层 RX/工作线程回调 → 主线程（QTimer.singleShot 在无事件循环的线程里不会触发，必须用 Signal）
    sig_passive_disconnected = Signal(str)
    sig_dongle_sub_disconnected = Signal(str)
    sig_dongle_removed = Signal(list)
    sig_update_event = Signal(str, object)  # 在线更新事件 (kind, payload)

    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("Madao 设备管理工具")
        from _bootstrap import resource_path
        self.setWindowIcon(QIcon(resource_path("assets", "app.ico")))
        self.resize(1100, 700)
        self.setMinimumSize(1100, 700)

        self._tool = DeviceTool()
        self._worker = make_worker_pool(self)
        self._worker.set_tool(self._tool)

        # address -> DeviceInfo（扫描到的，含 BLE/USB 未连接项）
        self._scanned: Dict[str, DeviceInfo] = {}
        # address -> name 来源：扫描阶段的名字（BLE 优先保留）
        self._scanned_names: Dict[str, str] = {}
        # address -> BaseDevice（已连接）
        self._connected: Dict[str, BaseDevice] = {}
        # address -> panel（已构建的详情面板）
        self._panels: Dict[str, QWidget] = {}
        self._device_seq = 0

        # 多联接收器虚拟项：address 为此常量，列表里最多一个
        self._dongle_panel: Optional[QWidget] = None
        # dongle 子设备被动掉线回调（DeviceManager -> GUI 主线程）
        self._tool._mgr.on_dongle_sub_disconnected = self._on_dongle_sub_disconnected
        self._tool._mgr.on_dongle_removed = self._on_dongle_removed
        # 设备被动断开（USB 拔出 / BLE 掉线）-> 自动刷新设备列表
        self._tool._mgr.on_device_disconnected = self._on_passive_disconnected

        self._build_ui()
        self._connect_signals()
        # 跨线程回调转投主线程（Signal 自动排队到对象所在线程）
        self.sig_passive_disconnected.connect(self._handle_passive_disconnected_ui)
        self.sig_dongle_sub_disconnected.connect(self._handle_sub_disconnected_ui)
        self.sig_dongle_removed.connect(self._handle_dongle_removed_ui)
        self.statusBar().showMessage("就绪")

    # ---- UI 构建 ----
    def _build_ui(self) -> None:
        # 菜单栏
        menu_bar = QMenuBar(self)
        self.setMenuBar(menu_bar)
        menu_record = QMenu("录音(&R)", self)
        self.act_save_record = QAction("保存录音文件", self)
        self.act_save_record.setCheckable(True)
        self.act_save_record.setChecked(record_config.is_save_enabled())
        self.act_save_record.triggered.connect(self._on_toggle_save_record)
        self.act_open_record_dir = QAction("打开录音文件夹", self)
        self.act_open_record_dir.triggered.connect(self._on_open_record_dir)
        self.act_set_record_dir = QAction("设置录音目录...", self)
        self.act_set_record_dir.triggered.connect(self._on_set_record_dir)
        menu_record.addAction(self.act_save_record)
        menu_record.addAction(self.act_open_record_dir)
        menu_record.addAction(self.act_set_record_dir)
        menu_bar.addMenu(menu_record)

        menu_tools = QMenu("工具(&T)", self)
        self.act_key_custom = QAction("鼠标按键自定义", self)
        self.act_key_custom.triggered.connect(self._open_key_custom)
        self.act_audio_convert = QAction("录音格式转换", self)
        self.act_audio_convert.triggered.connect(self._open_audio_convert)
        self.act_batch_ota = QAction("批量固件升级(&O)...", self)
        self.act_batch_ota.triggered.connect(self._open_batch_ota)
        menu_tools.addAction(self.act_key_custom)
        menu_tools.addAction(self.act_audio_convert)
        menu_tools.addAction(self.act_batch_ota)
        menu_bar.addMenu(menu_tools)

        menu_help = QMenu("帮助(&H)", self)
        self.act_check_update = QAction("检查更新...", self)
        self.act_check_update.triggered.connect(self._on_check_update)
        menu_help.addAction(self.act_check_update)
        menu_bar.addMenu(menu_help)

        central = QWidget()
        outer_layout = QVBoxLayout(central)

        # 上部：设备列表 + 详情面板（水平）
        top_widget = QWidget()
        top = QHBoxLayout(top_widget)
        top.setContentsMargins(0, 0, 0, 0)

        # 左：设备列表
        left = QVBoxLayout()
        left.addWidget(QLabel("设备列表"))
        self.lst_devices = QListWidget()
        self.lst_devices.itemDoubleClicked.connect(self._on_item_double_clicked)
        left.addWidget(self.lst_devices)
        top.addLayout(left, 1)

        # 右：详情（加滚动条，防止面板过长时撑开窗口）
        self.stack = QStackedWidget()
        self._placeholder = QLabel("未选择设备")
        self._placeholder.setAlignment(Qt.AlignCenter)
        self.stack.addWidget(self._placeholder)

        self._scroll = QScrollArea()
        self._scroll.setWidgetResizable(True)
        self._scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        self._scroll.setVerticalScrollBarPolicy(Qt.ScrollBarAsNeeded)
        self._scroll.setWidget(self.stack)
        top.addWidget(self._scroll, 5)

        # 下部：日志区
        self.log_widget = QPlainTextEdit()
        self.log_widget.setReadOnly(True)
        self.log_widget.setMaximumBlockCount(500)  # 最多保留 500 行
        self.log_widget.setMinimumHeight(160)  # 日志栏最小高度，避免完全看不到
        self.log_widget.setStyleSheet(
            "QPlainTextEdit { font-family: Consolas, 'Courier New', monospace; font-size: 12px; }"
        )

        # 用 splitter 分割上下，可拖拽调整
        splitter = QSplitter(Qt.Vertical)
        splitter.addWidget(top_widget)
        splitter.addWidget(self.log_widget)
        splitter.setStretchFactor(0, 4)  # 上部占 4/5
        splitter.setStretchFactor(1, 1)  # 日志占 1/5
        splitter.setCollapsible(1, False)  # 日志不可被完全收起
        outer_layout.addWidget(splitter)

        # 下部右下角：扫描设备 + 批量固件升级（放大按钮和文字）
        bottom_bar = QHBoxLayout()
        bottom_bar.addStretch(1)
        self.btn_scan = QPushButton("扫描设备")
        self.btn_batch_ota = QPushButton("批量固件升级")
        self.btn_batch_ota.setToolTip("对多台已连接设备并发升级固件")
        self.btn_batch_ota.clicked.connect(self._open_batch_ota)
        big_btn_style = (
            "QPushButton { font-size: 13px; padding: 5px 14px; "
            "min-height: 18px; min-width: 80px; }"
        )
        self.btn_scan.setStyleSheet(big_btn_style)
        self.btn_batch_ota.setStyleSheet(big_btn_style)
        bottom_bar.addWidget(self.btn_batch_ota)
        bottom_bar.addWidget(self.btn_scan)
        outer_layout.addLayout(bottom_bar)

        self.setCentralWidget(central)

        # 底部状态栏：左为常规消息，右侧永久固定显示实时回报率
        self.setStatusBar(QStatusBar())
        self.lbl_polling_rate = QLabel("实时回报率: 0 Hz")
        self.lbl_polling_rate.setStyleSheet(
            "QLabel { padding: 2px 8px; }"
        )
        self.statusBar().addPermanentWidget(self.lbl_polling_rate)

        # 启动实时回报率测量（独立线程 + 每秒定时刷新）
        self._init_polling_rate_monitor()

    def _connect_signals(self) -> None:
        self.btn_scan.clicked.connect(self._scan_all)

        # 录音菜单信号在 action 创建时已连

        self._worker.device_found.connect(self._on_device_found)
        self._worker.connected.connect(self._on_connected)
        self._worker.dongle_connected.connect(self._on_dongle_connected)
        self._worker.disconnected.connect(self._on_disconnected)
        self._worker.operation_result.connect(self._on_operation_result)
        self._worker.error.connect(self._on_error)

        self.lst_devices.itemClicked.connect(self._on_list_item_clicked)
        self.sig_update_event.connect(self._on_update_event)

    # ---- 在线更新事件路由 ----
    def _on_update_event(self, kind: str, payload) -> None:
        if kind == "progress":
            self._handle_update_progress(*payload)
        elif kind == "dl_done":
            self._handle_update_downloaded(payload)
        elif kind == "dl_error":
            dlg = getattr(self, "_update_progress_dlg", None)
            if dlg is not None:
                dlg.close()
                self._update_progress_dlg = None
            self._log(f"下载更新失败: {payload}", "ERROR")
            QMessageBox.warning(self, "下载失败", f"下载更新失败:\n{payload}")
        else:
            self._handle_update_event(kind, payload)

    # ---- 实时回报率监测 ----
    def _init_polling_rate_monitor(self) -> None:
        """启动 pynput 鼠标监听 + 每秒定时读取回报率。

        对齐参考项目 custom_mouse_app.py::run_mouse_listener：
        后台线程统计鼠标移动事件，状态栏右侧每秒刷新 Hz。
        """
        self._move_count = 0
        self._move_start: list = [None]  # 用 list 做可变容器避免 nonlocal
        self._pynput_listener = None

        self._rate_timer = QTimer(self)
        self._rate_timer.timeout.connect(self._update_polling_rate)
        # 延迟到事件循环启动后再创建 listener
        QTimer.singleShot(100, self._start_mouse_listener)

    def _start_mouse_listener(self) -> None:
        """创建并启动 pynput 鼠标监听（延迟调用）。"""
        try:
            from pynput import mouse

            def on_move(x, y):
                self._move_count += 1
                if self._move_start[0] is None:
                    self._move_start[0] = time.time()

            self._pynput_listener = mouse.Listener(on_move=on_move)
            self._pynput_listener.daemon = True
            self._pynput_listener.start()
            self._rate_timer.start(200)
        except Exception as e:
            logger.warning(f"无法启动回报率监测: {e}")
            self.lbl_polling_rate.setText("实时回报率: N/A")

    def _update_polling_rate(self) -> None:
        """每 200ms 读取并刷新状态栏回报率显示。

        读取累计事件数 / 经过时间，然后重置计数窗口。
        """
        count = self._move_count
        start = self._move_start[0]
        # 重置计数窗口
        self._move_count = 0
        self._move_start[0] = None

        if start is None:
            hz = 0.0
        else:
            elapsed = time.time() - start
            hz = (count / elapsed) if elapsed > 0 else 0.0
        self.lbl_polling_rate.setText(f"实时回报率: {hz:.0f} Hz")

    # ---- 工具栏动作 ----
    def _scan_all(self) -> None:
        """统一扫描入口：同时扫描 USB 直连和已配对 BLE 设备。"""
        self.statusBar().showMessage("扫描设备中...")
        self._scanned.clear()
        self._scanned_names.clear()
        self._rebuild_list()
        self._worker.async_scan_usb()
        self._worker.async_scan_ble_bonded()

    def _has_multi_dongle(self) -> bool:
        """是否已连接多联接收器（dongle）。"""
        dongle = getattr(self._tool._mgr, "_dongle", None)
        return dongle is not None and dongle.is_connected

    def _sync_dongle_panel(self) -> None:
        """根据 dongle 连接状态，创建或移除主界面的「多联接收器」虚拟项。"""
        if self._has_multi_dongle():
            if self._dongle_panel is None:
                from dongle_panel import DonglePanel
                panel = DonglePanel(self._tool._mgr._dongle, parent=self)
                # 子设备上线 -> 进设备池与设备列表（queued：跨线程安全）
                panel.sub_device_connected.connect(self._on_dongle_sub_connected)
                panel.sub_device_disconnected.connect(self._on_dongle_sub_mac_disconnected)
                self._dongle_panel = panel
                self.stack.addWidget(panel)
                self._rebuild_list()
                # 面板建好后立即查询一次已挂载子设备
                panel._refresh()
        else:
            if self._dongle_panel is not None:
                self._dongle_panel.teardown()
                self.stack.removeWidget(self._dongle_panel)
                self._dongle_panel.deleteLater()
                self._dongle_panel = None
                self._rebuild_list()
                # 若当前停在 dongle 面板，回到占位
                if self.stack.currentWidget() is None:
                    self.stack.setCurrentWidget(self._placeholder)

    def _current_device(self):
        """返回当前列表选中的已连接设备，未选中返回 None。"""
        item = self.lst_devices.currentItem()
        if item is None:
            return None
        addr = item.data(Qt.UserRole)
        return self._connected.get(addr)

    def _open_key_custom(self) -> None:
        """打开鼠标按键自定义对话框。仅鼠标设备允许。"""
        dev = self._current_device()
        if dev is None:
            QMessageBox.warning(self, "提示", "请先在设备列表中选择一个鼠标设备。")
            return
        if not isinstance(dev, MouseDevice):
            QMessageBox.warning(self, "提示", "按键自定义仅支持鼠标设备。")
            return
        from key_custom_dialog import KeyCustomDialog
        self._key_custom_dialog = KeyCustomDialog(dev, self._worker, parent=self)
        self._key_custom_dialog.exec()

    def _open_audio_convert(self) -> None:
        """打开录音格式转换对话框（OPUS -> PCM）。"""
        from audio_convert_dialog import AudioConvertDialog
        self._audio_convert_dialog = AudioConvertDialog(self)
        self._audio_convert_dialog.exec()

    def _open_batch_ota(self) -> None:
        """打开批量固件升级对话框：对所有已连接设备并发升级。

        对话框非模态，升级期间主窗口仍可操作（可继续连接新设备、
        再发起第二批升级）。
        """
        from ota_dialog import OtaDialog
        devices = list(self._connected.values())
        if not devices:
            QMessageBox.information(self, "提示", "当前没有已连接的设备。")
            return
        self._ota_dialog = OtaDialog(devices, self._worker, parent=self)
        self._ota_dialog.show()

    # ---- 在线更新 ----
    def _on_check_update(self) -> None:
        """检查 GitHub Release 更新，后台线程下载并提示重启。"""
        from updater import Updater, current_version

        self._log(f"检查更新… 当前版本 {current_version()}")
        self.act_check_update.setEnabled(False)
        up = Updater()

        import threading

        def _do():
            try:
                info = up.check()
            except Exception as e:
                self.sig_update_event.emit("error", str(e))
                return
            if info is None:
                self.sig_update_event.emit("latest", "")
                return
            self.sig_update_event.emit("found", info)

        threading.Thread(target=_do, daemon=True).start()

    def _handle_update_event(self, kind: str, payload) -> None:
        from updater import Updater
        if kind == "error":
            self._log(f"检查更新失败: {payload}", "ERROR")
            QMessageBox.warning(self, "检查更新", f"检查失败:\n{payload}")
        elif kind == "latest":
            self._log("已是最新版本")
            QMessageBox.information(self, "检查更新", "当前已是最新版本。")
        elif kind == "found":
            info = payload
            body = info.body.strip() or "（无更新说明）"
            reply = QMessageBox.question(
                self, "发现新版本",
                f"发现新版本 v{info.version}（当前 {__import__('updater').current_version()}）\n\n"
                f"更新说明:\n{body}\n\n是否下载并安装更新？",
                QMessageBox.Yes | QMessageBox.No, QMessageBox.Yes)
            if reply == QMessageBox.Yes:
                self._start_update_download(info)
        self.act_check_update.setEnabled(True)

    def _start_update_download(self, info) -> None:
        """后台下载更新包，完成后询问是否重启安装。"""
        from PySide6.QtWidgets import QProgressDialog
        from updater import Updater

        up = Updater()
        dlg = QProgressDialog("正在下载更新…", "取消", 0, 100, self)
        dlg.setWindowTitle("下载更新")
        dlg.setWindowModality(Qt.WindowModal)
        dlg.setAutoClose(False)
        dlg.show()
        self._update_progress_dlg = dlg

        import threading

        def _do():
            try:
                path = up.download(
                    info,
                    on_progress=lambda done, total:
                        self.sig_update_event.emit("progress", (done, total)),
                )
            except Exception as e:
                self.sig_update_event.emit("dl_error", str(e))
                return
            self.sig_update_event.emit("dl_done", path)

        threading.Thread(target=_do, daemon=True).start()

    def _handle_update_progress(self, done: int, total: int) -> None:
        dlg = getattr(self, "_update_progress_dlg", None)
        if dlg is not None and total > 0:
            dlg.setValue(int(done / total * 100))

    def _handle_update_downloaded(self, path: str) -> None:
        from updater import Updater
        dlg = getattr(self, "_update_progress_dlg", None)
        if dlg is not None:
            dlg.close()
            self._update_progress_dlg = None
        reply = QMessageBox.question(
            self, "下载完成",
            "更新包已下载完成。现在重启并安装更新？\n（工具将自动退出并重启）",
            QMessageBox.Yes | QMessageBox.No, QMessageBox.Yes)
        if reply == QMessageBox.Yes:
            self._log("开始替换并重启…")
            try:
                Updater().apply_and_restart(path)
            except Exception as e:
                self._log(f"更新替换失败: {e}", "ERROR")
                QMessageBox.warning(self, "更新失败", f"替换失败: {e}")

    # ---- 录音菜单 ----
    def _on_toggle_save_record(self, checked: bool) -> None:
        record_config.set_save_enabled(checked)
        self._log(f"保存录音文件：{'开启' if checked else '关闭'}")

    def _on_open_record_dir(self) -> None:
        path = record_config.get_record_dir()
        record_config.ensure_record_dir()
        self._log(f"打开录音文件夹: {path}")
        import os
        if os.path.isdir(path):
            os.startfile(path)
        else:
            self._log(f"录音文件夹不存在: {path}", "ERROR")

    def _on_set_record_dir(self) -> None:
        path = QFileDialog.getExistingDirectory(self, "选择录音保存目录", record_config.get_record_dir())
        if path:
            record_config.set_record_dir(path)
            self._log(f"录音保存目录已设置为: {path}")

    # ---- worker 信号 ----
    def _on_device_found(self, infos: List[DeviceInfo]) -> None:
        if not infos:
            # USB 设备被 claim 后 find 可能返回空，不是真的没设备
            return
        transport = infos[0].transport if infos else "?"
        for info in infos:
            self._scanned[info.address] = info
            if info.name:
                self._scanned_names[info.address] = info.name
        self._log(f"扫描完成({transport})：{len(infos)} 个设备，尝试连接...")
        for info in infos:
            addr = info.address
            if addr in self._connected:
                continue
            if info.transport == "ble":
                # 只自动连接本厂设备（按名字识别），避免把耳机等
                # 系统已配对的无关 BLE 设备也逐个尝试连接（各耗一个超时周期）
                if not is_known_device_name(info.name):
                    self._log(f"跳过非本厂 BLE 设备: {info.name or addr}")
                    continue
                dtype = infer_device_type_from_ble_name(info.name)
                self._worker.async_connect_ble_bonded(addr, name=info.name, device_type=dtype)
            else:
                # 多联接收器（device_type=2）：单独建立 dongle 通道，作为设备项显示
                if info.device_type == 2:
                    self._ensure_dongle(info)
                    continue
                self._worker.async_connect(info)

    def _ensure_dongle(self, info: DeviceInfo) -> None:
        """建立多联接收器连接并显示管理面板（若无）。走 worker 线程，不卡 UI。"""
        self._worker.async_connect_dongle(info)

    def _on_dongle_connected(self, dongle) -> None:
        self._sync_dongle_panel()
        self._log("已连接多联接收器")

    def _on_dongle_sub_connected(self, mac, name: str) -> None:
        """dongle 报告子设备上线：进设备池并刷新列表。"""
        address = bytes(mac).hex(":")
        try:
            dev = self._tool._mgr.connect_dongle_sub(bytes(mac))
        except Exception as e:
            self._log(f"连接子设备 {address} 失败: {e}", "ERROR")
            return
        if dev.address in self._connected:
            return  # 已在列表（轮询会重复上报）
        self._connected[dev.address] = dev
        if not dev.name or dev.name.startswith("sub-"):
            self._device_seq += 1
            dev.name = f"设备{self._device_seq}"
        self._build_panel_for(dev)
        self._rebuild_list()
        # 选中新设备并切换到其面板
        for i in range(self.lst_devices.count()):
            item = self.lst_devices.item(i)
            if item.data(Qt.UserRole) == dev.address:
                self.lst_devices.setCurrentRow(i)
                break
        panel = self._panels.get(dev.address)
        if panel is not None:
            self.stack.setCurrentWidget(panel)
        self._log(f"子设备已连接: {dev.name} ({dev.address})")

    def _on_dongle_sub_mac_disconnected(self, mac) -> None:
        """dongle 报告子设备掉线：从列表移除。"""
        address = bytes(mac).hex(":")
        self._on_dongle_sub_disconnected(address)

    def _on_dongle_sub_disconnected(self, address: str) -> None:
        """dongle 子设备被动掉线（DeviceManager 回调，RX 线程触发）。

        用 Signal 转投主线程执行 UI 更新（QTimer.singleShot 在无事件循环的
        RX 线程里不会触发，会导致界面不刷新）。
        """
        self.sig_dongle_sub_disconnected.emit(address)

    def _handle_sub_disconnected_ui(self, address: str) -> None:
        dev = self._connected.pop(address, None)
        self._notify_ota_device_gone(address)
        panel = self._panels.pop(address, None)
        if panel is not None:
            self.stack.removeWidget(panel)
            panel.deleteLater()
        self._rebuild_list()
        if dev is not None:
            self._log(f"子设备已断开: {dev.name} ({address})")

    def _on_dongle_removed(self, addresses: list) -> None:
        """dongle 被拔出（DeviceManager 回调，RX 线程触发）。"""
        self.sig_dongle_removed.emit(list(addresses))

    def _handle_dongle_removed_ui(self, addresses: list) -> None:
        for addr in addresses:
            self._connected.pop(addr, None)
            self._notify_ota_device_gone(addr)
            panel = self._panels.pop(addr, None)
            if panel is not None:
                if self.stack.currentWidget() is panel:
                    self.stack.setCurrentWidget(self._placeholder)
                self.stack.removeWidget(panel)
                panel.deleteLater()
        self._sync_dongle_panel()
        self._rebuild_list()
        self._log("多联接收器已拔出")

    def _on_connected(self, dev: BaseDevice) -> None:
        self._connected[dev.address] = dev
        self._scanned.pop(dev.address, None)

        # 设备命名策略：
        # - BLE：优先保留扫描阶段拿到的蓝牙名称，为空则后续由面板根据配置更新；
        # - USB：保持旧的“设备 N”兜底命名。
        if dev.transport == "ble":
            scanned_name = self._scanned_names.get(dev.address, "")
            if scanned_name and not dev.name:
                dev.name = scanned_name
        else:
            self._device_seq += 1
            if not dev.name or dev.name.startswith("USB") or dev.name.startswith("BTHLE"):
                dev.name = f"设备{self._device_seq}"

        self._build_panel_for(dev)
        import logging
        logging.getLogger(__name__).debug(
            f"_on_connected: dev={dev.address} type={type(dev).__name__} "
            f"transport={dev.transport!r} connection_type={dev.connection_type!r}"
        )
        self._rebuild_list()
        # 选中并切换面板
        for i in range(self.lst_devices.count()):
            item = self.lst_devices.item(i)
            if item.data(Qt.UserRole) == dev.address:
                self.lst_devices.setCurrentRow(i)
                break
        panel = self._panels.get(dev.address)
        if panel is not None:
            self.stack.setCurrentWidget(panel)
        self._sync_dongle_panel()
        self._log(f"已连接: {dev.name} ({dev.address})")

    def _on_disconnected(self, address: str) -> None:
        self._connected.pop(address, None)
        self._scanned_names.pop(address, None)
        self._notify_ota_device_gone(address)
        panel = self._panels.pop(address, None)
        if panel is not None:
            self.stack.removeWidget(panel)
            panel.deleteLater()
        self._rebuild_list()
        self.stack.setCurrentWidget(self._placeholder)
        self._sync_dongle_panel()
        self._log(f"已断开: {address}")

    def _on_passive_disconnected(self, address: str) -> None:
        """设备被动断开（拔出/掉线，底层线程触发）：转主线程自动刷新界面。"""
        self.sig_passive_disconnected.emit(address)

    def _handle_passive_disconnected_ui(self, address: str) -> None:
        """主线程：清理被动断开的设备并刷新列表（可重复调用，幂等）。"""
        dev = self._connected.pop(address, None)
        self._scanned_names.pop(address, None)
        self._notify_ota_device_gone(address)
        panel = self._panels.pop(address, None)
        if panel is not None:
            if self.stack.currentWidget() is panel:
                self.stack.setCurrentWidget(self._placeholder)
            self.stack.removeWidget(panel)
            panel.deleteLater()
        self._sync_dongle_panel()
        self._rebuild_list()
        if dev is not None:
            self._log(f"设备已断开: {dev.name} ({address})")

    def _notify_ota_device_gone(self, address: str) -> None:
        """设备断开：通知所有打开的 OTA 窗口移除该设备行。"""
        try:
            from ota_dialog import _ACTIVE_DIALOGS
        except Exception:
            return
        for dlg in list(_ACTIVE_DIALOGS):
            try:
                if dlg.remove_device(address):
                    self._log(f"OTA 窗口已移除断开设备: {address}")
            except Exception as e:
                self._log(f"OTA 窗口移除设备失败: {e}", "ERROR")

    def _on_operation_result(self, op_id: str, result) -> None:
        # 转发给对应的 panel（鼠标/键盘面板用 op_id 匹配回调）
        for panel in self._panels.values():
            if isinstance(panel, (MousePanel, KeyboardPanel)):
                panel.handle_result(op_id, result)
        # 转发给按键自定义对话框（如果打开）
        dlg = getattr(self, "_key_custom_dialog", None)
        if dlg is not None and "keycustom_" in op_id:
            dlg.handle_result(op_id, result)
        if op_id in ("scan_usb", "scan_ble", "connect", "disconnect"):
            self.statusBar().showMessage(f"{op_id} 完成: {result}")

    def _on_error(self, op_id: str, message: str) -> None:
        for panel in list(self._panels.values()):
            if isinstance(panel, (MousePanel, KeyboardPanel)):
                panel.handle_error(op_id, message)
        dlg = getattr(self, "_key_custom_dialog", None)
        if dlg is not None and "keycustom_" in op_id:
            dlg.handle_error(op_id, message)
        self._log(f"{op_id}: {message}", "ERROR")

    # ---- 列表交互 ----
    def _on_item_double_clicked(self, item: QListWidgetItem) -> None:
        addr = item.data(Qt.UserRole)
        if addr in self._connected:
            return
        info = self._scanned.get(addr)
        if info is None:
            return
        self.statusBar().showMessage(f"正在连接 {addr}...")
        if info.transport == "ble":
            dtype = infer_device_type_from_ble_name(info.name)
            self._worker.async_connect_ble_bonded(addr, name=info.name, device_type=dtype)
        else:
            self._worker.async_connect(info)

    def _on_list_item_clicked(self, item: QListWidgetItem) -> None:
        if item is None:
            return
        addr = item.data(Qt.UserRole)
        # 多联接收器虚拟项
        if addr == _DONGLE_ADDR:
            if self._dongle_panel is not None:
                self.stack.setCurrentWidget(self._dongle_panel)
            return
        panel = self._panels.get(addr)
        if panel is not None:
            self.stack.setCurrentWidget(panel)
        else:
            self.stack.setCurrentWidget(self._placeholder)

    # ---- 辅助 ----
    def _log(self, msg: str, level: str = "INFO") -> None:
        """写入底部日志区 + 日志文件。"""
        from datetime import datetime
        import logging
        ts = datetime.now().strftime("%H:%M:%S")
        self.log_widget.appendPlainText(f"[{ts}] [{level}] {msg}")
        # 同步写到日志文件
        logger = logging.getLogger("gui")
        getattr(logger, level.lower(), logger.info)(msg)

    def request_disconnect(self, address: str) -> None:
        """供 panel 通过 on_disconnect 回调请求断开。"""
        self._worker.async_disconnect(address)

    def _build_panel_for(self, dev: BaseDevice) -> None:
        old = self._panels.pop(dev.address, None)
        if old is not None:
            self.stack.removeWidget(old)
            old.deleteLater()
        if isinstance(dev, MouseDevice):
            panel: QWidget = MousePanel(dev, self._worker, on_disconnect=self,
                                        on_name_changed=self._refresh_list_names,
                                        on_type_mismatch=self._on_type_mismatch)
        elif isinstance(dev, KbDevice):
            panel = KeyboardPanel(dev, self._worker, on_disconnect=self,
                                  on_name_changed=self._refresh_list_names)
        else:
            panel = QLabel(f"未知设备类型: {type(dev).__name__}")
        self._panels[dev.address] = panel
        self.stack.addWidget(panel)

    def _on_type_mismatch(self, dev: BaseDevice) -> None:
        """鼠标面板发现实际是键盘时，重建为键盘面板。"""
        # 把 device 实例换成 KbDevice（保留 comm 和已读到的信息）
        from device_model import KbDevice
        old_dev = self._connected.get(dev.address)
        if old_dev is None:
            return
        new_dev = KbDevice(old_dev.comm, address=old_dev.address, name=dev.name,
                           transport=old_dev.transport)
        new_dev.product_no = old_dev.product_no
        new_dev.device_mac = old_dev.device_mac
        new_dev.device_version = old_dev.device_version
        self._connected[dev.address] = new_dev
        # 更新设备池
        with self._tool._mgr._lock:
            self._tool._mgr._devices[dev.address] = new_dev
        # 重建面板
        self._build_panel_for(new_dev)
        self._rebuild_list()
        self._log(f"设备类型切换: {dev.address} -> KbDevice")

    def _refresh_list_names(self) -> None:
        """设备名更新后刷新列表显示（保留当前选中）。"""
        self._rebuild_list()

    def _rebuild_list(self) -> None:
        self.lst_devices.blockSignals(True)
        current_addr = None
        # 记住当前选中项
        cur_item = self.lst_devices.currentItem()
        if cur_item:
            current_addr = cur_item.data(Qt.UserRole)
        self.lst_devices.clear()
        select_row = 0
        row_idx = 0
        # 多联接收器虚拟项（放在列表第一位）
        if self._dongle_panel is not None:
            item = QListWidgetItem("● 多联接收器")
            item.setData(Qt.UserRole, _DONGLE_ADDR)
            item.setForeground(Qt.darkBlue)
            self.lst_devices.addItem(item)
            if _DONGLE_ADDR == current_addr:
                select_row = row_idx
            row_idx += 1
        # 只显示已连接且协议验证成功的设备
        for addr, dev in self._connected.items():
            name = dev.name or "连接中..."
            text = f"● {name}"
            item = QListWidgetItem(text)
            item.setData(Qt.UserRole, addr)
            item.setForeground(Qt.darkGreen)
            self.lst_devices.addItem(item)
            if addr == current_addr:
                select_row = row_idx
            row_idx += 1
        self.lst_devices.blockSignals(False)
        # 恢复选中
        if self.lst_devices.count() > 0:
            self.lst_devices.setCurrentRow(select_row)

    # ---- 关闭 ----
    def closeEvent(self, event) -> None:
        try:
            # 停止回报率监测
            timer = getattr(self, "_rate_timer", None)
            if timer is not None:
                timer.stop()
            listener = getattr(self, "_pynput_listener", None)
            if listener is not None:
                listener.stop()
            if self._dongle_panel is not None:
                self._dongle_panel.teardown()
            self._worker.shutdown()
            self._tool.close()
        except Exception:
            logger.exception("关闭时异常")
        super().closeEvent(event)
