"""键盘设置面板。

只调用 KbDevice 的公开 API（RGB / 滚轮功能 / 基础信息 / 鉴权）。
所有设备操作通过 worker.async_op 在后台执行，结果经 Signal 回填 UI。
"""

from __future__ import annotations

import os
import struct
from datetime import datetime
from typing import Optional

from PySide6.QtCore import Qt, Signal
from PySide6.QtGui import QPixmap
from PySide6.QtWidgets import (
    QComboBox,
    QFormLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QMessageBox,
    QPushButton,
    QSpinBox,
    QVBoxLayout,
    QWidget,
)

import record_config
from comm_lib.protocol.cmd import EventType
from device_config import get_device_config
from device_model import KbDevice
from device_worker import DeviceWorker
import _bootstrap
from _bootstrap import resource_path

_IMG_DIR = resource_path("assets", "images")


def _format_version(ver: int) -> str:
    b = struct.pack("<I", ver & 0xFFFFFFFF)
    return ".".join(str(x) for x in b)


def _device_image_path(product_no_str: str) -> str:
    cfg = get_device_config(product_no_str)
    if cfg and cfg.get("image"):
        p = os.path.join(_IMG_DIR, cfg["image"])
        if os.path.isfile(p):
            return p
    default = os.path.join(_IMG_DIR, "MADAO.png")
    return default if os.path.isfile(default) else ""


def _connection_label(dev) -> str:
    """根据 device.connection_type / transport 返回中文标签。"""
    mapping = {
        "ble": "BLE（蓝牙）",
        "usb_direct": "USB 直连",
        "usb_single_dongle": "USB 接收器（单通道）",
        "usb_dongle": "USB 接收器（多联 dongle）",
    }
    ct = getattr(dev, "connection_type", "") or ""
    if not ct:
        transport = getattr(dev, "transport", "") or ""
        if transport == "usb":
            return mapping["usb_direct"]
        if transport == "ble":
            return mapping["ble"]
        return "-"
    return mapping.get(ct, ct or "-")


