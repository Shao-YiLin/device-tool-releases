"""测试 WinrtBleComm 在 Python 3.11 + winrt 3.0+ 下的 notify 接收。"""
import sys
import os
import time
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from comm_lib.channels.winrt_ble_comm import WinrtBleComm
from comm_lib.protocol.frame import build_frame
from comm_lib.protocol.cmd import Command

MAC = "FF:49:B0:A7:81:2C"


def main():
    comm = WinrtBleComm(uuid_set="telink")
    try:
        print("connecting...")
        comm.connect(MAC)
        print("connected:", comm.is_connected)

        # 注册一个原始帧回调
        received = []
        def on_frame(frame):
            print(f"  [on_frame] cmd={frame.cmd:#04x} seq={frame.sequence} payload={frame.payload.hex() if frame.payload else ''}")
            received.append(frame)
        comm.on_frame = on_frame

        # 发送 handshake 帧
        frame = build_frame(Command.HANDSHAKE, b"", 0)
        print("sending handshake:", frame.hex())
        n = comm.send_raw(frame)
        print("send_raw returned:", n)

        # 等待应答
        print("waiting 5s for notify...")
        time.sleep(5.0)
        print("received frames:", len(received))
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
