"""comm_lib.protocol 单元测试。

测试向量来源：
- CRC32C 标准向量 '123456789' -> 0xE3069283（RFC 3720 / ITU-T V.42）
- 帧结构对照 C 源码 protocol_manager.c::get_protocol_command 手工推导
- mouseTool/mspeech_usb.py 的示例帧 b'\\xfe\\x09\\x06...' 反推

运行：python -m pytest new_project/test/test_protocol.py
或独立运行：python new_project/test/test_protocol.py
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from comm_lib.protocol.frame import (
    COMMAND_MAGIC,
    ProtocolDecoder,
    SequenceCounter,
    build_frame,
    bytes_to_u32,
    crc32c,
    decode_ack_payload,
    decode_event_payload,
    parse_frames,
    u32_to_bytes,
)
from comm_lib.protocol.cmd import Command, EventType, UsbManagerCommand
from comm_lib.protocol.prop_key import PropKey


def test_crc32c_standard_vector():
    # CRC32C('123456789') == 0xE3069283 是业界标准向量
    assert crc32c(b"123456789") == 0xE3069283


def test_crc32c_empty():
    # 空 payload 的 CRC 与 C calculate_crc32(buf,0) 一致：INIT^XOROT = 0
    assert crc32c(b"") == 0


def test_crc32c_short():
    # 对照 mouseTool/mblue_crc.py 样例：b=[1,2,3,4,5,6], size=4
    assert crc32c(bytes([1, 2, 3, 4])) == 0x29308CF4


def test_sequence_counter_wraps_and_starts_at_zero():
    seq = SequenceCounter()
    # 对齐 C: get_global_sequence -> global_sequence++ % 256，首帧为 0
    assert seq.next() == 0
    assert seq.next() == 1
    seq._seq = 255
    assert seq.next() == 255  # 255 % 256
    assert seq.next() == 0    # 回绕


def test_build_frame_without_payload_matches_c_layout():
    # 对照 protocol_manager.c::get_protocol_command: length==0 时 total=7，不写 CRC
    frame = build_frame(Command.HANDSHAKE, b"", sequence=0x05)
    assert frame == bytes([COMMAND_MAGIC, 0x05, Command.HANDSHAKE, 0x00, 0x00, 0x00, 0x00])


def test_build_frame_with_payload_matches_c_layout():
    # payload=[DPI, level(4 LE)]，校验 magic/seq/cmd/length/payload/sign 顺序与小端
    payload = bytes([PropKey.DPI]) + u32_to_bytes(3)
    frame = build_frame(Command.SET_SLAVE_PROP, payload, sequence=0x09)
    assert frame[0] == COMMAND_MAGIC
    assert frame[1] == 0x09
    assert frame[2] == Command.SET_SLAVE_PROP
    assert frame[3:7] == (5).to_bytes(4, "little")
    assert frame[7:12] == payload
    assert frame[12:16] == crc32c(payload).to_bytes(4, "little")
    assert len(frame) == 16


def test_build_then_decode_roundtrip():
    payload = bytes([PropKey.POLLING_RATE]) + u32_to_bytes(500)
    seq = 0x21
    frame = build_frame(Command.SET_SLAVE_PROP, payload, seq)
    decoded = parse_frames(frame)
    assert len(decoded) == 1
    f = decoded[0]
    assert f.sequence == seq
    assert f.cmd == Command.SET_SLAVE_PROP
    assert f.payload == payload


def test_decode_real_world_sample_frame():
    # 来自 src_archive/mouseTool/mspeech_usb.py 与 mspeech_ble.py 的真实示例帧：
    #   b'\xfe\x09\x06\x01\x00\x00\x00\x0b\x6a\xf8\xf4\xcb'
    # 拆解：magic=FE seq=09 cmd=06(GET_SLAVE_PROP) len=1 payload=[0B(BATTERY)]
    #       sign = crc32c(b'\x0b') = 0xcbf4f86a -> 小端 6a f8 f4 cb
    sample = b"\xfe\x09\x06\x01\x00\x00\x00\x0b\x6a\xf8\xf4\xcb"
    assert crc32c(b"\x0b") == 0xCBF4F86A
    frames = parse_frames(sample)
    assert len(frames) == 1
    f = frames[0]
    assert f.cmd == Command.GET_SLAVE_PROP
    assert f.sequence == 0x09
    assert f.payload == bytes([PropKey.BATTERY])


def test_decoder_handles_streamed_chunks():
    # 模拟 USB/BLE 读取分片到达
    payload = bytes([PropKey.DPI]) + u32_to_bytes(2)
    frame = build_frame(Command.SET_SLAVE_PROP, payload, 0x10)
    dec = ProtocolDecoder()
    out = []
    # 按任意切点喂入
    cuts = [3, 0, 5, len(frame) - 8, 8]
    i = 0
    for c in cuts:
        dec.feed(frame[i:i + c])
        out.extend(dec.take_frames())
        i += c
    dec.feed(frame[i:])
    out.extend(dec.take_frames())
    assert len(out) == 1
    assert out[0].payload == payload


def test_decoder_drops_garbage_before_magic():
    payload = bytes([PropKey.VERSION_MOURSE])
    frame = build_frame(Command.GET_SLAVE_PROP, payload, 0x01)
    garbage = b"\x00\x11\x22"
    dec = ProtocolDecoder()
    dec.feed(garbage + frame)
    frames = dec.take_frames()
    assert len(frames) == 1
    assert frames[0].cmd == Command.GET_SLAVE_PROP


def test_decoder_drops_frame_with_bad_crc():
    # 篡改 payload 一个字节 -> CRC 不匹配，该帧被丢弃，缓冲清空
    payload = bytes([PropKey.DPI]) + u32_to_bytes(1)
    frame = bytearray(build_frame(Command.SET_SLAVE_PROP, payload, 0x02))
    frame[7] ^= 0xFF  # 破坏 payload
    dec = ProtocolDecoder()
    dec.feed(bytes(frame))
    assert dec.take_frames() == []


def test_decoder_handles_two_back_to_back_frames():
    f1 = build_frame(Command.GET_SLAVE_PROP, bytes([PropKey.DPI]), 0x01)
    f2 = build_frame(Command.GET_SLAVE_PROP, bytes([PropKey.MAC_MOURSE]), 0x02)
    dec = ProtocolDecoder()
    dec.feed(f1 + f2)
    frames = dec.take_frames()
    assert len(frames) == 2
    assert frames[0].sequence == 0x01 and frames[0].payload == bytes([PropKey.DPI])
    assert frames[1].sequence == 0x02 and frames[1].payload == bytes([PropKey.MAC_MOURSE])


def test_decode_ack_payload():
    # ACK payload = err_code(4 LE) + data
    err, data = decode_ack_payload((1).to_bytes(4, "little") + b"\x02\x00\x00\x00")
    assert err == 1
    assert bytes_to_u32(data) == 2


def test_decode_event_payload():
    type_, value = decode_event_payload(bytes([EventType.BATTERY]) + u32_to_bytes(80))
    assert type_ == EventType.BATTERY
    assert value == 80


def test_bytes_to_u32_partial():
    # 对齐 C byteToInt 的 1/2/3 字节分支
    assert bytes_to_u32(b"\x05", 1) == 5
    assert bytes_to_u32(b"\x01\x02", 2) == 0x0201
    assert bytes_to_u32(b"\x01\x02\x03", 3) == 0x030201


def test_u32_to_bytes_little_endian():
    assert u32_to_bytes(0x04030201) == b"\x01\x02\x03\x04"


def test_command_constants_match_c_source():
    # 锁定关键 cmd 值，防止误改
    assert Command.SET_SLAVE_PROP == 0x05
    assert Command.GET_SLAVE_PROP == 0x06
    assert Command.ACK == 0x81
    assert Command.EVENT == 0x09
    assert UsbManagerCommand.SCAN_DEVICE == 0x41
    assert UsbManagerCommand.DEVICE_MANAGER == 0x48


def test_prop_key_constants_match_c_source():
    assert PropKey.DPI == 0x02
    assert PropKey.POLLING_RATE == 0x1B
    assert PropKey.VERSION_MOURSE == 0x09
    assert PropKey.MAC_MOURSE == 0x0A
    assert PropKey.BATTERY == 0x0B
    assert PropKey.PRODUCT_NO == 0x03


if __name__ == "__main__":
    fns = [v for k, v in sorted(globals().items()) if k.startswith("test_") and callable(v)]
    passed = 0
    failed = []
    for fn in fns:
        try:
            fn()
            passed += 1
            print(f"PASS {fn.__name__}")
        except AssertionError as e:
            failed.append((fn.__name__, str(e)))
            print(f"FAIL {fn.__name__}: {e}")
    print(f"\n{passed}/{len(fns)} passed")
    if failed:
        raise SystemExit(1)
