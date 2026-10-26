"""record_test 录音压测引擎单元测试（不依赖真实硬件）。

用 FakeDevice 模拟 start/stop 应答与异常，验证：
- 循环计数与成功率计算
- 开始/停止失败、异常的分类统计
- 连续失败熔断（自动中止）
- 中途 stop() 收尾（补发 stop_record 且当次循环计入成功）
- 音频统计（含音频会话）
- 日志文件落盘

运行：python madao_device_tool/test/test_record_test.py
"""

import os
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from comm_lib.protocol.cmd import ErrCode
from device_model import DeviceError
from record_test import RecordStressTester


class FakeDevice:
    """模拟设备：可脚本化 start/stop 应答、异常与音频回调。"""

    def __init__(self):
        self.address = "04:ee"
        self.name = "AM60"
        self.start_calls = 0
        self.stop_calls = 0
        self.start_err = ErrCode.SUCCESS
        self.stop_err = ErrCode.SUCCESS
        self.start_errs = []      # 优先逐个弹出（脚本化前 N 次行为）
        self.stop_errs = []
        self.start_raise = None
        self.stop_raise = None
        self.audio_hook = None    # start_record 成功后回调（模拟设备推流）

    def start_record(self):
        self.start_calls += 1
        if self.start_raise is not None:
            raise self.start_raise
        err = self.start_errs.pop(0) if self.start_errs else self.start_err
        if err == ErrCode.SUCCESS and self.audio_hook is not None:
            # 真机是 start 后异步推流；延迟触发确保落在录音窗口内
            threading.Timer(0.02, self.audio_hook, args=(b"\x01\x02",)).start()
        return err

    def stop_record(self):
        self.stop_calls += 1
        if self.stop_raise is not None:
            raise self.stop_raise
        return self.stop_errs.pop(0) if self.stop_errs else self.stop_err


def _run(max_cycles, dev=None, events=None, **kw):
    dev = dev or FakeDevice()
    events = [] if events is None else events
    t = RecordStressTester(dev, min_record_s=0.01, max_record_s=0.01,
                           min_gap_s=0.01, max_gap_s=0.01,
                           on_event=events.append, **kw)
    t.start(max_cycles=max_cycles)
    t._thread.join(timeout=15)
    assert not t.is_running, "压测线程应已结束"
    return dev, t


def test_ok_cycles_and_success_rate():
    dev, t = _run(3)
    s = t.snapshot()
    assert s["cycles_total"] == 3
    assert s["cycles_ok"] == 3
    assert s["start_fail"] == 0 and s["stop_fail"] == 0 and s["exception_fail"] == 0
    assert s["success_rate"] == 100.0
    assert t.success_rate() == 100.0
    assert dev.start_calls == 3 and dev.stop_calls == 3
    # 日志文件落盘
    assert os.path.exists(t.log_path)
    content = open(t.log_path, encoding="utf-8").read()
    assert "CYCLE #1" in content and "压测结束" in content
    assert "成功率 100.0%" in content


def test_start_failure_classified():
    dev = FakeDevice()
    dev.start_errs = [ErrCode.FAIL]          # 第一次开始失败，其后成功
    dev, t = _run(2, dev=dev)
    s = t.snapshot()
    assert s["cycles_total"] == 2
    assert s["cycles_ok"] == 1
    assert s["start_fail"] == 1
    assert s["success_rate"] == 50.0


def test_stop_failure_retried_until_success():
    """停止失败自动重试：任一次重试成功则本循环计入成功。"""
    dev = FakeDevice()
    dev.stop_errs = [ErrCode.FAIL]           # 首次失败，重试成功
    events = []
    dev, t = _run(1, dev=dev, events=events)
    s = t.snapshot()
    assert s["cycles_total"] == 1
    assert s["cycles_ok"] == 1               # 重试成功 -> 本循环成功
    assert s["stop_fail"] == 0
    assert dev.stop_calls == 2               # 首次 + 1 次重试
    assert any("重试成功" in e for e in events)


def test_stop_failure_retries_exhausted():
    """重试全部耗尽才判失败；失败只计 1 次（按循环而非按尝试）。"""
    dev = FakeDevice()
    dev.stop_errs = [ErrCode.FAIL, ErrCode.FAIL, ErrCode.FAIL]  # 3 次尝试全失败
    events = []
    dev, t = _run(1, dev=dev, events=events, stop_retries=2)
    s = t.snapshot()
    assert s["cycles_total"] == 1
    assert s["cycles_ok"] == 0
    assert s["stop_fail"] == 1
    assert dev.stop_calls == 3               # 1 + stop_retries 次尝试
    assert any("尝试 3/3" in e for e in events)


def test_stop_failure_classified_no_retry():
    """stop_retries=0（旧行为）：停止失败直接计失败。"""
    dev = FakeDevice()
    dev.stop_errs = [ErrCode.FAIL]
    dev, t = _run(2, dev=dev, stop_retries=0)
    s = t.snapshot()
    assert s["cycles_total"] == 2
    assert s["cycles_ok"] == 1
    assert s["stop_fail"] == 1
    assert s["start_fail"] == 0


def test_exception_counts_and_abort():
    dev = FakeDevice()
    dev.start_raise = DeviceError("设备未连接: 04:ee")
    events = []
    dev, t = _run(10, dev=dev, events=events, max_consecutive_failures=2)
    s = t.snapshot()
    assert s["aborted"], "连续失败应触发熔断"
    assert "连续失败" in s["abort_reason"]
    assert s["exception_fail"] == 2
    assert s["cycles_total"] == 2, "熔断后不应继续循环"
    assert dev.start_calls == 2
    assert any("中止" in e for e in events)


