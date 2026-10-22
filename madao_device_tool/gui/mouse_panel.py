"""鼠标设置面板。

只调用 MouseDevice 的公开 API，不感知通信层。
所有设备操作通过 worker.async_op 在后台执行，结果经 Signal 回填 UI。
"""

from __future__ import annotations

import os
import struct
from datetime import datetime
from typing import Optional

from PySide6.QtCore import Qt, Signal
from PySide6.QtGui import QColor, QPixmap
from PySide6.QtWidgets import (
    QColorDialog,
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
from device_config import get_device_config, supports_light
from device_model import MouseDevice
from device_worker import DeviceWorker
import _bootstrap
from _bootstrap import resource_path

_IMG_DIR = resource_path("assets", "images")


# 灯光模式映射（对齐设备端 device_config_set_rgb）
_LIGHT_MODES = {
    0: "关闭",
    1: "单色",
    3: "呼吸",
    4: "触发",
}
def _format_version(ver: int) -> str:
    """u32 小端版本号转可读格式：0x17000001 -> '1.0.0.23'。"""
    b = struct.pack("<I", ver & 0xFFFFFFFF)
    return ".".join(str(x) for x in b)


def _device_image_path(product_no_str: str) -> str:
    """按 product_no 查图片。找不到配置/没图片/文件不存在时回退 MADAO.png。"""
    cfg = get_device_config(product_no_str)
    if cfg and cfg.get("image"):
        p = os.path.join(_IMG_DIR, cfg["image"])
        if os.path.isfile(p):
            return p
    # 回退默认图片
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


class MousePanel(QWidget):
    """鼠标设置面板。绑定一个已连接的 MouseDevice。"""

    # 音频数据从 WinRT 后台线程上抛，用 Signal 排队到主线程处理
    audio_received = Signal(int, int, bytes)
    # 设备事件（按键按下/抬起等）同样走 Signal 排队到主线程
    event_received = Signal(int, int)

    def __init__(self, dev: MouseDevice, worker: DeviceWorker,
                 on_disconnect: Optional[QObject] = None,
                 on_name_changed=None,
                 on_type_mismatch=None,
                 parent: Optional[QWidget] = None) -> None:
        super().__init__(parent)
        self._dev = dev
        self._worker = worker
        self._on_disconnect = on_disconnect
        self._on_name_changed = on_name_changed
        self._on_type_mismatch = on_type_mismatch
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

    # 注：CPI/回报率的初始化延迟到 product_no 读到、确认是鼠标后再调用，
    # 避免在设备类型未确定前对键盘设备错误地调用 get_cpi / get_rate。

    def _setup_event_callback(self) -> None:
        """注册设备事件回调（捕获按键按下/抬起等）。"""
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
            # 录音文件后缀按实际音频格式决定；首次收到格式时才创建文件
            if self._record_file is None:
                self._record_format = audio_type
                self._start_record_file(audio_type)
            if self._record_file is not None:
                try:
                    self._record_file.write(data)
                    self._record_file.flush()
                except Exception as e:
                    self._log(f"写入录音文件失败: {e}", "ERROR")
                    self._close_record_file()

    def _start_record_file(self, audio_type: int = 0) -> None:
        """开启一个新的录音文件。文件名包含设备名、MAC后缀、毫秒时间戳和格式后缀。"""
        ext_map = {1: "pcm", 2: "ico", 3: "opus"}
        ext = ext_map.get(audio_type, "bin")
        record_config.ensure_record_dir()
        name = self._dev.name or "device"
        name = "".join(c if c.isalnum() or c in "-_" else "_" for c in name)
        address = self._dev.address.replace(":", "")
        ts = datetime.now().strftime("%Y%m%d_%H%M%S_%f")[:-3]
        path = os.path.join(record_config.get_record_dir(), f"record_{name}_{address}_{ts}.{ext}")
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

        # 顶部行：左边设置组竖排，右边大图片区
        top_row = QHBoxLayout()
        left_col = QVBoxLayout()
        # 基础信息 + 操作按钮列在同一行（共享基础信息的空间，不占用图片区）
        info_row = QHBoxLayout()
        info_row.addWidget(self._build_info_group(), 1)
        info_row.addLayout(self._build_action_column())
        left_col.addLayout(info_row)
        left_col.addWidget(self._build_cpi_group())
        left_col.addWidget(self._build_rate_group())
        self._light_group = self._build_light_group()
        self._light_group.setVisible(False)
        left_col.addWidget(self._light_group)
        top_row.addLayout(left_col, 3)

        # 右侧图片框：放在基础信息框外，与左侧组等高
        self._build_image_label()
        top_row.addWidget(self.lbl_image, 2)  # stretch=2，图片占右半边
        root.addLayout(top_row)

        # M610 Ultra 专属功能（动态显隐）
        self._m610_ultra_group = self._build_m610_ultra_group()
        self._m610_ultra_group.setVisible(False)
        root.addWidget(self._m610_ultra_group)
        root.addStretch()

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
        """构建鼠标图片显示标签（放在基础信息框外面）。"""
        self.lbl_image = QLabel()
        self.lbl_image.setMinimumSize(220, 240)
        self.lbl_image.setAlignment(Qt.AlignCenter)
        self.lbl_image.setStyleSheet("border: 1px solid #ccc; background: #fafafa;")
        self.lbl_image.setText("无图片")
        self._current_pixmap = None  # 保存原始图片，resize 时重新缩放

    def _build_cpi_group(self) -> QGroupBox:
        g = QGroupBox("CPI（支持等级档位或具体数值）")
        h = QHBoxLayout(g)
        self.sb_cpi = QSpinBox()
        self.sb_cpi.setRange(100, 30000)
        self.btn_cpi_read = QPushButton("读取")
        self.btn_cpi_set = QPushButton("设置")
        self.btn_cpi_read.clicked.connect(self._read_cpi)
        self.btn_cpi_set.clicked.connect(self._set_cpi)
        h.addWidget(QLabel("CPI:"))
        h.addWidget(self.sb_cpi)
        h.addWidget(self.btn_cpi_read)
        h.addWidget(self.btn_cpi_set)
        h.addStretch()
        return g

    def _build_rate_group(self) -> QGroupBox:
        g = QGroupBox("回报率 (Hz)")
        h = QHBoxLayout(g)
        self.cmb_rate = QComboBox()
        for hz in (125, 250, 500, 1000):
            self.cmb_rate.addItem(f"{hz} Hz", hz)
        self.btn_rate_read = QPushButton("读取")
        self.btn_rate_set = QPushButton("设置")
        self.btn_rate_read.clicked.connect(self._read_rate)
        self.btn_rate_set.clicked.connect(self._set_rate)
        h.addWidget(self.cmb_rate)
        h.addWidget(self.btn_rate_read)
        h.addWidget(self.btn_rate_set)
        h.addStretch()
        return g

    def _build_light_group(self) -> QGroupBox:
        """RGB 灯光设置：模式 + 颜色。仅支持灯光的鼠标显示。"""
        g = QGroupBox("灯光设置")
        h = QHBoxLayout(g)
        self.cmb_light_mode = QComboBox()
        for mode, name in _LIGHT_MODES.items():
            self.cmb_light_mode.addItem(name, mode)
        self.cmb_light_mode.setCurrentIndex(0)
        self.btn_light_color = QPushButton("选择颜色")
        self.lbl_light_color = QLabel("#FFFFFF")
        self.lbl_light_color.setStyleSheet("background: #FFFFFF; color: #000000; padding: 2px 8px;")
        self.btn_light_color.clicked.connect(self._choose_light_color)
        self.btn_light_read = QPushButton("读取")
        self.btn_light_set = QPushButton("设置")
        self.btn_light_read.clicked.connect(self._read_light_theme)
        self.btn_light_set.clicked.connect(self._set_light_theme)
        h.addWidget(QLabel("模式:"))
        h.addWidget(self.cmb_light_mode)
        h.addWidget(self.btn_light_color)
        h.addWidget(self.lbl_light_color)
        h.addWidget(self.btn_light_read)
        h.addWidget(self.btn_light_set)
        h.addStretch()
        self._current_light_color = QColor(255, 255, 255)
        return g

    def _build_m610_ultra_group(self) -> QGroupBox:
        """M610 Ultra 专属：FTP 快传开关 + 未同步文件数 + 工作模式 + 触摸灵敏度。"""
        g = QGroupBox("M610 Ultra 专属")
        v = QVBoxLayout(g)

        # 第一行：快传 + 未同步 + 工作模式
        row1 = QHBoxLayout()
        self.btn_quick_transfer = QPushButton("打开快传")
        self.btn_get_unsynced = QPushButton("获取未同步文件数")
        self.lbl_unsynced_count = QLabel("-")
        self.btn_get_work_mode = QPushButton("获取鼠标模式")
        self.lbl_work_mode = QLabel("-")
        self.btn_quick_transfer.clicked.connect(self._toggle_quick_transfer)
        self.btn_get_unsynced.clicked.connect(self._get_unsynced_file_count)
        self.btn_get_work_mode.clicked.connect(self._get_work_mode)
        row1.addWidget(self.btn_quick_transfer)
        row1.addWidget(self.btn_get_unsynced)
        row1.addWidget(self.lbl_unsynced_count)
        row1.addWidget(self.btn_get_work_mode)
        row1.addWidget(self.lbl_work_mode)
        row1.addStretch()
        v.addLayout(row1)

        # 第二行：触摸滚动灵敏度（1~10）
        row2 = QHBoxLayout()
        row2.addWidget(QLabel("触摸灵敏度:"))
        self.sp_touch_sensitivity = QSpinBox()
        self.sp_touch_sensitivity.setRange(1, 10)
        self.btn_touch_read = QPushButton("读取")
        self.btn_touch_set = QPushButton("设置")
        self.btn_touch_read.clicked.connect(self._read_touch_sensitivity)
        self.btn_touch_set.clicked.connect(self._set_touch_sensitivity)
        row2.addWidget(self.sp_touch_sensitivity)
        row2.addWidget(self.btn_touch_read)
        row2.addWidget(self.btn_touch_set)
        row2.addStretch()
        v.addLayout(row2)

        # 第三行：从机版本号 + 屏幕休眠时间
        row3 = QHBoxLayout()
        row3.addWidget(QLabel("从机版本号:"))
        self.btn_slave_ver_read = QPushButton("读取")
        self.lbl_slave_version = QLabel("-")
        self.btn_slave_ver_read.clicked.connect(self._read_slave_device_version)
        row3.addWidget(self.btn_slave_ver_read)
        row3.addWidget(self.lbl_slave_version)
        row3.addSpacing(20)
        row3.addWidget(QLabel("屏幕休眠(秒):"))
        self.sp_screen_sleep = QSpinBox()
        self.sp_screen_sleep.setRange(0, 86400)
        self.btn_screen_sleep_read = QPushButton("读取")
        self.btn_screen_sleep_set = QPushButton("设置")
        self.btn_screen_sleep_read.clicked.connect(self._read_screen_sleep)
        self.btn_screen_sleep_set.clicked.connect(self._set_screen_sleep)
        row3.addWidget(self.sp_screen_sleep)
        row3.addWidget(self.btn_screen_sleep_read)
        row3.addWidget(self.btn_screen_sleep_set)
        row3.addStretch()
        v.addLayout(row3)

        # 第四行：设备休眠时间（0x10）
        row4 = QHBoxLayout()
        row4.addWidget(QLabel("设备休眠(秒):"))
        self.sp_sleep_time = QSpinBox()
        self.sp_sleep_time.setRange(0, 86400)
        self.btn_sleep_time_read = QPushButton("读取")
        self.btn_sleep_time_set = QPushButton("设置")
        self.btn_sleep_time_read.clicked.connect(self._read_sleep_time)
        self.btn_sleep_time_set.clicked.connect(self._set_sleep_time)
        row4.addWidget(self.sp_sleep_time)
        row4.addWidget(self.btn_sleep_time_read)
        row4.addWidget(self.btn_sleep_time_set)
        row4.addStretch()
        v.addLayout(row4)
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
            # 检查设备类型是否匹配：如果实际是键盘但当前是 MousePanel，需要重建
            from device_config import infer_device_type
            inferred = infer_device_type(pn_str)
            if inferred == "keyboard":
                # 键盘被误创建为鼠标面板，通知主窗口重建
                self._dev.name = cfg["name"] if cfg else pn_str
                if self._on_name_changed:
                    self._on_name_changed()
                if hasattr(self, "_on_type_mismatch") and self._on_type_mismatch:
                    self._on_type_mismatch(self._dev)
                return
            name = cfg["name"] if cfg else pn_str
            self.lbl_product_name.setText(name)
            # 加载设备图片
            img_path = _device_image_path(pn_str)
            if img_path:
                pix = QPixmap(img_path)
                if not pix.isNull():
                    self._current_pixmap = pix
                    self._rescale_image()
            # 同步更新设备名并通知主窗口刷新列表
            self._dev.name = name
            if self._on_name_changed:
                self._on_name_changed()
            # M610 Ultra (product_no == "1042") 才显示专属功能，并主动获取工作模式
            is_m610_ultra = (pn_str == "1042")
            self._m610_ultra_group.setVisible(is_m610_ultra)
            if is_m610_ultra:
                self._get_work_mode()
                self._read_touch_sensitivity()
                self._read_slave_device_version()
                self._read_screen_sleep()
                self._read_sleep_time()
            # 支持灯光的鼠标显示灯光设置并主动获取一次主题色
            has_light = supports_light(pn_str)
            self._light_group.setVisible(has_light)
            if has_light:
                self._read_light_theme()
            # 确认是鼠标后再读 CPI / 回报率（避免对键盘设备误调）
            self._read_cpi()
            self._read_rate()

        self._safe_op("get_product_no", "get_product_no", on_ok=_on_product_no)
        self._safe_op("get_device_mac", "get_device_mac",
                      on_ok=lambda b: self.lbl_mac.setText(b.hex(":") if isinstance(b, bytes) else str(b)))
        self._safe_op("get_device_version", "get_device_version",
                      on_ok=lambda v: self.lbl_version.setText(_format_version(v) if isinstance(v, int) else str(v)))
        self._safe_op("get_battery", "get_battery",
                      on_ok=lambda v: self.lbl_battery.setText(str(v)))

    # ---- 操作 ----
    def _read_cpi(self) -> None:
        self._safe_op("get_cpi", "get_cpi",
                      on_ok=lambda v: self.sb_cpi.setValue(int(v) if v is not None else 1))

    def _set_cpi(self) -> None:
        level = self.sb_cpi.value()
        self._safe_op("set_cpi", "set_cpi", level)

    def _read_rate(self) -> None:
        self._safe_op("get_rate", "get_rate",
                      on_ok=lambda v: self._set_combo_by_value(self.cmb_rate, v))

    def _set_rate(self) -> None:
        hz = self.cmb_rate.currentData()
        self._safe_op("set_rate", "set_rate", hz)

    def _choose_light_color(self) -> None:
        color = QColorDialog.getColor(self._current_light_color, self, "选择灯光颜色")
        if color.isValid():
            self._current_light_color = color
            self._update_light_color_label(color)

    def _update_light_color_label(self, color: QColor) -> None:
        hex_color = color.name().upper()
        # 根据背景亮度决定标签文字颜色
        luminance = 0.299 * color.red() + 0.587 * color.green() + 0.114 * color.blue()
        text_color = "#000000" if luminance > 128 else "#FFFFFF"
        self.lbl_light_color.setText(hex_color)
        self.lbl_light_color.setStyleSheet(
            f"background: {hex_color}; color: {text_color}; padding: 2px 8px; border: 1px solid #888;"
        )

    def _apply_light_background(self, r: int, g: int, b: int) -> None:
        """把图片背景色设置为当前灯光主题色。"""
        hex_color = f"#{r:02X}{g:02X}{b:02X}"
        self.lbl_image.setAutoFillBackground(True)
        self.lbl_image.setStyleSheet(
            f"border: 1px solid #ccc; background-color: {hex_color};"
        )

    def _read_light_theme(self) -> None:
        def _on_theme(result):
            mode, r, g, b = result
            self._log(f"获取到主题色: mode={mode} R={r} G={g} B={b}")
            # 同步设置下拉框模式
            self._set_combo_by_value(self.cmb_light_mode, mode)
            color = QColor(r, g, b)
            self._current_light_color = color
            self._update_light_color_label(color)
            self._apply_light_background(r, g, b)
        self._safe_op("get_rgb_theme", "get_rgb_theme", on_ok=_on_theme)

    def _set_light_theme(self) -> None:
        mode = self.cmb_light_mode.currentData()
        color = self._current_light_color
        # cycle_time 固定 3000（ms），对齐设备端呼吸/触发模式周期
        cycle_timer_ms = 3000
        self._safe_op("set_rgb", "set_rgb", mode, cycle_timer_ms,
                      color.red(), color.green(), color.blue(),
                      on_ok=lambda _: self._apply_light_background(
                          color.red(), color.green(), color.blue()))

    def _toggle_quick_transfer(self) -> None:
        """快传开关：根据当前按钮文本切换开/关。"""
        opening = self.btn_quick_transfer.text() == "打开快传"
        self._safe_op("quick_transfer", "set_quick_transfer", opening,
                      on_ok=lambda _: self._on_quick_transfer_changed(opening))

    def _on_quick_transfer_changed(self, enabled: bool) -> None:
        self.btn_quick_transfer.setText("关闭快传" if enabled else "打开快传")
        self._log(f"快传已{'打开' if enabled else '关闭'}")

    def _get_unsynced_file_count(self) -> None:
        self._safe_op("get_unsynced_file_count", "get_unsynced_file_count",
                      on_ok=lambda n: self.lbl_unsynced_count.setText(str(n)))

    def _get_work_mode(self) -> None:
        def _on_mode(value: int):
            name = {1: "笔模式", 2: "鼠标模式"}.get(value, f"未知({value})")
            self.lbl_work_mode.setText(name)
        self._safe_op("get_work_mode", "get_work_mode", on_ok=_on_mode)

    def _read_touch_sensitivity(self) -> None:
        def _on_val(value: int):
            if 1 <= value <= 10:
                self.sp_touch_sensitivity.setValue(value)
            self._log(f"触摸灵敏度: {value}")
        self._safe_op("get_touch_sensitivity", "get_touch_sensitivity", on_ok=_on_val)

    def _set_touch_sensitivity(self) -> None:
        value = self.sp_touch_sensitivity.value()
        self._safe_op("set_touch_sensitivity", "set_touch_sensitivity", value,
                      on_ok=lambda _: self._log(f"触摸灵敏度已设置为 {value}"))

    def _read_slave_device_version(self) -> None:
        def _on_ver(items: list):
            if not items:
                self.lbl_slave_version.setText("无")
                self._log("从机版本号: 无")
                return
            parts = []
            for dev_id, ver in items:
                # 2 字节设备号 = [类型(高字节)][索引(低字节)]
                t = (dev_id >> 8) & 0xFF
                idx = dev_id & 0xFF
                parts.append(f"类型{t}#索引{idx}:{_format_version(ver)}")
            self.lbl_slave_version.setText("; ".join(parts))
            self._log("从机版本号: " + ", ".join(parts))
        self._safe_op("get_slave_device_version", "get_slave_device_version", on_ok=_on_ver)

    def _read_screen_sleep(self) -> None:
        def _on_val(value: int):
            self.sp_screen_sleep.setValue(value)
            self._log(f"屏幕休眠时间: {value} 秒")
        self._safe_op("get_slave_screen_sleep", "get_slave_screen_sleep", on_ok=_on_val)

    def _set_screen_sleep(self) -> None:
        seconds = self.sp_screen_sleep.value()
        self._safe_op("set_slave_screen_sleep", "set_slave_screen_sleep", seconds,
                      on_ok=lambda _: self._log(f"屏幕休眠时间已设置为 {seconds} 秒"))

    def _read_sleep_time(self) -> None:
        def _on_val(value: int):
            self.sp_sleep_time.setValue(value)
            self._log(f"设备休眠时间: {value} 秒")
        self._safe_op("get_sleep_time", "get_sleep_time", on_ok=_on_val)

    def _set_sleep_time(self) -> None:
        seconds = self.sp_sleep_time.value()
        self._safe_op("set_sleep_time", "set_sleep_time", seconds,
                      on_ok=lambda _: self._log(f"设备休眠时间已设置为 {seconds} 秒"))

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
    def _safe_op(self, op_id: str, method_name: str, *args,
                 on_ok=None) -> None:
        """派发后台操作，把 worker 的结果路由到本面板。"""
        # op_id 加 address 前缀保证多设备不冲突
        unique_op_id = f"{self._dev.address}:{op_id}"
        self._worker.async_op(unique_op_id, self._dev.address, method_name, *args)
        if on_ok is not None:
            self._pending_callbacks[unique_op_id] = on_ok

    def _set_combo_by_value(self, combo: QComboBox, value) -> None:
        idx = combo.findData(value)
        if idx >= 0:
            combo.setCurrentIndex(idx)
        else:
            self._info(f"值 {value} 不在可选范围")

    def _info(self, msg: str) -> None:
        QMessageBox.information(self, "提示", msg)

    def handle_result(self, op_id: str, result) -> None:
        """由 MainWindow 在 operation_result 信号里转发过来。"""
        cb = self._pending_callbacks.pop(op_id, None)
        if cb is not None:
            try:
                cb(result)
            except Exception as e:
                self._info(f"更新 UI 失败: {e}")

    def handle_error(self, op_id: str, message: str) -> None:
        self._pending_callbacks.pop(op_id, None)
        # 不弹窗，错误已由 MainWindow 写入底部日志

    def _log(self, msg: str, level: str = "INFO") -> None:
        """写入底部日志区（通过主窗口的 _log 方法）。"""
        if self._on_disconnect is not None and hasattr(self._on_disconnect, "_log"):
            self._on_disconnect._log(msg, level)
