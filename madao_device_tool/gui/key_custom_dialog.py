"""按键自定义对话框（PySide6）。

功能对齐参考项目 ui/key_custom_dialog.py：
- 6 个按键选择（左/右/中/侧1/侧2/语音）
- 7 种功能模式（默认/键盘按键/consume/鼠标按键/DPI/灯光/禁用）
- 读取/写入/读取全部
- 数据编辑（flag/type/data）

通过 DeviceWorker 异步派发 KbDevice.get/set_custom_button 调用，
结果经 Signal 回填 UI。
"""

from __future__ import annotations

from typing import Optional

from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import (
    QButtonGroup,
    QCheckBox,
    QComboBox,
    QDialog,
    QFormLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMessageBox,
    QPushButton,
    QSpinBox,
    QTextEdit,
    QVBoxLayout,
    QWidget,
)

from key_codes import (
    CONSUME_KEY_CODE,
    CONTROL_KEYS,
    DEFAULT_DATA,
    DEFAULT_BUTTON_MODE,
    DPI_BUTTON_CHANGE,
    DPI_OPTIONS,
    FORBIDDEN_BUTTON,
    KEY_BUTTON_NAMES,
    KEY_MAP,
    MOUSE_KEY_CODE,
    NORMAL_BUTTON_KEYBOARD,
    NORMAL_BUTTON_MOUSE,
    RGB_BUTTON_CHANGE,
)


