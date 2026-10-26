"""录音压测引擎：随机时长开/关录音，长时间运行，统计成功率并保存日志。

接口（不依赖 GUI，设备对象只需实现 start_record/stop_record/on_audio）：

    from madao_device_tool.record_test import RecordStressTester

    tester = RecordStressTester(dev, min_record_s=2, max_record_s=8,
                                min_gap_s=1, max_gap_s=5)
    tester.start(duration_s=3600)      # 或 max_cycles=100；都不传则跑到 stop()
    ...
    tester.stop()
    print(tester.snapshot())           # 总数/成功/失败分类/成功率/音频统计
    print(tester.log_path)             # 逐循环日志文件

每个循环：start_record -> 随机时长 -> stop_record -> 随机间隔。
单循环成功定义：start/stop 应答均为 ErrCode.SUCCESS。
音频数据由通信层线程回调 note_audio() 统计（线程安全）。
连续失败超过 max_consecutive_failures 次自动中止（防止设备掉线后刷失败）。
"""

from __future__ import annotations

import logging
import os
import random
import threading
import time
from datetime import datetime
from typing import Callable, Optional

from comm_lib.protocol.cmd import ErrCode
from logging_config import get_log_dir


class RecordStressTester:
    """随机开/关录音压测。start() 后台线程运行，stop() 请求结束。"""

    def __init__(self, device,
                 min_record_s: float = 2.0, max_record_s: float = 8.0,
                 min_gap_s: float = 1.0, max_gap_s: float = 5.0,
                 max_consecutive_failures: int = 5,
                 log_path: Optional[str] = None,
                 on_event: Optional[Callable[[str], None]] = None):
        if min_record_s > max_record_s:
            min_record_s, max_record_s = max_record_s, min_record_s
        if min_gap_s > max_gap_s:
            min_gap_s, max_gap_s = max_gap_s, min_gap_s

        self._dev = device
        self._min_record_s = float(min_record_s)
        self._max_record_s = float(max_record_s)
        self._min_gap_s = float(min_gap_s)
        self._max_gap_s = float(max_gap_s)
        self._max_consecutive_failures = int(max_consecutive_failures)
        self._on_event = on_event

        self._lock = threading.Lock()
        self._stop_flag = threading.Event()
        self._thread: Optional[threading.Thread] = None
        self._recording = False      # 当前会话是否正在收音频
        self._session_audio = 0      # 当前会话累计音频字节

        # 统计（访问需持 _lock）
        self.cycles_total = 0
        self.cycles_ok = 0
        self.start_fail = 0
        self.stop_fail = 0
        self.exception_fail = 0
        self.audio_bytes_total = 0
        self.audio_events = 0
        self.sessions_with_audio = 0
        self.consecutive_failures = 0
        self.aborted = False
        self.abort_reason = ""

        self._file_logger, self.log_path = self._make_file_logger(log_path)

    # ---- 对外接口 ----

    def start(self, duration_s: Optional[float] = None,
              max_cycles: Optional[int] = None) -> None:
        """后台线程开始压测。duration_s/max_cycles 都为 None 时跑到 stop()。"""
        if self.is_running:
            raise RuntimeError("压测已在运行")
        self._stop_flag.clear()
        with self._lock:
            self.cycles_total = 0
            self.cycles_ok = 0
            self.start_fail = 0
            self.stop_fail = 0
            self.exception_fail = 0
            self.audio_bytes_total = 0
            self.audio_events = 0
            self.sessions_with_audio = 0
            self.consecutive_failures = 0
            self.aborted = False
            self.abort_reason = ""
        self._file_logger.info(
            "=== 压测启动: record %.1f~%.1fs, gap %.1f~%.1fs, duration=%s, max_cycles=%s ===",
            self._min_record_s, self._max_record_s,
            self._min_gap_s, self._max_gap_s, duration_s, max_cycles)
        self._thread = threading.Thread(
            target=self._run, args=(duration_s, max_cycles),
            name="record-stress-test", daemon=True)
        self._thread.start()

    def stop(self) -> None:
        """请求结束（当前循环会正常收尾：补发 stop_record 并计入统计）。"""
        self._stop_flag.set()

    @property
    def is_running(self) -> bool:
        return self._thread is not None and self._thread.is_alive()

    def note_audio(self, audio_type: int, channel: int, data: bytes) -> None:
        """通信层音频回调入口（RX 线程调用，线程安全）。

        GUI 挂钩方式：保存原 dev.on_audio，包一层先调本方法再转发原回调。
        """
        n = len(data)
        with self._lock:
            self.audio_events += 1
            self.audio_bytes_total += n
            if self._recording:
                self._session_audio += n

    def success_rate(self) -> float:
        """成功率（%），无循环时返回 0.0。"""
        with self._lock:
            if not self.cycles_total:
                return 0.0
            return self.cycles_ok / self.cycles_total * 100.0

    def snapshot(self) -> dict:
        """统计快照（GUI 轮询用）。"""
        with self._lock:
            rate = (self.cycles_ok / self.cycles_total * 100.0
                    if self.cycles_total else 0.0)
            return {
                "cycles_total": self.cycles_total,
                "cycles_ok": self.cycles_ok,
                "start_fail": self.start_fail,
                "stop_fail": self.stop_fail,
                "exception_fail": self.exception_fail,
                "audio_bytes_total": self.audio_bytes_total,
                "audio_events": self.audio_events,
                "sessions_with_audio": self.sessions_with_audio,
                "consecutive_failures": self.consecutive_failures,
                "aborted": self.aborted,
                "abort_reason": self.abort_reason,
                "running": self.is_running,
                "success_rate": round(rate, 1),
            }

    # ---- 内部实现 ----

    def _make_file_logger(self, log_path: Optional[str]):
        if not log_path:
            ts = datetime.now().strftime("%Y%m%d_%H%M%S")
            addr = str(getattr(self._dev, "address", "dev")).replace(":", "")
            log_path = os.path.join(get_log_dir(), f"record_test_{addr}_{ts}.log")
        os.makedirs(os.path.dirname(os.path.abspath(log_path)), exist_ok=True)
        lg = logging.getLogger(f"record_test.{id(self):x}")
        lg.setLevel(logging.DEBUG)
        lg.propagate = False  # 不进主日志（长测会刷屏）
        if not lg.handlers:
            handler = logging.FileHandler(log_path, encoding="utf-8")
            handler.setFormatter(logging.Formatter(
                "[%(asctime)s] %(message)s", datefmt="%Y-%m-%d %H:%M:%S"))
            lg.addHandler(handler)
        return lg, log_path

    def _emit(self, msg: str, *args) -> None:
        if args:
            msg = msg % args
        self._file_logger.info(msg)
        if self._on_event:
            try:
                self._on_event(msg)
            except Exception:
                pass

    def _run(self, duration_s: Optional[float], max_cycles: Optional[int]) -> None:
        deadline = (time.monotonic() + duration_s) if duration_s else None
        cycle = 0
        while not self._stop_flag.is_set():
            if deadline is not None and time.monotonic() >= deadline:
                break
            if max_cycles is not None and cycle >= max_cycles:
                break
            cycle += 1
            self._do_cycle(cycle)

        # 收尾：若停在录音中，补发 stop 防止设备一直录
        if self._recording:
            with self._lock:
                self._recording = False
            try:
                err = self._dev.stop_record()
                self._file_logger.info("收尾补发 stop_record: err=%s", err)
            except Exception as e:
                self._file_logger.info("收尾补发 stop_record 异常: %s", e)

        s = self.snapshot()
        self._emit(
            "=== 压测结束: 总循环 %d, 成功 %d (成功率 %.1f%%), "
            "开始失败 %d, 停止失败 %d, 异常 %d, 音频 %dB/%d包, 含音频会话 %d%s%s ===",
            s["cycles_total"], s["cycles_ok"], s["success_rate"],
            s["start_fail"], s["stop_fail"], s["exception_fail"],
            s["audio_bytes_total"], s["audio_events"], s["sessions_with_audio"],
            "，已中止" if s["aborted"] else "",
            f"（{s['abort_reason']}）" if s["abort_reason"] else "")

    def _do_cycle(self, cycle: int) -> None:
        with self._lock:
            self.cycles_total += 1
            session_audio = 0

        # ---- START ----
        try:
            err = self._dev.start_record()
        except Exception as e:
            self._fail(cycle, "开始", f"异常: {type(e).__name__}: {e}")
            return
        if err != ErrCode.SUCCESS:
            self._fail(cycle, "开始", f"err={err}")
            return
        self._emit("[CYCLE #%d] 开始录音 OK(err=%s)" % (cycle, err))

        # ---- 录音窗口（可被 stop() 打断） ----
        with self._lock:
            self._recording = True
            self._session_audio = 0
        record_s = random.uniform(self._min_record_s, self._max_record_s)
        self._stop_flag.wait(record_s)
        with self._lock:
            self._recording = False
            session_audio = self._session_audio

        # ---- STOP ----
        try:
            err = self._dev.stop_record()
        except Exception as e:
            self._fail(cycle, "停止", f"异常: {type(e).__name__}: {e}",
                       session_audio=session_audio)
            return
        if err != ErrCode.SUCCESS:
            self._fail(cycle, "停止", f"err={err}", session_audio=session_audio)
            return

        # ---- 成功 ----
        with self._lock:
            self.cycles_ok += 1
            if session_audio > 0:
                self.sessions_with_audio += 1
            self.consecutive_failures = 0
        self._emit("[CYCLE #%d] 结果: 成功 | 录音 %.1fs, 音频 %dB | 累计 %d/%d (%.1f%%)"
                   % (cycle, record_s, session_audio,
                      self.cycles_ok, self.cycles_total, self.success_rate()))

        # ---- 间隔（可被 stop() 打断） ----
        gap = random.uniform(self._min_gap_s, self._max_gap_s)
        self._stop_flag.wait(gap)

    def _fail(self, cycle: int, phase: str, detail: str,
              session_audio: int = 0) -> None:
        """记录一次失败：分类计数 + 连续失败熔断。"""
        with self._lock:
            if phase == "开始":
                if detail.startswith("异常"):
                    self.exception_fail += 1
                else:
                    self.start_fail += 1
            else:
                if detail.startswith("异常"):
                    self.exception_fail += 1
                else:
                    self.stop_fail += 1
            self.consecutive_failures += 1
            consecutive = self.consecutive_failures
            total, ok = self.cycles_total, self.cycles_ok
        self._emit("[CYCLE #%d] 结果: 失败(%s %s) | 音频 %dB | 累计 %d/%d (%.1f%%)"
                   % (cycle, phase, detail, session_audio,
                      ok, total, self.success_rate()))
        if consecutive >= self._max_consecutive_failures:
            with self._lock:
                self.aborted = True
                self.abort_reason = f"连续失败 {consecutive} 次（{phase}阶段: {detail}）"
            self._emit("!!! 连续失败 %d 次，中止压测 !!!" % consecutive)
            self._stop_flag.set()
        else:
            # 失败后也等间隔再重试，避免失败风暴
            gap = random.uniform(self._min_gap_s, self._max_gap_s)
            self._stop_flag.wait(gap)
