"""在独立线程里调用 DeviceTool.connect_ble_bonded，模拟 GUI worker。"""
import sys
import os
import threading
import traceback

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from devtool import DeviceTool

MAC = "FF:49:B0:A7:81:2C"


def worker(result):
    try:
        tool = DeviceTool()
        dev = tool.connect_ble_bonded(MAC, device_type=0x32, name="T8")
        result["ok"] = True
        result["dev"] = dev
        result["name"] = dev.name
        result["connected"] = dev.is_connected
    except Exception as e:
        result["ok"] = False
        result["error"] = str(e)
        traceback.print_exc()


def main():
    result = {}
    t = threading.Thread(target=worker, args=(result,), daemon=True)
    t.start()
    t.join(timeout=30.0)
    print("result:", result)


if __name__ == "__main__":
    main()
