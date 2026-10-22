"""在 PySide6 QThread 环境中测试 DeviceTool.connect_ble_bonded。"""
import sys
import os
import traceback

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from PySide6.QtCore import QObject, QThread, Signal, Slot, QCoreApplication
from devtool import DeviceTool

MAC = "FF:49:B0:A7:81:2C"


class Worker(QObject):
    result = Signal(object)

    @Slot()
    def do_connect(self):
        try:
            tool = DeviceTool()
            dev = tool.connect_ble_bonded(MAC, device_type=0x32, name="T8")
            self.result.emit({
                "ok": True,
                "name": dev.name,
                "connected": dev.is_connected,
                "type": type(dev).__name__,
            })
        except Exception as e:
            traceback.print_exc()
            self.result.emit({"ok": False, "error": str(e)})


def main():
    app = QCoreApplication(sys.argv)
    thread = QThread()
    worker = Worker()
    worker.moveToThread(thread)
    thread.start()

    def on_result(r):
        print("result:", r)
        app.quit()

    worker.result.connect(on_result)
    # 用 QTimer 让事件循环启动后再触发
    from PySide6.QtCore import QTimer
    QTimer.singleShot(100, worker.do_connect)
    app.exec()
    thread.quit()
    thread.wait(2000)


if __name__ == "__main__":
    main()
