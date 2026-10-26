"""录音压测对话框：随机开/关录音长时间运行，实时统计成功率。

非模态窗口，压测在后台线程跑；设备对象来自面板（dev.start_record/stop_record）。
"""

import os
import sys

from PySide6.QtCore import Qt, QTimer, Signal
from PySide6.QtWidgets import (
    QDialog,
    QDoubleSpinBox,
    QFormLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QSpinBox,
    QTextEdit,
    QVBoxLayout,
)

# 包根目录（record_test 引擎在包根，gui 目录是平铺导入）
_PKG_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _PKG_ROOT not in sys.path:
    sys.path.insert(0, _PKG_ROOT)

from record_test import RecordStressTester  # noqa: E402


def _fmt_bytes(n: int) -> str:
    if n >= 1024 * 1024:
        return f"{n / 1024 / 1024:.2f} MB"
    if n >= 1024:
        return f"{n / 1024:.1f} KB"
    return f"{n} B"


class RecordTestDialog(QDialog):
    """录音压测控制台。"""

    # 压测线程事件 → 主线程（tester 的 on_event 在后台线程回调，不能直接碰 UI）
    event_received = Signal(str)

    def __init__(self, dev, parent=None):
        super().__init__(parent)
        self._dev = dev
        self._tester = None
        self._prev_on_audio = None
        self.setWindowTitle(f"录音压测 - {dev.name or dev.address}")
        self.setModal(False)
        self.resize(620, 520)

        root = QVBoxLayout(self)

        # 参数
        pg = QGroupBox("压测参数")
        form = QFormLayout(pg)
        self.sp_rec_min = QDoubleSpinBox()
        self.sp_rec_max = QDoubleSpinBox()
        for sp, val in ((self.sp_rec_min, 2.0), (self.sp_rec_max, 8.0)):
            sp.setRange(0.1, 600.0)
            sp.setValue(val)
            sp.setSuffix(" 秒")
            sp.setDecimals(1)
        self.sp_gap_min = QDoubleSpinBox()
        self.sp_gap_max = QDoubleSpinBox()
        for sp, val in ((self.sp_gap_min, 1.0), (self.sp_gap_max, 5.0)):
            sp.setRange(0.0, 600.0)
            sp.setValue(val)
            sp.setSuffix(" 秒")
            sp.setDecimals(1)
        self.sp_duration = QSpinBox()     # 0 = 不限时
        self.sp_duration.setRange(0, 100000)
        self.sp_duration.setValue(0)
        self.sp_duration.setSuffix(" 分钟")
        self.sp_max_cycles = QSpinBox()   # 0 = 不限
        self.sp_max_cycles.setRange(0, 1000000)
        self.sp_max_cycles.setValue(0)
        form.addRow("录音时长(随机范围):", self._hbox(self.sp_rec_min, self.sp_rec_max))
        form.addRow("循环间隔(随机范围):", self._hbox(self.sp_gap_min, self.sp_gap_max))
        form.addRow("测试时长(0=不限):", self.sp_duration)
        form.addRow("最大循环数(0=不限):", self.sp_max_cycles)
        root.addWidget(pg)

        # 控制
        btns = QHBoxLayout()
        self.btn_start = QPushButton("开始压测")
        self.btn_start.clicked.connect(self._on_start)
        self.btn_stop = QPushButton("停止")
        self.btn_stop.setEnabled(False)
        self.btn_stop.clicked.connect(self._on_stop)
        btns.addWidget(self.btn_start)
        btns.addWidget(self.btn_stop)
        btns.addStretch()
        root.addLayout(btns)

        # 实时统计
        self.lbl_status = QLabel("状态: 未开始")
        root.addWidget(self.lbl_status)
        self.lbl_stats = QLabel("-")
        self.lbl_stats.setTextInteractionFlags(Qt.TextSelectableByMouse)
        root.addWidget(self.lbl_stats)

        # 事件日志（只保留最近 500 行）
        self.txt_log = QTextEdit()
        self.txt_log.setReadOnly(True)
        self.txt_log.document().setMaximumBlockCount(500)
        root.addWidget(self.txt_log, 1)

        self.lbl_log_path = QLabel("日志: -")
        self.lbl_log_path.setTextInteractionFlags(Qt.TextSelectableByMouse)
        root.addWidget(self.lbl_log_path)

        self.event_received.connect(self.txt_log.append)
        self._timer = QTimer(self)
        self._timer.timeout.connect(self._refresh)
        self._timer.start(500)

    @staticmethod
    def _hbox(a, b):
        h = QHBoxLayout()
        h.addWidget(a)
        h.addWidget(QLabel("~"))
        h.addWidget(b)
        return h

    # ---- 音频挂钩：统计 + 转发原回调（面板的保存录音逻辑不受影响） ----

    def _hook_audio(self) -> None:
        if getattr(self, "_audio_hooked", False):
            return
        self._prev_on_audio = self._dev.on_audio
        self._dev.on_audio = self._on_audio
        self._audio_hooked = True

    def _unhook_audio(self) -> None:
        if not getattr(self, "_audio_hooked", False):
            return
        self._dev.on_audio = self._prev_on_audio
        self._prev_on_audio = None
        self._audio_hooked = False

    def _on_audio(self, audio_type: int, channel: int, data: bytes) -> None:
        tester = self._tester
        if tester is not None:
            tester.note_audio(audio_type, channel, data)
        prev = self._prev_on_audio
        if prev is not None:
            try:
                prev(audio_type, channel, data)
            except Exception:
                pass

    # ---- 控制 ----

    def _on_start(self) -> None:
        if self._tester is not None and self._tester.is_running:
            return
        rec_lo, rec_hi = sorted((self.sp_rec_min.value(), self.sp_rec_max.value()))
        gap_lo, gap_hi = sorted((self.sp_gap_min.value(), self.sp_gap_max.value()))
        duration_s = self.sp_duration.value() * 60 or None
        max_cycles = self.sp_max_cycles.value() or None

        self._hook_audio()
        self._tester = RecordStressTester(
            self._dev,
            min_record_s=rec_lo, max_record_s=rec_hi,
            min_gap_s=gap_lo, max_gap_s=gap_hi,
            on_event=lambda s: self.event_received.emit(s))
        self._tester.start(duration_s=duration_s, max_cycles=max_cycles)
        self.lbl_log_path.setText(f"日志: {self._tester.log_path}")
        self.txt_log.append(f"=== 开始压测（{self._dev.name or self._dev.address}）===")
        self._refresh()

    def _on_stop(self) -> None:
        if self._tester is not None:
            self._tester.stop()

    def _refresh(self) -> None:
        t = self._tester
        if t is None:
            self.lbl_status.setText("状态: 未开始")
            self.btn_start.setEnabled(True)
            self.btn_stop.setEnabled(False)
            return
        s = t.snapshot()
        if s["running"]:
            self.lbl_status.setText("状态: 运行中…")
        elif s["aborted"]:
            self.lbl_status.setText(f"状态: 已中止（{s['abort_reason']}）")
        else:
            self.lbl_status.setText("状态: 已结束")
        self.btn_start.setEnabled(not s["running"])
        self.btn_stop.setEnabled(s["running"])
        self.lbl_stats.setText(
            f"循环: {s['cycles_total']}  成功: {s['cycles_ok']}  "
            f"成功率: {s['success_rate']}%\n"
            f"失败: 开始 {s['start_fail']} / 停止 {s['stop_fail']} / 异常 {s['exception_fail']}"
            f"（连续失败 {s['consecutive_failures']}）\n"
            f"音频: {_fmt_bytes(s['audio_bytes_total'])} ({s['audio_events']} 包)  "
            f"含音频会话: {s['sessions_with_audio']}/{s['cycles_total']}")

    # ---- 关闭 ----

    def closeEvent(self, event) -> None:
        self._timer.stop()
        try:
            self.event_received.disconnect()
        except (TypeError, RuntimeError):
            pass
        if self._tester is not None and self._tester.is_running:
            self._tester.stop()  # 后台线程收尾（补发 stop_record）
        self._unhook_audio()
        super().closeEvent(event)
