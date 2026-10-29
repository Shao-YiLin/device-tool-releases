"""OTA 固件升级对话框（PySide6，多设备并发）。

功能：
- 一个对话框同时管理多个设备，每台设备独立进度条 / 状态 / 日志
- 每台设备可单独选择固件文件（也可一键"应用到全部"）
- 点"开始升级"后，各设备在各自的后台线程里并发升级
- 非模态窗口：升级期间主窗口仍可操作（连接新设备、发起第二批升级）

注意：OTA 有变砖风险。前置条件是设备已完成鉴权（对话框会自动触发鉴权）。
"""

from __future__ import annotations

import os
import threading
from typing import List, Optional

from PySide6.QtCore import QObject, Signal
from PySide6.QtWidgets import (
    QCheckBox,
    QDialog,
    QFileDialog,
    QHBoxLayout,
    QHeaderView,
    QLabel,
    QLineEdit,
    QMessageBox,
    QProgressBar,
    QPushButton,
    QTableWidget,
    QTextEdit,
    QVBoxLayout,
    QWidget,
)

# 正在升级的设备 address，防止同一台设备被两个任务同时升级
_ACTIVE_LOCK = threading.Lock()
_ACTIVE_ADDRESSES = set()

# 已打开的对话框（保活，避免被 Python GC 回收导致窗口消失）
_ACTIVE_DIALOGS = set()


class _OtaTask(QObject):
    """单台设备的 OTA 任务。

    在后台线程里执行，Signal 把进度/日志/结果排队回主线程。
    信号第一个参数固定为表格行对象（行号会随设备移除而变化，
    因此用 row 对象在回调时取实时 index 定位）。
    """

    progress = Signal(object, int, int)  # (row, current, total)
    log_msg = Signal(object, str)        # (row, message)
    done = Signal(object, bool, str)     # (row, success, message)

    def __init__(self, row, dev, firmware: bytes, cancel: dict) -> None:
        super().__init__()
        self._row = row
        self._dev = dev
        self._firmware = firmware
        self._cancel = cancel

    def run(self) -> None:
        """后台线程主体：鉴权 -> perform_ota。"""
        try:
            # 自动鉴权（若尚未鉴权）
            if getattr(self._dev, "_auth_session_key", None) is None:
                self.log_msg.emit(self._row, "正在鉴权…")
                if not self._dev.authenticate():
                    self.done.emit(self._row, False, "鉴权失败，无法开始 OTA")
                    return
                self.log_msg.emit(self._row, "鉴权成功")
            self.log_msg.emit(self._row, "开始传输固件…")
            success = self._dev.perform_ota(
                self._firmware,
                on_progress=lambda cur, tot: self.progress.emit(self._row, cur, tot),
                on_check_cancel=lambda: bool(self._cancel.get("cancel")),
            )
            if success:
                self.done.emit(self._row, True, "升级成功")
            else:
                self.done.emit(self._row, False, "升级失败（设备返回错误）")
        except Exception as e:
            msg = str(e)
            if "取消" in msg:
                self.done.emit(self._row, False, "升级已取消")
            else:
                self.done.emit(self._row, False, f"升级异常: {msg}")


class _DeviceRow:
    """表格中的一行：一台设备 + 其固件选择 + 进度状态。"""

    def __init__(self, index: int, dev) -> None:
        self.index = index
        self.dev = dev
        self.firmware_path: str = ""
        self.firmware_data: Optional[bytes] = None
        self.cancel: dict = {"cancel": False}
        self.task: Optional[_OtaTask] = None
        self.thread: Optional[threading.Thread] = None
        self.finished: bool = False
        # 控件（由对话框创建）
        self.checkbox: Optional[QCheckBox] = None
        self.name_label: Optional[QLabel] = None
        self.pn_label: Optional[QLabel] = None
        self.fw_edit: Optional[QLineEdit] = None
        self.progress: Optional[QProgressBar] = None
        self.status_label: Optional[QLabel] = None

    @property
    def address(self) -> str:
        return getattr(self.dev, "address", "")

    @property
    def title(self) -> str:
        name = getattr(self.dev, "name", "") or self.address
        return f"{name} ({self.address})"


