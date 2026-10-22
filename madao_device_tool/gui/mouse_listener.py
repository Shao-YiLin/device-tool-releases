"""实时回报率测量工具。

使用 pynput 监听鼠标移动事件，统计单位时间内的报告次数，
计算出鼠标的实际回报率（Polling Rate / Hz）。

与设备协议级 get/set_rate 不同，这里测量的是操作系统实际收到的报告频率。
"""

from __future__ import annotations

import threading
import time
from typing import Optional


class MouseListener:
    """基于 pynput 的实时回报率测量器。

    在独立线程中统计鼠标移动事件，调用方通过 :meth:`get_rate` 获取当前 Hz。
    每次读取后自动重置计数窗口，实现连续测量。
    """

    def __init__(self) -> None:
        self._event_count = 0
        self._start_time: Optional[float] = None
        self._listener = None
        self._lock = threading.Lock()
        self._running = False

    def _on_move(self, x, y) -> None:
        with self._lock:
            self._event_count += 1
            if self._start_time is None:
                self._start_time = time.time()

    def start(self) -> None:
        if self._running:
            return
        from pynput import mouse

        self._running = True
        with self._lock:
            self._event_count = 0
            self._start_time = None
        self._listener = mouse.Listener(on_move=self._on_move)
        self._listener.daemon = True
        self._listener.start()

    def stop(self) -> None:
        self._running = False
        if self._listener is not None:
            self._listener.stop()
            self._listener = None

    @property
    def running(self) -> bool:
        return self._running

    def get_rate(self) -> float:
        """返回当前窗口的回报率（Hz），并重置计数窗口。

        Returns:
            0.0 如果尚未开始或窗口时间太短。
        """
        with self._lock:
            if self._start_time is None or self._event_count == 0:
                self._event_count = 0
                self._start_time = None
                return 0.0
            elapsed = time.time() - self._start_time
            count = self._event_count
            self._event_count = 0
            self._start_time = None
        if elapsed <= 0:
            return 0.0
        return count / elapsed
