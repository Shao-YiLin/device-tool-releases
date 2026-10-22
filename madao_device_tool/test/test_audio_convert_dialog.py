"""audio_convert_dialog 的 GUI 层自动化验证（无头跑 Qt 事件循环）。"""
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


def check(name, cond):
    print(("[PASS] " if cond else "[FAIL] ") + name)
    if not cond:
        failures.append(name)


def run():
    app = QApplication(sys.argv)
    dlg = AudioConvertDialog()
    dlg.show()

    opus = os.path.join(_PROJECT_ROOT, "records", "CALL_20260920_073047.opus")
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
    return len(failures) == 0


if __name__ == "__main__":
    ok = run()
    print("RESULT:", "PASS" if ok else "FAIL")
    sys.exit(0 if ok else 1)
