"""私有协议帧打包/解包/校验。

帧结构（7 字节头 + payload + 4 字节 CRC32C）：
    [0]     magic    = 0xFE
    [1]     sequence = u8，发送端递增 mod 256
    [2]     cmd      = u8
    [3..7]  length   = u32 小端，payload 字节数
    [7..]   payload  = length 字节
    [7+L..] sign     = u32 小端，CRC32C(payload)

1:1 对齐 src_archive/windows/ProtocolLib/protocol_manager.c::get_protocol_command
与 src_archive/AM50/third/protocol/protocol_manager.c::prvGetProtocolCommand。
两处打包逻辑一致；解包对齐 protocol_manager.c::__is_full_pack / _parse_command
与 AM50 prvParsePack（带 CRC 校验）。
"""

from __future__ import annotations

import struct
from dataclasses import dataclass
from typing import List, Optional

COMMAND_MAGIC = 0xFE
_HEADER_SIZE = 7  # magic + sequence + cmd + length(4)
_SIGN_SIZE = 4


# ---------------------------------------------------------------------------
# CRC32C (Castagnoli, reflected, INIT/XOROUT=0xFFFFFFFF)
# 对齐 src_archive/mouseTool/mblue_crc.py::calculate_crc32 与
# src_archive/windows/.../mblue_crc.c::calculate_crc32。
# 用查表实现，表与 mblue_crc.py 的 crc32c_table 完全一致。
# ---------------------------------------------------------------------------

def _build_crc32c_table() -> List[int]:
    poly = 0x82F63B78  # CRC32C reflected polynomial
    table = []
    for i in range(256):
        crc = i
        for _ in range(8):
            crc = (crc >> 1) ^ poly if (crc & 1) else (crc >> 1)
        table.append(crc & 0xFFFFFFFF)
    return table


_CRC32C_TABLE = _build_crc32c_table()


def crc32c(data: bytes) -> int:
    """计算 CRC32C，与 C calculate_crc32 逐字节一致。"""
    crc = 0xFFFFFFFF
    for b in data:
        crc = _CRC32C_TABLE[(crc ^ b) & 0xFF] ^ (crc >> 8)
    return crc ^ 0xFFFFFFFF


# ---------------------------------------------------------------------------
# 序列号管理
# 对齐 protocol_manager.c::get_global_sequence：global_sequence++ % 256。
# 首帧 sequence=0。
# ---------------------------------------------------------------------------

class SequenceCounter:
    """帧序列号计数器，与 C 端 global_sequence 行为一致。"""

    def __init__(self) -> None:
        self._seq = 0

    def next(self) -> int:
        seq = self._seq % 256
        self._seq += 1
        return seq

    def reset(self) -> None:
        self._seq = 0


# ---------------------------------------------------------------------------
# 帧数据结构
# ---------------------------------------------------------------------------

@dataclass
class ProtocolFrame:
    magic: int
    sequence: int
    cmd: int
    payload: bytes
    # dongle 转发场景下标记来源子设备 mac（6 字节）；
    # 直连 / 设备自身帧为 None。多个子设备共用一个 dongle 通道时，
    # 靠它把应答帧隔离到各自设备，避免并发请求串台。
    source_mac: Optional[bytes] = None

    @property
    def total_length(self) -> int:
        # 头 + payload + (CRC if payload>0)
        if self.payload:
            return _HEADER_SIZE + len(self.payload) + _SIGN_SIZE
        return _HEADER_SIZE


# ---------------------------------------------------------------------------
# 打包
# 对齐 protocol_manager.c::get_protocol_command。
# ---------------------------------------------------------------------------

def build_frame(cmd: int, payload: bytes, sequence: int) -> bytes:
    """构造一个完整帧。

    payload 为空时不附加 CRC（与 C: total_size = HEADER；不写 sign 一致）。
    """
    if payload is None:
        payload = b""
    out = bytearray()
    out.append(COMMAND_MAGIC)
    out.append(sequence & 0xFF)
    out.append(cmd & 0xFF)
    out += struct.pack("<I", len(payload))
    if payload:
        out += payload
        out += struct.pack("<I", crc32c(payload))
    return bytes(out)


