"""OTA 固件升级对话框（PySide6）。

功能：
- 选择明文固件文件
- 自动鉴权（若未鉴权）
- 后台线程执行 perform_ota，进度条 + 日志实时更新
- "结束升级"按钮：中途取消时通知设备终止（end_ota enable=0）
- 升级结束后提示成功/失败

注意：OTA 有变砖风险。前置条件是设备已完成鉴权（对话框会自动触发鉴权）。
"""

from __future__ import annotations

import threading
from typing import Optional

from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import (
    QDialog,
    QFileDialog,
    QFormLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMessageBox,
    QProgressBar,
    QPushButton,
    QTextEdit,
    QVBoxLayout,
    QWidget,
)


class OtaDialog(QDialog):
    """固件升级对话框。

    Args:
        dev: BaseDevice 实例（需已连接）
        worker: DeviceWorker（用于异步鉴权和 OTA 操作）
        parent: 父窗口
    """

    # 进度信号（工作线程 -> 主线程）
    _progress = Signal(int, int)  # (current, total)
    _log_msg = Signal(str)
    _finished = Signal(bool, str)  # (success, message)

    def __init__(self, dev, worker, parent: Optional[QWidget] = None) -> None:
        super().__init__(parent)
        self._dev = dev
        self._worker = worker
        self._firmware_path: Optional[str] = None
        self._firmware_data: Optional[bytes] = None
        self._upgrading = False
        self._cancel_requested = False
        self._thread: Optional[threading.Thread] = None

        self.setWindowTitle("固件升级")
        self.setModal(True)
        self.resize(540, 416)
        self.setFixedSize(540, 416)

        self._build_ui()
        self._connect_signals()

    def _build_ui(self) -> None:
        root = QVBoxLayout(self)

        # 设备信息
        info = QFormLayout()
        dev_name = self._dev.name or self._dev.address
        info.addRow("设备:", QLabel(dev_name))
        pn_label = "-"
        if self._dev.product_no:
            pn_label = bytes(self._dev.product_no).decode("ascii", errors="replace").strip("\x00")
        info.addRow("Product NO:", QLabel(pn_label))
        root.addLayout(info)

        # 固件选择
        fw_group = QGroupBox("固件文件（明文 .bin）")
        fw_layout = QHBoxLayout(fw_group)
        self.le_firmware = QLineEdit()
        self.le_firmware.setReadOnly(True)
        self.le_firmware.setPlaceholderText("请选择固件文件...")
        self.btn_browse = QPushButton("浏览…")
        self.btn_browse.clicked.connect(self._on_browse)
        fw_layout.addWidget(self.le_firmware, 1)
        fw_layout.addWidget(self.btn_browse)
        root.addWidget(fw_group)

        # 进度
        prog_group = QGroupBox("升级进度")
        prog_layout = QVBoxLayout(prog_group)
        self.progressbar = QProgressBar()
        self.progressbar.setRange(0, 100)
        self.progressbar.setValue(0)
        prog_layout.addWidget(self.progressbar)
        self.lbl_status = QLabel("请选择固件文件后开始升级")
        prog_layout.addWidget(self.lbl_status)
        root.addWidget(prog_group)

        # 日志
        log_group = QGroupBox("日志")
        log_layout = QVBoxLayout(log_group)
        self.log_text = QTextEdit()
        self.log_text.setReadOnly(True)
        self.log_text.setMaximumHeight(160)
        log_layout.addWidget(self.log_text)
        root.addWidget(log_group)

        # 按钮
        btn_row = QHBoxLayout()
        self.btn_start = QPushButton("开始升级")
        self.btn_start.clicked.connect(self._on_start)
        self.btn_stop = QPushButton("结束升级")
        self.btn_stop.clicked.connect(self._on_stop)
        self.btn_stop.setEnabled(False)
        self.btn_close = QPushButton("关闭")
        self.btn_close.clicked.connect(self._on_close)
        btn_row.addWidget(self.btn_start)
        btn_row.addWidget(self.btn_stop)
        btn_row.addStretch()
        btn_row.addWidget(self.btn_close)
        root.addLayout(btn_row)

    def _connect_signals(self) -> None:
        self._progress.connect(self._on_progress)
        self._log_msg.connect(self._on_log_msg)
        self._finished.connect(self._on_finished)

    # ---- UI 操作 ----
    def _on_browse(self) -> None:
        path, _ = QFileDialog.getOpenFileName(
            self, "选择固件文件", "", "固件文件 (*.bin);;所有文件 (*.*)")
        if path:
            self._firmware_path = path
            self.le_firmware.setText(path)
            self.lbl_status.setText("固件已选择，点击\"开始升级\"")

    def _append_log(self, msg: str) -> None:
        self.log_text.append(msg)

    def _on_log_msg(self, msg: str) -> None:
        self._append_log(msg)

    def _on_progress(self, current: int, total: int) -> None:
        percent = (current / total * 100) if total > 0 else 0
        self.progressbar.setValue(int(percent))
        self.lbl_status.setText(f"升级中… {percent:.1f}% ({current}/{total})")
        if current >= total:
            self._append_log("数据传输完成，等待设备校验…")

    def _on_finished(self, success: bool, msg: str) -> None:
        self._upgrading = False
        self._cancel_requested = False
        self.btn_start.setEnabled(True)
        self.btn_stop.setEnabled(False)
        self.lbl_status.setText(msg)
        prefix = "✓ " if success else "✗ "
        self._append_log(prefix + msg)
        if success:
            self.progressbar.setValue(100)
            QMessageBox.information(self, "固件升级", msg)
        else:
            QMessageBox.warning(self, "固件升级", msg)

    # ---- 升级流程 ----
    def _on_start(self) -> None:
        if self._upgrading:
            return
        if not self._firmware_path:
            QMessageBox.warning(self, "提示", "请先选择固件文件")
            return
        try:
            with open(self._firmware_path, "rb") as f:
                self._firmware_data = f.read()
        except OSError as e:
            QMessageBox.warning(self, "错误", f"读取固件文件失败: {e}")
            return

        size = len(self._firmware_data)
        chunks = (size + 31) // 32
        self._append_log(f"固件大小: {size} 字节，预计 {chunks} 片")

        self._upgrading = True
        self._cancel_requested = False
        self.btn_start.setEnabled(False)
        self.btn_stop.setEnabled(True)
        self.progressbar.setValue(0)
        self.log_text.clear()
        self._append_log(f"固件大小: {size} 字节，预计 {chunks} 片")

        # 在后台线程执行完整 OTA 流程
        self._thread = threading.Thread(target=self._upgrade_thread, daemon=True)
        self._thread.start()

    def _on_stop(self) -> None:
        """请求结束升级。实际的 end_ota(enable=0) 由后台线程在捕获取消后发送。"""
        if not self._upgrading:
            return
        reply = QMessageBox.question(
            self, "确认结束", "确定要结束升级吗？设备将不启用新固件。",
            QMessageBox.Yes | QMessageBox.No, QMessageBox.No)
        if reply != QMessageBox.Yes:
            return
        self._cancel_requested = True
        self.btn_stop.setEnabled(False)
        self._append_log("正在结束升级…")
        self.lbl_status.setText("正在结束升级…")

    def _upgrade_thread(self) -> None:
        """后台线程：鉴权 → perform_ota。"""
        try:
            # 自动鉴权（若尚未鉴权）
            if self._dev._auth_session_key is None:
                self._log_msg.emit("正在鉴权…")
                ok = self._dev.authenticate()
                if not ok:
                    self._finished.emit(False, "鉴权失败，无法开始 OTA")
                    return
                self._log_msg.emit("鉴权成功")

            self._log_msg.emit("开始 OTA 升级…")
            self._log_msg.emit("正在传输固件…")

            def on_progress(current, total):
                self._progress.emit(current, total)

            def on_check_cancel():
                return self._cancel_requested

            success = self._dev.perform_ota(self._firmware_data,
                                            on_progress=on_progress,
                                            on_check_cancel=on_check_cancel)
            if success:
                self._finished.emit(True, "升级成功")
            else:
                self._finished.emit(False, "升级失败（设备返回错误）")
        except Exception as e:
            msg = str(e)
            if "取消" in msg:
                self._finished.emit(False, "升级已取消")
            else:
                self._finished.emit(False, f"升级异常: {msg}")

    # ---- 关闭 ----
    def _on_close(self) -> None:
        if self._upgrading:
            reply = QMessageBox.question(
                self, "确认", "升级正在进行，确定关闭？",
                QMessageBox.Yes | QMessageBox.No, QMessageBox.No)
            if reply != QMessageBox.Yes:
                return
            self._cancel_requested = True
        self.accept()
