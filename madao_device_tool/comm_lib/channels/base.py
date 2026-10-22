"""通信通道抽象基类。

上层设备模型只依赖 BaseComm，不感知底层是 BLE 还是 USB。
对外接口（子类必须实现）：
    scan()            -> 异步扫描，结果通过 on_device_found 回调
    connect(target)   -> 连接指定设备
    disconnect()      -> 断开，释放底层资源（句柄/连接）
    is_connected      -> 当前是否已连接
    send_raw(frame)   -> 发送一个完整帧（bytes）
    start() / stop()  -> 启停接收循环

接收侧约定：子类在收到任意字节后调用 self._feed_rx(data)，
基类用 ProtocolDecoder 解出帧，通过 on_frame 回调上抛 ProtocolFrame。
"""

from __future__ import annotations

import logging
import threading
from abc import ABC, abstractmethod
from typing import Callable, Optional

from ..protocol.frame import ProtocolDecoder, ProtocolFrame
from exceptions import DeviceToolError

logger = logging.getLogger(__name__)


class CommError(DeviceToolError):
    """通信层基础异常。"""


class DeviceInfo:
    """扫描到的设备基本信息。子类可扩展额外字段（如 ble_device_id / usb_address）。"""

    def __init__(
        self,
        transport: str,
        address: str,
        name: str = "",
        product_no: Optional[bytes] = None,
        device_type: Optional[int] = None,
        extra: Optional[dict] = None,
    ) -> None:
        self.transport = transport  # "ble" / "usb"
        self.address = address      # BLE: mac 字符串；USB: bus:address 或 dongle 上的子设备 mac
        self.name = name
        self.product_no = product_no
        self.device_type = device_type  # 0x31=mouse 0x32=keyboard
        self.extra = extra or {}

    def __repr__(self) -> str:
        return (
            f"DeviceInfo(transport={self.transport!r}, address={self.address!r}, "
            f"name={self.name!r}, device_type={hex(self.device_type) if self.device_type else None})"
        )


class BaseComm(ABC):
    """通信通道抽象基类。BLE/USB 子类实现硬件相关方法。"""

    # 自动重连参数
    RECONNECT_MAX_ATTEMPTS = 3
    RECONNECT_INTERVAL_SEC = 2.0

    def __init__(self, auto_reconnect: bool = False) -> None:
        self._decoder = ProtocolDecoder()
        self._rx_thread: Optional[threading.Thread] = None
        self._running = threading.Event()
        self._lock = threading.Lock()
        self._auto_reconnect = auto_reconnect
        self._reconnecting = threading.Event()
        # 保存最近一次连接目标，供自动重连使用
        self._last_target = None
        # 外部回调
        self.on_frame: Optional[Callable[[ProtocolFrame], None]] = None
        self.on_device_found: Optional[Callable[[DeviceInfo], None]] = None
        self.on_disconnected: Optional[Callable[[], None]] = None
        self.on_reconnect_failed: Optional[Callable[[], None]] = None

    # ---- 子类必须实现 ----
    @abstractmethod
    def scan(self) -> None:
        """启动扫描。扫描结果通过 on_device_found 回调上报。"""

    @abstractmethod
    def connect(self, target) -> None:
        """连接目标设备。target 类型由子类定义（mac 字符串 / DeviceInfo / 地址等）。"""

    @abstractmethod
    def disconnect(self) -> None:
        """断开连接，释放底层资源（USB 句柄、BLE 连接等），避免泄漏。"""

    @abstractmethod
    def send_raw(self, frame: bytes) -> int:
        """发送一个完整帧的原始字节。返回写入字节数，失败抛 CommError。"""

    @property
    @abstractmethod
    def is_connected(self) -> bool:
        """当前是否处于已连接状态。"""

    # ---- 基类提供的通用能力 ----
    def _feed_rx(self, data: bytes) -> None:
        """子类接收循环调用：把刚收到的字节喂入解码器，解出的帧回调上层。"""
        if not data:
            return
        self._decoder.feed(data)
        for frame in self._decoder.take_frames():
            logger.debug("rx frame cmd=0x%02x seq=%d len=%d", frame.cmd, frame.sequence, len(frame.payload))
            if self.on_frame:
                self.on_frame(frame)

    def _notify_disconnected(self) -> None:
        """子类在连接断开（拔出/掉线）时调用。

        若启用 auto_reconnect，则在新线程中尝试重连，避免阻塞接收循环；
        重连失败（达到最大次数）后回调 on_reconnect_failed。
        """
        if self.on_disconnected:
            self.on_disconnected()
        if self._auto_reconnect and not self._reconnecting.is_set():
            self._reconnecting.set()
            t = threading.Thread(target=self._reconnect_loop, name="comm-reconnect", daemon=True)
            t.start()

    def _reconnect_loop(self) -> None:
        """自动重连主循环：最多 RECONNECT_MAX_ATTEMPTS 次，间隔 RECONNECT_INTERVAL_SEC 秒。

        子类需实现 _reconnect() 返回 bool（True=成功）。
        成功后调用 _after_reconnect() 重新订阅 notify / claim interface。
        """
        try:
            for attempt in range(1, self.RECONNECT_MAX_ATTEMPTS + 1):
                logger.debug("reconnect attempt %d/%d", attempt, self.RECONNECT_MAX_ATTEMPTS)
                try:
                    ok = self._reconnect()
                except Exception as e:
                    logger.debug("reconnect attempt %d raised: %r", attempt, e)
                    ok = False
                if ok:
                    try:
                        self._after_reconnect()
                    except Exception as e:
                        logger.debug("after_reconnect raised: %r", e)
                    logger.debug("reconnect succeeded on attempt %d", attempt)
                    return
                if attempt < self.RECONNECT_MAX_ATTEMPTS:
                    self._reconnecting.wait(timeout=self.RECONNECT_INTERVAL_SEC)
                    if not self._reconnecting.is_set():
                        return  # 已被外部清除（如主动 disconnect）
            logger.debug("reconnect failed after %d attempts", self.RECONNECT_MAX_ATTEMPTS)
            if self.on_reconnect_failed:
                self.on_reconnect_failed()
        finally:
            self._reconnecting.clear()

    def _reconnect(self) -> bool:
        """子类实现具体重连动作。返回 True 表示成功。

        默认实现返回 False（不重连）。启用 auto_reconnect 的子类应覆盖此方法。
        """
        return False

    def _after_reconnect(self) -> None:
        """重连成功后的恢复动作：BLE 重新订阅 notify，USB 重新 claim interface。

        默认空实现，子类按需覆盖。
        """
        return None

    def send_frame(self, cmd: int, payload: bytes, sequence: int) -> int:
        """构造帧并发送。便于上层直接用 cmd/payload 调用。"""
        from ..protocol.frame import build_frame
        logger.debug("tx frame cmd=0x%02x seq=%d len=%d", cmd, sequence, len(payload))
        return self.send_raw(build_frame(cmd, payload, sequence))

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, tb):
        try:
            self.disconnect()
        except Exception:
            pass
