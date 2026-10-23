"""连接 AM60 并打印 product_no 等基础信息。"""
import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from devtool import DeviceTool

MAC = "FF:49:3C:2B:3A:05"


def main():
    tool = DeviceTool()
    try:
        # 默认按鼠标连接
        dev = tool.connect_ble_bonded(MAC, device_type=0x31, name="AM60")
        print("device type:", type(dev).__name__)
        print("connected:", dev.is_connected)
        pn = dev.get_product_no()
        print("product_no raw:", pn)
        print("product_no str:", pn.decode("ascii", errors="replace"))
        mac = dev.get_device_mac()
        print("mac:", mac.hex(":"))
        ver = dev.get_device_version()
        print("version raw:", ver.hex() if isinstance(ver, bytes) else ver)
        bat = dev.get_battery()
        print("battery:", bat)
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