# ---------------------------------------------------------------------------
# 解包（流式状态机）
# 对齐 protocol_manager.c::__is_full_pack + _start_parse_service 的行为：
#   - 丢弃非 magic 字节
#   - 凑齐一帧后校验 CRC，CRC 错则丢弃该帧（AM50 prvParsePack 返回 NULL）
#   - 支持一次收到的字节流包含多帧 / 半帧
# ---------------------------------------------------------------------------

class ProtocolDecoder:
    """增量式协议解码器。

    用法：feed(chunk) 追加字节，take_frames() 取出已完成的完整帧。
    保留未消费的尾巴，等待下次 feed。CRC 校验失败的帧被丢弃，剩余字节继续解析。
    """

    def __init__(self) -> None:
        self._buf = bytearray()

    def feed(self, data: bytes) -> None:
        if data:
            self._buf += data

    def take_frames(self) -> List[ProtocolFrame]:
        frames: List[ProtocolFrame] = []
        while True:
            frame, consumed = self._try_parse_one()
            if frame is not None:
                frames.append(frame)
            if consumed <= 0:
                break
            del self._buf[:consumed]
        return frames

    def _try_parse_one(self) -> tuple[Optional[ProtocolFrame], int]:
        # 1. 丢弃非 magic 的前导字节（对齐 _start_parse_service 的 else 分支）
        idx = 0
        while idx < len(self._buf) and self._buf[idx] != COMMAND_MAGIC:
            idx += 1
        if idx > 0:
            # 先消费掉这些垃圾字节，本轮不再尝试解析（避免还要等更多数据）
            return None, idx
        if idx == len(self._buf):
            return None, 0

        buf = self._buf
        # 2. 头未到齐
        if len(buf) < _HEADER_SIZE:
            return None, 0

        magic = buf[0]
        sequence = buf[1]
        cmd = buf[2]
        length = struct.unpack_from("<I", buf, 3)[0]
        # 防御：length 异常大直接丢弃整个缓冲（避免恶意/错乱数据撑爆内存）
        if length > 0xFFFF:
            return None, len(buf)

        total = _HEADER_SIZE + length + (_SIGN_SIZE if length > 0 else 0)
        if len(buf) < total:
            return None, 0  # 等待更多数据

        payload = bytes(buf[_HEADER_SIZE:_HEADER_SIZE + length])
        if length > 0:
            sign = struct.unpack_from("<I", buf, _HEADER_SIZE + length)[0]
            # CRC 校验失败：丢弃这一帧（含头），对齐 AM50 prvParsePack 返回 NULL
            if crc32c(payload) != sign:
                return None, total
        # magic 仅用于解析入口判断，dataclass 仍记录以便上层排查
        return ProtocolFrame(magic, sequence, cmd, payload), total


def parse_frames(data: bytes) -> List[ProtocolFrame]:
    """一次性解析便利函数：从一段完整字节流中解析所有帧。"""
    decoder = ProtocolDecoder()
    decoder.feed(data)
    return decoder.take_frames()


def decode_ack_payload(payload: bytes) -> tuple[int, bytes]:
    """解析 ACK 帧 payload：(err_code, data)。

    对齐 AM50 protocol_manager.c::prvCommonAck / prvGetPropResp：
    payload = err_code(u32 LE) + data。
    """
    if len(payload) < 4:
        return 0, b""
    err_code = struct.unpack_from("<I", payload, 0)[0]
    return err_code, payload[4:]


def decode_event_payload(payload: bytes) -> tuple[int, int]:
    """解析 EVENT 帧 payload：(type, value)。

    对齐 AM50 prvGetEventPack：payload = type(u8) + value(u32 LE)。
    """
    if len(payload) < 5:
        return 0, 0
    type_ = payload[0]
    value = struct.unpack_from("<I", payload, 1)[0]
    return type_, value


def u32_to_bytes(value: int) -> bytes:
    """u32 小端打包。对齐 protocol_define.h::intToByte。"""
    return struct.pack("<I", value & 0xFFFFFFFF)


def bytes_to_u32(data: bytes, length: int = 4) -> int:
    """小端解包 1~4 字节为无符号整数。对齐 byteToInt。"""
    if length <= 0 or not data:
        return 0
    length = min(length, len(data), 4)
    value = 0
    for i in range(length):
        value |= data[i] << (8 * i)
    return value & 0xFFFFFFFF
