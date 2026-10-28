"""audio_convert_dialog 的 GUI 层自动化验证（无头跑 Qt 事件循环）。

输入 .opus 文件优先用本地已有录音（records/CALL_*.opus）；
缺失时用 libopus 编码器现场合成同格式的裸包文件（40B 定长分帧），
保证测试不依赖本地历史数据。
"""
import ctypes
import os
import sys

_PKG_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_GUI_DIR = os.path.join(_PKG_ROOT, "gui")
sys.path.insert(0, _PKG_ROOT)
sys.path.insert(0, _GUI_DIR)
_PROJECT_ROOT = os.path.dirname(_PKG_ROOT)

import _bootstrap  # noqa: F401

from PySide6.QtCore import QTimer
from PySide6.QtWidgets import QApplication

from audio_convert_dialog import AudioConvertDialog

failures = []
_SYNTHESIZED = False


def _ensure_opus_input() -> str:
    """返回可用的 .opus 输入路径；本地没有录音样本时现场合成一个。"""
    records_dir = os.path.join(_PKG_ROOT, "records")
    if os.path.isdir(records_dir):
        for name in sorted(os.listdir(records_dir)):
            if name.endswith(".opus"):
                return os.path.join(records_dir, name)
    # 现场合成：libopus 编码 10 帧静音（16kbps/20ms），按设备格式
    # 补齐为 40B/包（不足填零，code-0 单帧包帧长=剩余全部，可正常解码）
    from audio.opus_pcm import _load_lib, _SAMPLE_RATE
    dll = _load_lib()
    dll.opus_encoder_create.restype = ctypes.c_void_p
    dll.opus_encoder_create.argtypes = [ctypes.c_int, ctypes.c_int,
                                        ctypes.c_int, ctypes.c_void_p]
    dll.opus_encoder_destroy.argtypes = [ctypes.c_void_p]
    dll.opus_encoder_ctl.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p]
    dll.opus_encode.restype = ctypes.c_int
    dll.opus_encode.argtypes = [ctypes.c_void_p, ctypes.c_void_p,
                                ctypes.c_int, ctypes.c_void_p, ctypes.c_int]
    err = ctypes.c_int(0)
    enc = dll.opus_encoder_create(_SAMPLE_RATE, 1, 2048, ctypes.byref(err))
    assert err.value == 0, f"opus 编码器创建失败 err={err.value}"
    try:
        bitrate = ctypes.c_int(16000)
        dll.opus_encoder_ctl(enc, 4002, ctypes.byref(bitrate))
        pcm = (ctypes.c_int16 * 320)()  # 20ms @16kHz 静音
        out = ctypes.create_string_buffer(400)
        data = bytearray()
        for _ in range(10):
            n = dll.opus_encode(enc, pcm, 320, out, 400)
            if n > 0:
                data.extend(out.raw[:n].ljust(40, b"\x00"))
    finally:
        dll.opus_encoder_destroy(enc)
    os.makedirs(records_dir, exist_ok=True)
    path = os.path.join(records_dir, "CALL_TEST_SYNTHESIZED.opus")
    with open(path, "wb") as f:
        f.write(bytes(data))
    global _SYNTHESIZED
    _SYNTHESIZED = True
    print(f"[INFO] 本地无录音样本，已合成测试输入: {path}")
    return path


def check(name, cond):
    print(("[PASS] " if cond else "[FAIL] ") + name)
    if not cond:
        failures.append(name)


def run():
    app = QApplication(sys.argv)
    dlg = AudioConvertDialog()
    dlg.show()

    opus = _ensure_opus_input()
    check("opus 文件存在", os.path.isfile(opus))

    done = {"result": None, "finished": False}

    dlg.le_input.setText(opus)
    dlg._log("TEST: 开始")

    def start():
        dlg.btn_convert.setEnabled(True)
        dlg._on_convert()

    def poll():
        if "转换成功" in dlg.log_text.toPlainText() or done["finished"]:
            check("完成回调触发", done["finished"])
            check("日志含转换成功", "转换成功" in dlg.log_text.toPlainText())
            check("按钮恢复", dlg.btn_convert.isEnabled())
            pcm = os.path.splitext(opus)[0] + ".pcm"
            check("pcm 已生成", os.path.isfile(pcm))
            app.quit()
        else:
            QTimer.singleShot(300, poll)

    dlg._sig_done.connect(lambda r: done.update(result=r, finished=True))
    QTimer.singleShot(100, start)
    QTimer.singleShot(200, poll)
    QTimer.singleShot(30000, app.quit)  # 兜底超时
    app.exec()
    if _SYNTHESIZED:
        # 清理合成文件（.opus 输入 + 转换产物 .pcm），不污染 records/
        for suffix in (".opus", ".pcm"):
            try:
                os.remove(os.path.splitext(opus)[0] + suffix)
            except OSError:
                pass
    return len(failures) == 0


if __name__ == "__main__":
    ok = run()
    print("RESULT:", "PASS" if ok else "FAIL")
    sys.exit(0 if ok else 1)