class KeyCustomDialog(QDialog):
    """按键自定义独立对话框。

    Args:
        dev: KbDevice 实例
        worker: DeviceWorker（用于异步操作）
        parent: 父窗口
    """

    op_result = Signal(str, object)

    def __init__(self, dev, worker, parent: Optional[QWidget] = None) -> None:
        super().__init__(parent)
        self._dev = dev
        self._worker = worker
        self._pending: dict[str, callable] = {}

        self.setWindowTitle("按键自定义")
        self.resize(780, 560)

        # 每个按键 6 组数据缓存：flag, type, data[8]
        self._flags = [DEFAULT_BUTTON_MODE] * 6
        self._types = [0] * 6
        self._datas = [list(DEFAULT_DATA) for _ in range(6)]
        self._current_index = 0
        self._capturing_key = False
        self._normal_key_code: Optional[int] = None

        self._build_ui()
        self._update_data_display()

    # ---- UI ----
    def _build_ui(self) -> None:
        root = QVBoxLayout(self)

        top = QHBoxLayout()
        root.addLayout(top, 4)

        # 左：按键选择
        left = QGroupBox("按键选择")
        lv = QVBoxLayout(left)
        self._key_btn_group = QButtonGroup(self)
        self._key_btn_group.setExclusive(True)
        self._key_buttons: list[QPushButton] = []
        for i, name in enumerate(KEY_BUTTON_NAMES):
            btn = QPushButton(name)
            btn.setCheckable(True)
            btn.clicked.connect(lambda checked, idx=i: self._on_key_select(idx))
            self._key_btn_group.addButton(btn, i)
            lv.addWidget(btn)
            self._key_buttons.append(btn)
        self._key_buttons[0].setChecked(True)
        lv.addStretch()
        top.addWidget(left, 1)

        # 中：功能设置
        mid = QGroupBox("功能设置")
        mv = QVBoxLayout(mid)

        self.btn_default = QPushButton("默认功能")
        self.btn_default.clicked.connect(lambda: self._apply_mode(DEFAULT_BUTTON_MODE))
        mv.addWidget(self.btn_default)

        # 键盘按键
        kb_group = QGroupBox("键盘按键")
        kb_form = QFormLayout(kb_group)
        self.chk_capture = QCheckBox("普通按键（勾选后按键捕获）")
        self.chk_capture.toggled.connect(self._on_capture_toggled)
        kb_form.addRow(self.chk_capture)
        self.le_normal_key = QLineEdit()
        self.le_normal_key.setReadOnly(True)
        kb_form.addRow("捕获按键:", self.le_normal_key)
        ctrl_row = QHBoxLayout()
        self._ctrl_vars: list[QCheckBox] = []
        for text, _ in CONTROL_KEYS:
            cb = QCheckBox(text)
            self._ctrl_vars.append(cb)
            ctrl_row.addWidget(cb)
        ctrl_row.addStretch()
        kb_form.addRow("控制键:", None)
        kb_form.addRow(ctrl_row)
        cycle_row = QHBoxLayout()
        cycle_row.addWidget(QLabel("循环"))
        self.sp_cycle = QSpinBox()
        self.sp_cycle.setRange(0, 20)
        cycle_row.addWidget(self.sp_cycle)
        cycle_row.addWidget(QLabel("次/秒(最大20)"))
        self.btn_set_key = QPushButton("设置")
        self.btn_set_key.clicked.connect(self._on_set_keyboard)
        cycle_row.addWidget(self.btn_set_key)
        kb_form.addRow(cycle_row)
        mv.addWidget(kb_group)

        # Consume 按键
        consume_group = QGroupBox("Consume 按键")
        cv = QVBoxLayout(consume_group)
        self.cmb_consume = QComboBox()
        for name in CONSUME_KEY_CODE:
            self.cmb_consume.addItem(name)
        self.cmb_consume.currentIndexChanged.connect(self._on_consume_select)
        cv.addWidget(self.cmb_consume)
        mv.addWidget(consume_group)

        # 鼠标按键
        mouse_group = QGroupBox("鼠标按键")
        mcv = QVBoxLayout(mouse_group)
        self.cmb_mouse = QComboBox()
        for name in MOUSE_KEY_CODE:
            self.cmb_mouse.addItem(name)
        self.cmb_mouse.currentIndexChanged.connect(self._on_mouse_select)
        mcv.addWidget(self.cmb_mouse)
        mv.addWidget(mouse_group)

        # DPI
        dpi_group = QGroupBox("DPI 设置")
        dv = QVBoxLayout(dpi_group)
        self.cmb_dpi = QComboBox()
        for name in DPI_OPTIONS:
            self.cmb_dpi.addItem(name)
        self.cmb_dpi.currentIndexChanged.connect(self._on_dpi_select)
        dv.addWidget(self.cmb_dpi)
        mv.addWidget(dpi_group)

        # 灯光 / 禁用
        self.btn_light = QPushButton("灯光切换")
        self.btn_light.clicked.connect(lambda: self._apply_mode(RGB_BUTTON_CHANGE))
        mv.addWidget(self.btn_light)
        self.btn_disable = QPushButton("禁用按键")
        self.btn_disable.clicked.connect(lambda: self._apply_mode(FORBIDDEN_BUTTON))
        mv.addWidget(self.btn_disable)
        mv.addStretch()
        top.addWidget(mid, 3)

        # 右：数据编辑
        right = QGroupBox("数据编辑")
        rv = QFormLayout(right)
        self.le_flag = QLineEdit()
        self.le_type = QLineEdit()
        rv.addRow("Flag:", self.le_flag)
        rv.addRow("Type:", self.le_type)
        data_row = QHBoxLayout()
        self.le_data: list[QLineEdit] = []
        for j in range(8):
            le = QLineEdit()
            le.setMaxLength(4)
            le.setFixedWidth(40)
            self.le_data.append(le)
            data_row.addWidget(le)
        data_row.addStretch()
        rv.addRow("Data:", data_row)
        top.addWidget(right, 2)

        # 底部操作
        btn_row = QHBoxLayout()
        self.btn_read = QPushButton("读取")
        self.btn_read.clicked.connect(self._on_read)
        self.btn_write = QPushButton("写入")
        self.btn_write.clicked.connect(self._on_write)
        self.btn_read_all = QPushButton("读取全部")
        self.btn_read_all.clicked.connect(self._on_read_all)
        self.btn_close = QPushButton("关闭")
        self.btn_close.clicked.connect(self.accept)
        btn_row.addWidget(self.btn_read)
        btn_row.addWidget(self.btn_write)
        btn_row.addWidget(self.btn_read_all)
        btn_row.addStretch()
        btn_row.addWidget(self.btn_close)
        root.addLayout(btn_row)

    # ---- 按键选择 ----
    def _on_key_select(self, idx: int) -> None:
        self._current_index = idx
        self._update_data_display()

    # ---- 按键捕获 ----
    def _on_capture_toggled(self, checked: bool) -> None:
        self._capturing_key = checked
        if checked:
            self.grabKeyboard()
            self.setFocus()
        else:
            self.releaseKeyboard()

    def keyPressEvent(self, event) -> None:
        if not self._capturing_key:
            super().keyPressEvent(event)
            return
        native = event.nativeVirtualKey()
        if native in KEY_MAP:
            info = KEY_MAP[native]
            name = list(info.keys())[0]
            code = list(info.values())[0]
            self._normal_key_code = code
            self.le_normal_key.setText(name)
        self.chk_capture.setChecked(False)
        self._capturing_key = False
        self.releaseKeyboard()

    # ---- 功能模式应用 ----
    def _apply_mode(self, mode: int, key_type: int = 0,
                    data: Optional[list] = None) -> None:
        idx = self._current_index
        self._flags[idx] = mode
        self._types[idx] = key_type
        self._datas[idx] = list(data) if data else list(DEFAULT_DATA)
        self._send_key_data()
        self._update_data_display()

    def _on_set_keyboard(self) -> None:
        idx = self._current_index
        control = 0
        for i, cb in enumerate(self._ctrl_vars):
            if cb.isChecked():
                control |= (1 << i)
        data = list(DEFAULT_DATA)
        data[0] = control
        data[1] = min(self.sp_cycle.value(), 20)
        data[2] = self._normal_key_code or 0
        self._apply_mode(NORMAL_BUTTON_KEYBOARD, 0, data)

    def _on_consume_select(self) -> None:
        name = self.cmb_consume.currentText()
        if not name:
            return
        vals = CONSUME_KEY_CODE[name]
        data = list(DEFAULT_DATA)
        data[:2] = list(vals)
        self._apply_mode(NORMAL_BUTTON_KEYBOARD, 1, data)

    def _on_mouse_select(self) -> None:
        name = self.cmb_mouse.currentText()
        if not name:
            return
        mtype, mdata = MOUSE_KEY_CODE[name]
        data = list(DEFAULT_DATA)
        data[:4] = list(mdata)
        self._apply_mode(NORMAL_BUTTON_MOUSE, mtype, data)

    def _on_dpi_select(self) -> None:
        name = self.cmb_dpi.currentText()
        if not name:
            return
        self._apply_mode(DPI_BUTTON_CHANGE, DPI_OPTIONS.index(name))

    # ---- 读取/写入 ----
    def _on_read(self) -> None:
        idx = self._current_index
        op_id = f"keycustom_read:{idx}"
        self._pending[op_id] = self._on_read_result
        self._worker.async_op(f"{self._dev.address}:{op_id}",
                              self._dev.address, "get_custom_button", idx)

    def _on_read_all(self) -> None:
        for i in range(6):
            op_id = f"keycustom_read:{i}"
            self._pending[op_id] = self._on_read_result
            self._worker.async_op(f"{self._dev.address}:{op_id}",
                                  self._dev.address, "get_custom_button", i)

    def _on_read_result(self, result) -> None:
        """get_custom_button 返回 (flag, key_type, data(8字节))。"""
        # 注意：op_id 在 MainWindow 中已加 address 前缀，这里 result 是原始返回
        flag, key_type, data = result
        idx = self._current_index
        self._flags[idx] = flag
        self._types[idx] = key_type
        self._datas[idx] = list(data[:8])
        self._update_data_display()

    def _on_read_result_by_idx(self, idx: int, result) -> None:
        flag, key_type, data = result
        self._flags[idx] = flag
        self._types[idx] = key_type
        self._datas[idx] = list(data[:8])
        if idx == self._current_index:
            self._update_data_display()

    def _on_write(self) -> None:
        idx = self._current_index
        try:
            self._flags[idx] = self._parse_int(self.le_flag.text(), self._flags[idx])
            self._types[idx] = self._parse_int(self.le_type.text(), self._types[idx])
            for i, le in enumerate(self.le_data):
                self._datas[idx][i] = self._parse_int(le.text(), self._datas[idx][i])
        except ValueError as e:
            QMessageBox.warning(self, "输入错误", str(e))
            return
        self._send_key_data()

    def _send_key_data(self) -> None:
        idx = self._current_index
        data = bytes(self._datas[idx])
        op_id = f"keycustom_write:{idx}"
        self._worker.async_op(f"{self._dev.address}:{op_id}",
                              self._dev.address, "set_custom_button",
                              idx, self._flags[idx], self._types[idx], data)

    # ---- 数据显示 ----
    def _update_data_display(self) -> None:
        idx = self._current_index
        self.le_flag.setText(f"0x{self._flags[idx]:02X}")
        self.le_type.setText(f"0x{self._types[idx]:02X}")
        for i, le in enumerate(self.le_data):
            le.setText(f"0x{self._datas[idx][i]:02X}")

    # ---- 结果回调路由 ----
    def handle_result(self, op_id: str, result) -> None:
        """由 MainWindow 转发 operation_result。

        op_id 格式：{address}:keycustom_read:{idx} 或 {address}:keycustom_write:{idx}
        """
        # 去掉 address 前缀
        prefix = f"{self._dev.address}:"
        if op_id.startswith(prefix):
            op_id = op_id[len(prefix):]
        if op_id.startswith("keycustom_read:"):
            idx = int(op_id.split(":")[1])
            self._on_read_result_by_idx(idx, result)
        elif op_id.startswith("keycustom_write:"):
            pass  # 写入无需特殊处理

    def handle_error(self, op_id: str, message: str) -> None:
        prefix = f"{self._dev.address}:"
        if op_id.startswith(prefix):
            op_id = op_id[len(prefix):]
        self._pending.pop(op_id, None)

    # ---- 辅助 ----
    @staticmethod
    def _parse_int(text: str, default: int) -> int:
        text = text.strip()
        if not text:
            return default
        if text.lower().startswith("0x"):
            return int(text, 16)
        return int(text)