def test_stop_mid_cycle_finishes_cleanly():
    dev = FakeDevice()
    events = []
    t = RecordStressTester(dev, min_record_s=5.0, max_record_s=5.0,
                           min_gap_s=0.01, max_gap_s=0.01,
                           on_event=events.append)
    t.start(max_cycles=1)
    deadline = time.monotonic() + 3
    while dev.start_calls == 0 and time.monotonic() < deadline:
        time.sleep(0.01)
    assert dev.start_calls == 1, "应已进入录音会话"
    t.stop()
    t._thread.join(timeout=10)
    assert not t.is_running
    s = t.snapshot()
    # 中途停止：当前循环正常收尾（补发 stop 且计入成功）
    assert dev.stop_calls == 1
    assert s["cycles_total"] == 1 and s["cycles_ok"] == 1
    assert s["success_rate"] == 100.0


def test_audio_stats():
    # 会话外音频：只进总量，不算会话（不 start，避免计数被重置）
    dev0 = FakeDevice()
    t0 = RecordStressTester(dev0, min_record_s=0.1, max_record_s=0.1,
                            min_gap_s=0.01, max_gap_s=0.01)
    t0.note_audio(3, 1, b"abc")
    t0.note_audio(3, 1, b"de")
    s0 = t0.snapshot()
    assert s0["audio_bytes_total"] == 5 and s0["audio_events"] == 2
    assert s0["sessions_with_audio"] == 0

    # 会话内音频：dev.start_record 时通过 hook 模拟设备推流（落在录音窗口内）
    dev = FakeDevice()
    t = RecordStressTester(dev, min_record_s=0.1, max_record_s=0.1,
                           min_gap_s=0.01, max_gap_s=0.01)
    dev.audio_hook = lambda data: t.note_audio(3, 1, data)
    t.start(max_cycles=1)
    t._thread.join(timeout=15)
    s = t.snapshot()
    assert s["audio_bytes_total"] == 2
    assert s["audio_events"] == 1
    assert s["sessions_with_audio"] == 1


def test_duration_limit_ends_run():
    dev = FakeDevice()
    t = RecordStressTester(dev, min_record_s=0.01, max_record_s=0.01,
                           min_gap_s=0.01, max_gap_s=0.01)
    t.start(duration_s=0.05)
    t._thread.join(timeout=10)
    assert not t.is_running
    s = t.snapshot()
    assert s["cycles_total"] >= 1
    assert dev.stop_calls == s["cycles_total"]


# ---------------------------------------------------------------------------
# GUI 冒烟（需要 PySide6，缺失则跳过）
# ---------------------------------------------------------------------------

def test_dialog_smoke():
    os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
    try:
        from PySide6.QtWidgets import QApplication
    except ImportError:
        print("SKIP: PySide6 未安装，跳过 GUI 用例")
        return
    gui_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "gui")
    if gui_dir not in sys.path:
        sys.path.insert(0, gui_dir)

    app = QApplication.instance() or QApplication([])
    from record_test_dialog import RecordTestDialog

    dev = FakeDevice()
    sentinel = object()
    dev.on_audio = sentinel
    dlg = RecordTestDialog(dev)
    try:
        assert dlg.btn_start.text() == "开始压测"
        assert not dlg.btn_stop.isEnabled()
        assert dlg.sp_rec_min.value() == 2.0 and dlg.sp_rec_max.value() == 8.0
        # 用真实引擎跑一小段（FakeDevice，极短时长），验证端到端接线
        dlg.sp_rec_min.setValue(0.01)
        dlg.sp_rec_max.setValue(0.01)
        dlg.sp_gap_min.setValue(0.01)
        dlg.sp_gap_max.setValue(0.01)
        dlg.sp_max_cycles.setValue(2)
        dlg._on_start()
        assert dlg._tester is not None
        dlg._tester._thread.join(timeout=15)
        for _ in range(50):  # 等 QTimer 刷新状态
            app.processEvents()
            if not dlg._tester.is_running:
                break
            time.sleep(0.05)
        s = dlg._tester.snapshot()
        assert s["cycles_total"] == 2 and s["cycles_ok"] == 2
        # 音频挂钩：dev.on_audio 已被替换且转发原回调
        assert dev.on_audio is not dlg._prev_on_audio
        dlg._on_audio(3, 1, b"xy")
        assert dlg._tester.snapshot()["audio_bytes_total"] == 2
        assert dev.on_audio is not sentinel  # 仍处于挂钩状态
    finally:
        dlg.close()
        app.processEvents()
    assert dev.on_audio is sentinel, "关闭对话框应恢复原 on_audio"


def _run_all():
    fns = [v for k, v in sorted(globals().items()) if k.startswith("test_")]
    failed = 0
    for fn in fns:
        try:
            fn()
            print(f"PASS: {fn.__name__}")
        except AssertionError as e:
            failed += 1
            print(f"FAIL: {fn.__name__}: {e}")
        except Exception as e:
            failed += 1
            print(f"ERROR: {fn.__name__}: {type(e).__name__}: {e}")
    print(f"\n{'=' * 40}\n通过: {len(fns) - failed}, 失败: {failed}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(_run_all())