class KeyboardPanel(QWidget):
    """键盘设置面板。绑定一个已连接的 KbDevice。"""

    # 音频数据从 WinRT 后台线程上抛，用 Signal 排队到主线程处理
    audio_received = Signal(int, int, bytes)
    # 设备事件（按键按下/抬起等）同样走 Signal 排队到主线程
    event_received = Signal(int, int)

    def __init__(self, dev: KbDevice, worker: DeviceWorker,
                 on_disconnect: Optional[QObject] = None,
                 on_name_changed=None,
                 parent: Optional[QWidget] = None) -> None:
        super().__init__(parent)
        self._dev = dev
        self._worker = worker
        self._on_disconnect = on_disconnect
        self._on_name_changed = on_name_changed
        self._pending_callbacks: dict = {}
        self._recording = False
        self._record_file = None
        self._record_bytes = 0
        self._last_logged_bytes = 0
        self._record_format_seen = False
        self._build_ui()
        self._refresh_base_async()
        self._setup_audio_callback()
        self._setup_event_callback()

    def _setup_event_callback(self) -> None:
        """注册设备事件回调（捕获自定义按键事件等）。"""
        self.event_received.connect(self._on_event_from_main)
        self._dev.on_event = self._on_event_from_worker

    def _on_event_from_worker(self, event_type: int, value: int) -> None:
        """WinRT 后台线程入口：通过 Signal 转发到主线程。"""
        self.event_received.emit(event_type, value)

    def _on_event_from_main(self, event_type: int, value: int) -> None:
        """主线程：处理设备事件（按键按下/抬起等）。

        KEY 事件 value 格式（实际固件）：
            bit 0         = 状态（1=按下，0=抬起）
            bit 16-23     = key_index
        """
        device_tag = f"[{self._dev.name or '设备'} {self._dev.address}]"
        if event_type == EventType.KEY:
            key_index = (value >> 16) & 0xFF
            pressed = (value & 0x01) == 1
            action = "按下" if pressed else "抬起"
            self._log(f"{device_tag}【按键{key_index}】{action}")
        elif event_type == EventType.BATTERY:
            # 电池事件直接更新电量标签（即便 get_battery 已超时也能刷新）
            self.lbl_battery.setText(f"{value} mV")
        else:
            self._log(f"{device_tag} 事件 type=0x{event_type:02X} value=0x{value:08X}")

    def _setup_audio_callback(self) -> None:
        """注册音频数据回调，用于接收录音数据。"""
        self.audio_received.connect(self._on_audio)
        self._dev.on_audio = self._on_audio_from_worker

    def _on_audio_from_worker(self, audio_type: int, channel: int, data: bytes) -> None:
        """WinRT 后台线程入口：通过 Signal 转发到主线程。"""
        import logging
        logging.getLogger(__name__).debug(
            f"on_audio called: addr={self._dev.address} type={audio_type} ch={channel} len={len(data)}"
        )
        self.audio_received.emit(audio_type, channel, data)

    def _on_audio(self, audio_type: int, channel: int, data: bytes) -> None:
        """主线程：处理设备上报的音频数据。

        数据包格式：payload=[type][channel][data...]
            type    = 编码格式（0=PCM / 1=ICO / 2=OPUS 等）
            channel = 通道号
            data    = 音频数据
        """
        # 首次收到音频包时打日志显示设备使用的格式和通道
        if not getattr(self, "_record_format_seen", False):
            self._record_format_seen = True
            device_tag = f"[{self._dev.name or '设备'} {self._dev.address}]"
            self._log(f"{device_tag} 收到音频: type={audio_type} ch={channel} "
                      f"(1=PCM, 2=ICO, 3=OPUS)")
        self._record_bytes += len(data)
        # 每累计 10KB 打印一次进度
        if self._record_bytes - self._last_logged_bytes >= 10240:
            self._last_logged_bytes = self._record_bytes
            self._log(f"录音中... 已接收 {self._record_bytes} 字节")
        if record_config.is_save_enabled() and data:
            if self._record_file is None:
                self._start_record_file()
            if self._record_file is not None:
                try:
                    self._record_file.write(data)
                    self._record_file.flush()
                except Exception as e:
                    self._log(f"写入录音文件失败: {e}", "ERROR")
                    self._close_record_file()

    def _start_record_file(self) -> None:
        """开启一个新的录音文件。文件名包含设备名、MAC后缀、毫秒时间戳，避免多设备冲突。"""
        record_config.ensure_record_dir()
        name = self._dev.name or "device"
        name = "".join(c if c.isalnum() or c in "-_" else "_" for c in name)
        address = self._dev.address.replace(":", "")
        ts = datetime.now().strftime("%Y%m%d_%H%M%S_%f")[:-3]
        path = os.path.join(record_config.get_record_dir(), f"record_{name}_{address}_{ts}.pcm")
        try:
            self._record_file = open(path, "wb")
            self._log(f"开始保存录音: {path}")
        except Exception as e:
            self._log(f"创建录音文件失败: {e}", "ERROR")
            self._record_file = None

    def _close_record_file(self) -> None:
        """关闭当前录音文件。"""
        if self._record_file is not None:
            try:
                self._record_file.close()
            except Exception:
                pass
            self._record_file = None

    # ---- UI ----
    def _build_ui(self) -> None:
        root = QVBoxLayout(self)

        # 顶部行：左边（基础信息 + 操作按钮列同一行），右边图片
        top_row = QHBoxLayout()
        left_block = QHBoxLayout()
        left_block.addWidget(self._build_info_group(), 1)
        left_block.addLayout(self._build_action_column())
        left_widget = QWidget()
        left_widget.setLayout(left_block)
        top_row.addWidget(left_widget, 2)
        self._build_image_label()
        top_row.addWidget(self.lbl_image, 3)
        root.addLayout(top_row)

        # RGB 三项并排（按 capabilities 动态显示）
        self._rgb_row = QHBoxLayout()
        self._rgb_mode_group = self._build_rgb_mode_group()
        self._rgb_brightness_group = self._build_rgb_brightness_group()
        self._rgb_speed_group = self._build_rgb_speed_group()
        self._rgb_row.addWidget(self._rgb_mode_group)
        self._rgb_row.addWidget(self._rgb_brightness_group)
        self._rgb_row.addWidget(self._rgb_speed_group)
        root.addLayout(self._rgb_row)

        # 滚轮功能（按 capabilities 动态显示）
        self._wheel_group = self._build_wheel_group()
        root.addWidget(self._wheel_group)

        # 设备休眠时间（0x10，设备通用能力）
        self._sleep_group = self._build_sleep_group()
        root.addWidget(self._sleep_group)

        # AT80 专属（按 product_no 动态显示）
        self._at80_group = self._build_at80_group()
        self._at80_group.setVisible(False)
        root.addWidget(self._at80_group)
        root.addStretch()

        # 默认全部显示；product_no 读到后再按 capabilities 调整
        self._apply_capabilities()

    def _build_action_column(self) -> QVBoxLayout:
        col = QVBoxLayout()
        self.btn_auth = QPushButton("鉴权")
        self.btn_auth.clicked.connect(self._auth)
        self.btn_record = QPushButton("开始录音")
        self.btn_record.clicked.connect(self._toggle_record)
        self.btn_ota = QPushButton("固件升级")
        self.btn_ota.clicked.connect(self._open_ota)
        self.btn_disconnect = QPushButton("断开")
        self.btn_disconnect.clicked.connect(self._do_disconnect)
        col.addWidget(self.btn_auth)
        col.addWidget(self.btn_record)
        col.addWidget(self.btn_ota)
        col.addWidget(self.btn_disconnect)
        col.addStretch()
        return col

    def _apply_capabilities(self) -> None:
        """根据设备 product_no 对应的 capabilities，动态显示/隐藏功能组。"""
        # 构造时 product_no 可能还没读到，先用扫描名推断或默认全部显示
        pn_str = ""
        if self._dev.product_no:
            pn_str = bytes(self._dev.product_no).decode("ascii", errors="replace").strip("\x00")
        cfg = get_device_config(pn_str) if pn_str else None
        caps = set((cfg or {}).get("capabilities", []) or [])
        has_rgb = "rgb" in caps
        has_wheel = "wheel" in caps
        # 缺配置或没有 rgb/wheel 能力时，隐藏对应组
        self._rgb_row_widget_visible = has_rgb
        for w in (self._rgb_mode_group, self._rgb_brightness_group, self._rgb_speed_group):
            w.setVisible(has_rgb)
        self._wheel_group.setVisible(has_wheel)
        # AT80 (product_no == "3007") 才显示专属功能，并主动读取一次从机版本号
        is_at80 = (pn_str == "3007")
        self._at80_group.setVisible(is_at80)
        if is_at80:
            self._read_slave_device_version()

    def _build_info_group(self) -> QGroupBox:
        g = QGroupBox("基础信息")
        v = QVBoxLayout(g)
        form = QFormLayout()
        self.lbl_product_no = QLabel("-")
        self.lbl_product_name = QLabel("-")
        self.lbl_mac = QLabel("-")
        self.lbl_version = QLabel("-")
        self.lbl_battery = QLabel("-")
        self.lbl_connection = QLabel(_connection_label(self._dev))
        form.addRow("产品编号:", self.lbl_product_no)
        form.addRow("产品名称:", self.lbl_product_name)
        form.addRow("连接方式:", self.lbl_connection)
        form.addRow("MAC:", self.lbl_mac)
        form.addRow("版本:", self.lbl_version)
        form.addRow("电池(mV):", self.lbl_battery)
        v.addLayout(form)
        v.addStretch()
        return g

    def _build_image_label(self) -> None:
        """构建键盘图片显示标签（放在基础信息框外面，与基础信息等高）。"""
        self.lbl_image = QLabel()
        self.lbl_image.setMinimumSize(320, 180)
        self.lbl_image.setAlignment(Qt.AlignCenter)
        self.lbl_image.setStyleSheet("border: 1px solid #ccc; background: #fafafa;")
        self.lbl_image.setText("无图片")
        self._current_pixmap = None

    def _build_rgb_mode_group(self) -> QGroupBox:
        g = QGroupBox("RGB 模式 (0-17)")
        h = QHBoxLayout(g)
        self.sp_mode = QSpinBox()
        self.sp_mode.setRange(0, 17)
        self.btn_mode_read = QPushButton("读取")
        self.btn_mode_set = QPushButton("设置")
        self.btn_mode_read.clicked.connect(self._read_mode)
        self.btn_mode_set.clicked.connect(self._set_mode)
        h.addWidget(self.sp_mode)
        h.addWidget(self.btn_mode_read)
        h.addWidget(self.btn_mode_set)
        h.addStretch()
        return g

    def _build_rgb_brightness_group(self) -> QGroupBox:
        g = QGroupBox("RGB 亮度")
        h = QHBoxLayout(g)
        self.cmb_brightness = QComboBox()
        for v in (0, 1, 2, 3):
            self.cmb_brightness.addItem(str(v), v)
        self.btn_brt_read = QPushButton("读取")
        self.btn_brt_set = QPushButton("设置")
        self.btn_brt_read.clicked.connect(self._read_brightness)
        self.btn_brt_set.clicked.connect(self._set_brightness)
        h.addWidget(self.cmb_brightness)
        h.addWidget(self.btn_brt_read)
        h.addWidget(self.btn_brt_set)
        h.addStretch()
        return g

    def _build_rgb_speed_group(self) -> QGroupBox:
        g = QGroupBox("RGB 速度")
        h = QHBoxLayout(g)
        self.cmb_speed = QComboBox()
        for v in (0, 1, 2, 3):
            self.cmb_speed.addItem(str(v), v)
        self.btn_spd_read = QPushButton("读取")
        self.btn_spd_set = QPushButton("设置")
        self.btn_spd_read.clicked.connect(self._read_speed)
        self.btn_spd_set.clicked.connect(self._set_speed)
        h.addWidget(self.cmb_speed)
        h.addWidget(self.btn_spd_read)
        h.addWidget(self.btn_spd_set)
        h.addStretch()
        return g

    def _build_wheel_group(self) -> QGroupBox:
        g = QGroupBox("滚轮功能")
        h = QHBoxLayout(g)
        self.sp_wheel = QSpinBox()
        self.sp_wheel.setRange(0, 0x7FFFFFFF)
        self.btn_wheel_read = QPushButton("读取")
        self.btn_wheel_set = QPushButton("设置")
        self.btn_wheel_read.clicked.connect(self._read_wheel)
        self.btn_wheel_set.clicked.connect(self._set_wheel)
        h.addWidget(self.sp_wheel)
        h.addWidget(self.btn_wheel_read)
        h.addWidget(self.btn_wheel_set)
        h.addStretch()
        return g

    def _build_sleep_group(self) -> QGroupBox:
        """设备休眠时间（0x10，设备通用能力）。"""
        g = QGroupBox("设备休眠时间 (秒)")
        h = QHBoxLayout(g)
        self.sp_sleep_time = QSpinBox()
        self.sp_sleep_time.setRange(0, 86400)
        self.btn_sleep_time_read = QPushButton("读取")
        self.btn_sleep_time_set = QPushButton("设置")
        self.btn_sleep_time_read.clicked.connect(self._read_sleep_time)
        self.btn_sleep_time_set.clicked.connect(self._set_sleep_time)
        h.addWidget(self.sp_sleep_time)
        h.addWidget(self.btn_sleep_time_read)
        h.addWidget(self.btn_sleep_time_set)
        h.addStretch()
        return g

    def _build_at80_group(self) -> QGroupBox:
        """AT80 专属：从机版本号读取。"""
        g = QGroupBox("AT80 专属")
        h = QHBoxLayout(g)
        h.addWidget(QLabel("从机版本号:"))
        self.btn_slave_ver_read = QPushButton("读取")
        self.lbl_slave_version = QLabel("-")
        self.btn_slave_ver_read.clicked.connect(self._read_slave_device_version)
        h.addWidget(self.btn_slave_ver_read)
        h.addWidget(self.lbl_slave_version)
        h.addStretch()
        return g

    # ---- 图片自适应 ----
    def _rescale_image(self) -> None:
        if self._current_pixmap and not self._current_pixmap.isNull():
            w = max(self.lbl_image.width() - 8, 50)
            h = max(self.lbl_image.height() - 8, 50)
            scaled = self._current_pixmap.scaled(
                w, h, Qt.KeepAspectRatio, Qt.SmoothTransformation
            )
            self.lbl_image.setPixmap(scaled)

    def resizeEvent(self, event) -> None:
        super().resizeEvent(event)
        self._rescale_image()

    # ---- 基础信息 ----
    def _refresh_base_async(self) -> None:
        def _on_product_no(b):
            pn_str = b.decode("ascii", errors="replace") if isinstance(b, bytes) else str(b)
            self.lbl_product_no.setText(pn_str)
            self._dev.product_no = pn_str.encode() if isinstance(b, bytes) else b
            cfg = get_device_config(pn_str)
            name = cfg["name"] if cfg else pn_str
            self.lbl_product_name.setText(name)
            img_path = _device_image_path(pn_str)
            if img_path:
                pix = QPixmap(img_path)
                if not pix.isNull():
                    self._current_pixmap = pix
                    self._rescale_image()
            self._dev.name = name
            # 根据 capabilities 动态显示/隐藏 RGB、滚轮组
            self._apply_capabilities()
            if self._on_name_changed:
                self._on_name_changed()

        self._safe_op("get_product_no", "get_product_no", on_ok=_on_product_no)
        self._safe_op("get_device_mac", "get_device_mac",
                      on_ok=lambda b: self.lbl_mac.setText(
                          bytes(b[::-1]).hex(":") if isinstance(b, bytes) else str(b)))
        self._safe_op("get_device_version", "get_device_version",
                      on_ok=lambda v: self.lbl_version.setText(_format_version(v) if isinstance(v, int) else str(v)))
        self._safe_op("get_battery", "get_battery",
                      on_ok=lambda v: self.lbl_battery.setText(str(v)))

    # ---- 操作 ----
    def _read_mode(self) -> None:
        self._safe_op("get_rgb_mode", "get_rgb_mode",
                      on_ok=lambda v: self.sp_mode.setValue(v))

    def _set_mode(self) -> None:
        self._safe_op("set_rgb_mode", "set_rgb_mode", self.sp_mode.value())

    def _read_brightness(self) -> None:
        self._safe_op("get_rgb_brightness", "get_rgb_brightness",
                      on_ok=lambda v: self._set_combo_by_value(self.cmb_brightness, v))

    def _set_brightness(self) -> None:
        self._safe_op("set_rgb_brightness", "set_rgb_brightness",
                      self.cmb_brightness.currentData())

    def _read_speed(self) -> None:
        self._safe_op("get_rgb_speed", "get_rgb_speed",
                      on_ok=lambda v: self._set_combo_by_value(self.cmb_speed, v))

    def _set_speed(self) -> None:
        self._safe_op("set_rgb_speed", "set_rgb_speed",
                      self.cmb_speed.currentData())

    def _read_wheel(self) -> None:
        self._safe_op("get_wheel_function", "get_wheel_function",
                      on_ok=lambda v: self.sp_wheel.setValue(v))

    def _set_wheel(self) -> None:
        self._safe_op("set_wheel_function", "set_wheel_function",
                      self.sp_wheel.value())

    def _read_sleep_time(self) -> None:
        def _on_val(value):
            self.sp_sleep_time.setValue(value)
            self._log(f"设备休眠时间: {value} 秒")
        self._safe_op("get_sleep_time", "get_sleep_time", on_ok=_on_val)

    def _set_sleep_time(self) -> None:
        seconds = self.sp_sleep_time.value()
        self._safe_op("set_sleep_time", "set_sleep_time", seconds,
                      on_ok=lambda _: self._log(f"设备休眠时间已设置为 {seconds} 秒"))

    def _read_slave_device_version(self) -> None:
        def _on_ver(items: list):
            if not items:
                self.lbl_slave_version.setText("无")
                self._log("从机版本号: 无")
                return
            parts = []
            for dev_id, ver in items:
                # version_bytes 按应答原样字节序拼成 x.y.z.w
                version = ".".join(str(b) for b in ver)
                if dev_id:
                    # 2 字节设备号 = [类型(高字节)][索引(低字节)]
                    t = (dev_id >> 8) & 0xFF
                    idx = dev_id & 0xFF
                    parts.append(f"类型{t}#索引{idx}:{version}")
                else:
                    parts.append(version)
            self.lbl_slave_version.setText("; ".join(parts))
            self._log("从机版本号: " + ", ".join(parts))
        self._safe_op("get_slave_device_version", "get_slave_device_version", on_ok=_on_ver)

    def _auth(self) -> None:
        self._safe_op("authenticate", "authenticate",
                      on_ok=lambda ok: self._info(f"鉴权结果: {ok}"))

    def _toggle_record(self) -> None:
        if self._recording:
            self._safe_op("stop_record", "stop_record",
                          on_ok=lambda _: self._on_record_stopped())
        else:
            self._record_bytes = 0
            self._record_format_seen = False
            self._safe_op("start_record", "start_record",
                          on_ok=lambda _: self._on_record_started())

    def _on_record_started(self) -> None:
        self._recording = True
        self._last_logged_bytes = 0
        self.btn_record.setText("停止录音")
        device_tag = f"[{self._dev.name or '设备'} {self._dev.address}]"
        self._log(f"{device_tag} 开始录音")
        if record_config.is_save_enabled():
            self._start_record_file()

    def _on_record_stopped(self) -> None:
        self._recording = False
        self.btn_record.setText("开始录音")
        self._close_record_file()
        device_tag = f"[{self._dev.name or '设备'} {self._dev.address}]"
        self._log(f"{device_tag} 停止录音，共接收 {self._record_bytes} 字节")
        self._record_bytes = 0
        self._last_logged_bytes = 0
        self._record_format_seen = False

    def _do_disconnect(self) -> None:
        if self._on_disconnect is not None:
            self._on_disconnect.request_disconnect(self._dev.address)

    def _open_ota(self) -> None:
        """打开固件升级对话框（非模态，升级期间不阻塞主窗口）。"""
        from ota_dialog import OtaDialog
        self._ota_dialog = OtaDialog([self._dev], self._worker)
        self._ota_dialog.show()

    # ---- 辅助 ----
    def _safe_op(self, op_id: str, method_name: str, *args, on_ok=None) -> None:
        unique_op_id = f"{self._dev.address}:{op_id}"
        self._worker.async_op(unique_op_id, self._dev.address, method_name, *args)
        if on_ok is not None:
            self._pending_callbacks[unique_op_id] = on_ok

    def _set_combo_by_value(self, combo, value) -> None:
        idx = combo.findData(value)
        if idx >= 0:
            combo.setCurrentIndex(idx)

    def _info(self, msg: str) -> None:
        QMessageBox.information(self, "提示", msg)

    def handle_result(self, op_id: str, result) -> None:
        cb = self._pending_callbacks.pop(op_id, None)
        if cb is not None:
            try:
                cb(result)
            except Exception as e:
                self._info(f"更新 UI 失败: {e}")

    def handle_error(self, op_id: str, message: str) -> None:
        self._pending_callbacks.pop(op_id, None)

    def _log(self, msg: str, level: str = "INFO") -> None:
        """写入底部日志区（通过主窗口的 _log 方法）。"""
        if self._on_disconnect is not None and hasattr(self._on_disconnect, "_log"):
            self._on_disconnect._log(msg, level)