class OtaDialog(QDialog):
    """固件升级对话框（支持多台设备并发升级）。

    Args:
        devices: 待升级设备列表（BaseDevice）；传单个设备亦可
        worker: DeviceWorker（保留参数，升级走独立后台线程，不占用 worker）
        parent: 父窗口
    """

    def __init__(self, devices, worker=None, parent: Optional[QWidget] = None) -> None:
        super().__init__(parent)
        if not isinstance(devices, (list, tuple)):
            devices = [devices]
        self._worker = worker
        self._rows: List[_DeviceRow] = [_DeviceRow(i, d) for i, d in enumerate(devices)]
        self._tasks: List[_OtaTask] = []
        self._threads: List[threading.Thread] = []

        self.setWindowTitle("固件升级（支持多设备并发）")
        # 非模态：升级期间主窗口仍可操作
        self.setModal(False)
        self.resize(880, 520)

        self._build_ui()
        _ACTIVE_DIALOGS.add(self)

    # ---- UI ----
    def _build_ui(self) -> None:
        root = QVBoxLayout(self)

        self.table = QTableWidget(len(self._rows), 6)
        self.table.setHorizontalHeaderLabels(
            ["升级", "设备", "ProductNo", "固件文件", "进度", "状态"]
        )
        self.table.verticalHeader().setVisible(False)
        self.table.setEditTriggers(QTableWidget.NoEditTriggers)
        self.table.setSelectionMode(QTableWidget.NoSelection)
        header = self.table.horizontalHeader()
        header.setSectionResizeMode(0, QHeaderView.ResizeToContents)
        header.setSectionResizeMode(1, QHeaderView.Interactive)
        header.setSectionResizeMode(2, QHeaderView.ResizeToContents)
        header.setSectionResizeMode(3, QHeaderView.Stretch)
        header.setSectionResizeMode(4, QHeaderView.Fixed)
        header.setSectionResizeMode(5, QHeaderView.Interactive)
        self.table.setColumnWidth(1, 220)
        self.table.setColumnWidth(4, 160)
        self.table.setColumnWidth(5, 180)

        for row in self._rows:
            self._fill_row(row)

        root.addWidget(self.table, 3)

        # 日志
        log_group = QWidget()
        log_layout = QVBoxLayout(log_group)
        log_layout.setContentsMargins(0, 0, 0, 0)
        log_layout.addWidget(QLabel("升级日志"))
        self.log_text = QTextEdit()
        self.log_text.setReadOnly(True)
        log_layout.addWidget(self.log_text)
        root.addWidget(log_group, 2)

        # 按钮
        btn_row = QHBoxLayout()
        self.btn_apply_all = QPushButton("固件应用到全部")
        self.btn_apply_all.setToolTip("把第一台设备选中的固件文件应用到所有设备")
        self.btn_apply_all.clicked.connect(self._on_apply_to_all)
        self.btn_start = QPushButton("开始升级")
        self.btn_start.clicked.connect(self._on_start)
        self.btn_stop = QPushButton("结束全部")
        self.btn_stop.clicked.connect(self._on_stop_all)
        self.btn_stop.setEnabled(False)
        self.btn_close = QPushButton("关闭")
        self.btn_close.clicked.connect(self._on_close)
        btn_row.addWidget(self.btn_apply_all)
        btn_row.addStretch()
        btn_row.addWidget(self.btn_start)
        btn_row.addWidget(self.btn_stop)
        btn_row.addWidget(self.btn_close)
        root.addLayout(btn_row)

    def _fill_row(self, row: _DeviceRow) -> None:
        """填充一行的控件。"""
        # 0 勾选
        row.checkbox = QCheckBox()
        row.checkbox.setChecked(True)
        self.table.setCellWidget(row.index, 0, row.checkbox)

        # 1 设备名
        row.name_label = QLabel(row.title)
        self.table.setCellWidget(row.index, 1, row.name_label)

        # 2 ProductNo
        pn = "-"
        product_no = getattr(row.dev, "product_no", None)
        if product_no:
            pn = bytes(product_no).decode("ascii", errors="replace").strip("\x00")
        row.pn_label = QLabel(pn)
        self.table.setCellWidget(row.index, 2, row.pn_label)

        # 3 固件文件
        fw_widget = QWidget()
        fw_layout = QHBoxLayout(fw_widget)
        fw_layout.setContentsMargins(2, 2, 2, 2)
        row.fw_edit = QLineEdit()
        row.fw_edit.setReadOnly(True)
        row.fw_edit.setPlaceholderText("选择明文 .bin 固件…")
        btn = QPushButton("选择…")
        btn.clicked.connect(lambda _checked=False, r=row: self._on_browse(r))
        fw_layout.addWidget(row.fw_edit, 1)
        fw_layout.addWidget(btn)
        self.table.setCellWidget(row.index, 3, fw_widget)

        # 4 进度
        row.progress = QProgressBar()
        row.progress.setRange(0, 100)
        row.progress.setValue(0)
        self.table.setCellWidget(row.index, 4, row.progress)

        # 5 状态
        row.status_label = QLabel("待升级")
        self.table.setCellWidget(row.index, 5, row.status_label)

    # ---- 固件选择 ----
    def _on_browse(self, row: _DeviceRow) -> None:
        path, _ = QFileDialog.getOpenFileName(
            self, "选择固件文件", "", "固件文件 (*.bin);;所有文件 (*.*)")
        if not path:
            return
        row.firmware_path = path
        row.fw_edit.setText(path)
        if row.status_label.text() == "待升级":
            row.status_label.setText("已选择固件")

    def _on_apply_to_all(self) -> None:
        """把第一台已选固件的设备作为模板，应用到所有设备。"""
        template = next((r for r in self._rows if r.firmware_path), None)
        if template is None:
            QMessageBox.information(self, "提示", "请先给至少一台设备选择固件文件")
            return
        for row in self._rows:
            if row is template:
                continue
            row.firmware_path = template.firmware_path
            row.fw_edit.setText(template.firmware_path)
            if row.status_label.text() in ("待升级", "已选择固件"):
                row.status_label.setText("已选择固件")
        self._append_log(f"已把固件应用到全部设备: {os.path.basename(template.firmware_path)}")

    # ---- 升级流程 ----
    def _on_start(self) -> None:
        if self._running_rows():
            QMessageBox.information(self, "提示", "已有设备正在升级，请等待完成或先结束")
            return

        targets = [r for r in self._rows if r.checkbox.isChecked()]
        if not targets:
            QMessageBox.information(self, "提示", "请至少勾选一台设备")
            return

        missing = [r for r in targets if not r.firmware_path]
        if missing:
            names = "、".join(r.title for r in missing)
            QMessageBox.warning(self, "提示", f"以下设备未选择固件文件：\n{names}")
            return

        # 同一台设备不能被两个任务同时升级
        skipped = []
        started = []
        with _ACTIVE_LOCK:
            for row in targets:
                if row.address in _ACTIVE_ADDRESSES:
                    skipped.append(row.title)
                    continue
                _ACTIVE_ADDRESSES.add(row.address)
                started.append(row)
        if skipped:
            self._append_log("跳过（已在升级中）：" + "、".join(skipped))
        if not started:
            return

        for row in started:
            data = self._read_firmware(row)
            if data is None:
                with _ACTIVE_LOCK:
                    _ACTIVE_ADDRESSES.discard(row.address)
                continue
            row.firmware_data = data
            row.cancel["cancel"] = False
            row.finished = False
            row.progress.setValue(0)
            row.status_label.setText("升级中…")

            task = _OtaTask(row, row.dev, data, row.cancel)
            task.progress.connect(self._on_progress)
            task.log_msg.connect(self._on_log_msg)
            task.done.connect(self._on_done)
            row.task = task
            self._tasks.append(task)

            thread = threading.Thread(
                target=task.run, daemon=True, name=f"ota-{row.address}")
            row.thread = thread
            self._threads.append(thread)
            thread.start()

            chunks = (len(data) + 31) // 32
            self._append_log(
                f"[{row.title}] 固件 {len(data)} 字节，预计 {chunks} 片，开始升级")

        self.btn_start.setEnabled(False)
        self.btn_stop.setEnabled(True)

    def _read_firmware(self, row: _DeviceRow) -> Optional[bytes]:
        try:
            with open(row.firmware_path, "rb") as f:
                return f.read()
        except OSError as e:
            row.status_label.setText("读取固件失败")
            self._append_log(f"[{row.title}] 读取固件失败: {e}")
            return None

    def _on_stop_all(self) -> None:
        running = self._running_rows()
        if not running:
            return
        reply = QMessageBox.question(
            self, "确认结束", "确定要结束升级吗？设备将不启用新固件。",
            QMessageBox.Yes | QMessageBox.No, QMessageBox.No)
        if reply != QMessageBox.Yes:
            return
        for row in running:
            row.cancel["cancel"] = True
            row.status_label.setText("正在结束…")
        self._append_log("已请求结束全部升级")

    def _running_rows(self) -> List[_DeviceRow]:
        """正在升级的行（以任务完成标记为准，避免线程收尾瞬间状态错乱）。"""
        return [r for r in self._rows if r.task is not None and not r.finished]

    # ---- 信号回调（主线程）----
    def _on_progress(self, row, current: int, total: int) -> None:
        if row is None or row not in self._rows:
            return
        percent = (current / total * 100) if total > 0 else 0
        row.progress.setValue(int(percent))
        row.status_label.setText(f"升级中… {percent:.1f}%")

    def _on_log_msg(self, row, msg: str) -> None:
        title = row.title if row is not None else "未知设备"
        self._append_log(f"[{title}] {msg}")

    def _on_done(self, row, success: bool, msg: str) -> None:
        if row is None or row not in self._rows:
            # 设备行已被移除（断开）：仅记录日志
            self._append_log(f"[{row.title if row else '?'}] {msg}" if row else msg)
            return
        row.finished = True
        row.status_label.setText(msg)
        if success:
            row.progress.setValue(100)
        self._append_log(f"[{row.title}] {msg}")
        with _ACTIVE_LOCK:
            _ACTIVE_ADDRESSES.discard(row.address)
        if not self._running_rows():
            self.btn_start.setEnabled(True)
            self.btn_stop.setEnabled(False)

    # ---- 设备断开 ----
    def remove_device(self, address: str) -> bool:
        """设备断开：从表格中移除该设备行。

        正在升级的设备先请求取消；移除后重排行号（行号用于定位信号来源）。
        返回是否真的移除了。
        """
        row = next((r for r in self._rows if r.address == address), None)
        if row is None:
            return False
        with _ACTIVE_LOCK:
            _ACTIVE_ADDRESSES.discard(address)
        if row.task is not None and not row.finished:
            row.cancel["cancel"] = True
            self._append_log(f"[{row.title}] 设备已断开，升级终止")
        # 从后往前删表格行，避免行号偏移
        idx = self._rows.index(row)
        for r in self._rows[idx:]:
            if r.task is not None and not r.finished:
                r.cancel["cancel"] = True
        self._rows.remove(row)
        self.table.removeRow(row.index)
        # 重排行号，保持 _row_at(index) 与表格一致
        for i, r in enumerate(self._rows):
            r.index = i
        if not self._running_rows():
            self.btn_start.setEnabled(True)
            self.btn_stop.setEnabled(False)
        return True

    def _append_log(self, msg: str) -> None:
        self.log_text.append(msg)

    # ---- 关闭 ----
    def _on_close(self) -> None:
        running = self._running_rows()
        if running:
            reply = QMessageBox.question(
                self, "确认", "升级正在进行，确定关闭？\n（将通知设备终止升级）",
                QMessageBox.Yes | QMessageBox.No, QMessageBox.No)
            if reply != QMessageBox.Yes:
                return
            for row in running:
                row.cancel["cancel"] = True
        self.close()

    def closeEvent(self, event) -> None:
        for row in self._rows:
            with _ACTIVE_LOCK:
                _ACTIVE_ADDRESSES.discard(row.address)
        _ACTIVE_DIALOGS.discard(self)
        super().closeEvent(event)

    def showEvent(self, event) -> None:
        """显示时置前，避免被主窗口挡住。"""
        super().showEvent(event)
        self.raise_()
        self.activateWindow()
