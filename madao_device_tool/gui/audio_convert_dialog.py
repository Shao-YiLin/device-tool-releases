"""录音格式转换对话框（OPUS -> PCM）。"""

from __future__ import annotations

import os
from typing import Optional

from PySide6.QtCore import Signal
from PySide6.QtWidgets import (
    QComboBox,
    QDialog,
    QFileDialog,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMessageBox,
    QPlainTextEdit,
    QPushButton,
    QVBoxLayout,
    QWidget,
)


class AudioConvertDialog(QDialog):
    """OPUS 转 PCM 对话框。

    Args:
        parent: 父窗口
    """

    # 跨线程信号：日志 / 完成（工作线程 emit，主线程接收）
    _sig_log = Signal(str)
    _sig_done = Signal(object)

    def __init__(self, parent: Optional[QWidget] = None) -> None:
        super().__init__(parent)
        self.setWindowTitle("录音格式转换")
        self.resize(520, 320)

        self._build_ui()
        self._sig_log.connect(self._log)
        self._sig_done.connect(self._on_convert_done)

    def _build_ui(self) -> None:
        root = QVBoxLayout(self)

        root.addWidget(QLabel("将录音 OPUS 文件转换为 PCM（16bit / 16kHz）"))

        # 输入文件行
        in_row = QHBoxLayout()
        in_row.addWidget(QLabel("OPUS 文件:"))
        self.le_input = QLineEdit()
        self.le_input.setReadOnly(True)
        self.le_input.setPlaceholderText("选择 .opus 文件...")
        self.btn_browse = QPushButton("浏览…")
        self.btn_browse.clicked.connect(self._on_browse)
        in_row.addWidget(self.le_input, 1)
        in_row.addWidget(self.btn_browse)
        root.addLayout(in_row)

        # 声道数
        ch_row = QHBoxLayout()
        ch_row.addWidget(QLabel("声道数:"))
        self.cmb_channels = QComboBox()
        self.cmb_channels.addItem("单声道 (1)", 1)
        self.cmb_channels.addItem("立体声 (2)", 2)
        ch_row.addWidget(self.cmb_channels)
        ch_row.addStretch()
        root.addLayout(ch_row)

        # 日志
        root.addWidget(QLabel("转换日志:"))
        self.log_text = QPlainTextEdit()
        self.log_text.setReadOnly(True)
        self.log_text.setMaximumBlockCount(200)
        root.addWidget(self.log_text, 1)

        # 按钮
        btn_row = QHBoxLayout()
        self.btn_convert = QPushButton("开始转换")
        self.btn_convert.setEnabled(False)
        self.btn_convert.clicked.connect(self._on_convert)
        self.btn_close = QPushButton("关闭")
        self.btn_close.clicked.connect(self.accept)
        btn_row.addWidget(self.btn_convert)
        btn_row.addStretch()
        btn_row.addWidget(self.btn_close)
        root.addLayout(btn_row)

    def _log(self, msg: str) -> None:
        self.log_text.appendPlainText(msg)

    def _on_browse(self) -> None:
        path, _ = QFileDialog.getOpenFileName(
            self, "选择 OPUS 文件", "", "OPUS 文件 (*.opus);;所有文件 (*.*)")
        if path:
            self.le_input.setText(path)
            self.btn_convert.setEnabled(True)

    def _on_convert(self) -> None:
        input_path = self.le_input.text().strip()
        if not input_path:
            QMessageBox.warning(self, "提示", "请先选择 OPUS 文件")
            return
        channels = self.cmb_channels.currentData()
        self.btn_convert.setEnabled(False)
        self._log(f"开始转换: {input_path} ({channels}ch)")

        # 转换在后台线程执行，日志/完成通过 Signal 回主线程
        import threading

        from audio.opus_pcm import opus_to_pcm

        def worker():
            result = opus_to_pcm(input_path, channels=channels,
                                 logger=lambda m: self._sig_log.emit(m))
            self._sig_done.emit(result)

        threading.Thread(target=worker, daemon=True).start()

    def _on_convert_done(self, output_path) -> None:
        self.btn_convert.setEnabled(True)
        if output_path:
            self._log(f"输出文件: {output_path}")
            QMessageBox.information(self, "完成", f"转换成功\n输出: {output_path}")
        else:
            QMessageBox.warning(self, "失败", "转换失败，详见日志")
