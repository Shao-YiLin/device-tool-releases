"""测试 DeviceTool.connect_ble_bonded 的完整路径（包含 handshake）。"""
import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from devtool import DeviceTool

MAC = "FF:49:B0:A7:81:2C"

def main():
    tool = DeviceTool()
    try:
        print("connecting via DeviceTool...")
        dev = tool.connect_ble_bonded(MAC, device_type=0x32, name="T8")
        print("connected device:", dev.name, dev.address, type(dev).__name__)
        print("is_connected:", dev.is_connected)
        print("product_no:", dev.product_no)
    except Exception as e:
        print("ERROR:", type(e).__name__, e)
        import traceback
        traceback.print_exc()
    finally:
        try:
            tool.close()
        except Exception as e:
            print("close error:", e)

if __name__ == "__main__":
    main()
