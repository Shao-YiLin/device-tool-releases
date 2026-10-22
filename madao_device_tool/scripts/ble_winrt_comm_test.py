"""直接测试 madao_device_tool 的 WinrtBleComm 类。"""
import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from comm_lib.channels.winrt_ble_comm import WinrtBleComm

MAC = "FF:49:B0:A7:81:2C"

def main():
    comm = WinrtBleComm(uuid_set="telink")
    try:
        print("connecting...")
        comm.connect(MAC)
        print("connected:", comm.is_connected)
        frame = bytes([0xFE, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00])
        print("sending frame:", frame.hex())
        n = comm.send_raw(frame)
        print("send_raw returned:", n)
    except Exception as e:
        print("ERROR:", type(e).__name__, e)
        import traceback
        traceback.print_exc()
    finally:
        try:
            comm.disconnect()
        except Exception as e:
            print("disconnect error:", e)

if __name__ == "__main__":
    main()
