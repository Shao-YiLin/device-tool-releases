"""真机联调 demo 脚本。

使用步骤：
1. 安装依赖：pip install -r madao_device_tool/requirements.txt
2. （USB）Windows 下需要 Zadig 把鼠标的 interface 1 替换成 WinUSB / libusb 驱动
3. 运行：python madao_device_tool/demo_connect_mouse.py

本脚本会：
- 扫描 BLE / USB 设备并打印
- 连接第一个扫描到的鼠标
- 读取 product_no / mac / version / battery
- 读取并设置 CPI / 回报率
- 断开
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from devtool import DeviceTool
from device_model import MouseDevice


def main():
    tool = DeviceTool()
    try:
        print("== 扫描 BLE ==")
        ble = tool.scan_ble(timeout=5.0)
        for i in ble:
            print("  ", i)

        print("== 扫描 USB 直连 ==")
        usb = tool.scan_usb()
        for i in usb:
            print("  ", i)

        # 优先 USB 直连（BLE 在 Windows 上需要先配对 bonding，见 README）
        target = usb[0] if usb else (ble[0] if ble else None)
        if target is None:
            print("未扫描到任何设备")
            return

        print(f"\n== 连接 {target} ==")
        dev = tool.connect(target)
        print(f"已连接: {type(dev).__name__}")

        try:
            print("product_no:", dev.get_product_no())
        except Exception as e:
            print("product_no 失败:", e)
        try:
            print("mac:", dev.get_device_mac().hex(":"))
        except Exception as e:
            print("mac 失败:", e)
        try:
            print("version:", hex(dev.get_device_version()))
        except Exception as e:
            print("version 失败:", e)
        try:
            print("battery:", dev.get_battery())
        except Exception as e:
            print("battery 失败:", e)

        if isinstance(dev, MouseDevice):
            try:
                print("当前 CPI:", dev.get_cpi())
                dev.set_cpi(2)
                print("设置 CPI=2 后:", dev.get_cpi())
            except Exception as e:
                print("CPI 失败:", e)
            try:
                print("当前回报率:", dev.get_rate())
                dev.set_rate(500)
                print("设置回报率=500 后:", dev.get_rate())
            except Exception as e:
                print("回报率失败:", e)

    finally:
        print("\n== 断开所有设备 ==")
        tool.close()


if __name__ == "__main__":
    main()