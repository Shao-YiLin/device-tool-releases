"""OTA 升级流程编排。

对齐 src_archive/windows/libProtocolFramework/lib_device_manager.c 的 OTA 状态机
与 AM50/vendor/8258_dual_mouse/logic_protocol_manager.c::_ota_logic。

协议（设备端 WRITE_DATA 命令，payload=[type=1][ota_type][data]）：
  START_OTA: [1][1][session_ciphertext(16B)] -> ACK(err, MD5(session)(16B))
  UPDATE_OTA:[1][2][transformed_chunk(32B)] -> ACK(err, 累计sdbm_hash(4B LE))
  END_OTA:   [1][3][enable(1B)][expected_crc32(4B)] -> action_resp(err)

session_ciphertext = AES(session_key, ota_session)，
ota_session 由上位机生成的随机 16 字节，作为固件传输层的密钥种子。

数据流（对齐 C 库 _check_the_firmware + _update_ota_data）：
  1. 加密包 [product_no][file_size][AES-128-ECB 密文] 经 firmware key 解密得到**明文固件**
  2. 每个 32B 明文 chunk 做传输变换后发送：
     chunk[:16]:  XOR session_key[:16] -> AES-ECB-encrypt(ota_session)
     chunk[16:]:  XOR session_key[:16] -> AES-ECB-encrypt(ota_session)
  3. 设备端 identification_firmware 做逆变换还原**明文** chunk
  4. 主机端和设备端都对明文 chunk 累计 CRC32/sdbm_hash

因此传输的是明文（经传输变换加密），不是固件加密的密文。
固件加密（firmware_crypto.encrypt_firmware）仅用于 _check_the_firmware 校验格式，
校验通过后 C 库内部解密得到明文再传输。Python 重写时应直接传明文。
"""

from __future__ import annotations

import secrets
from typing import Callable, Optional

from comm_lib.crypto import aes_ecb_encrypt, md5_digest
from comm_lib.protocol.cmd import Command, ErrCode
from comm_lib.protocol.frame import u32_to_bytes, bytes_to_u32
from exceptions import DeviceToolError

OTA_TYPE_START = 1
OTA_TYPE_UPDATE = 2
OTA_TYPE_END = 3

PART_FIRMWARE_SIZE = 32  # 对齐 lib_device_manager.h::PART_FIRMWARE_SIZE
HALF_BLOCK = 16          # AES-128 块大小
_PAD_BYTE = 0xFF         # 明文/分片不足 32B 时的补齐字节（对齐 C 端 __read_part_firmware）


class OtaError(DeviceToolError):
    """OTA 流程错误。"""


def start_ota_session(device, session_key: bytes, timeout: float = 5.0) -> bytes:
    """发起 OTA，返回 ota_session（16B 随机密钥，供 transfer_firmware 使用）。

    生成随机 ota_session（16B），用 session_key 做 AES-ECB 加密后发给设备，
    设备解密后存为 ota_pc_session，返回 MD5(ota_session) 供上位机确认。
    """
    ota_session = secrets.token_bytes(16)
    ciphertext = aes_ecb_encrypt(session_key, ota_session)
    payload = bytes([0x01, OTA_TYPE_START]) + ciphertext
    err, data = device.send_and_wait(Command.WRITE_DATA, payload, timeout=timeout)
    if err != ErrCode.SUCCESS or len(data) < 16:
        raise OtaError(f"START_OTA 失败 err={err}")
    expected = md5_digest(ota_session)
    if data[:16] != expected:
        raise OtaError("START_OTA 应答 MD5 校验失败")
    return ota_session


def _transform_chunk(plaintext_chunk: bytes, session_key: bytes,
                     ota_session: bytes) -> bytes:
    """对 32B 明文 chunk 做传输变换。

    对齐 lib_device_manager.c::_update_ota_data：
      chunk[:16]:  XOR session_key[:16] -> AES-ECB-encrypt(ota_session)
      chunk[16:]:  XOR session_key[:16] -> AES-ECB-encrypt(ota_session)

    设备端 identification_firmware 做逆变换还原明文。
    """
    if len(plaintext_chunk) != PART_FIRMWARE_SIZE:
        raise OtaError(f"chunk 长度必须 {PART_FIRMWARE_SIZE}，实际 {len(plaintext_chunk)}")
    result = bytearray(PART_FIRMWARE_SIZE)
    for half in range(2):
        start = half * HALF_BLOCK
        src = plaintext_chunk[start:start + HALF_BLOCK]
        xored = bytes(src[i] ^ session_key[i] for i in range(HALF_BLOCK))
        enc = aes_ecb_encrypt(ota_session, xored)
        result[start:start + HALF_BLOCK] = enc
    return bytes(result)


def _iterate_plaintext_chunks(plaintext: bytes, chunk_size: int = PART_FIRMWARE_SIZE):
    """把明文固件按 chunk_size 切片迭代，最后一片不足时用 0xFF 补齐。

    对齐 C 端 __read_part_firmware：对明文固件按 32B 分片，最后一片不足补 0xFF。
    """
    total = len(plaintext)
    i = 0
    while i < total:
        chunk = plaintext[i:i + chunk_size]
        if len(chunk) < chunk_size:
            chunk = chunk + bytes([_PAD_BYTE] * (chunk_size - len(chunk)))
        yield chunk
        i += chunk_size


def transfer_firmware(
    device,
    plaintext_firmware: bytes,
    session_key: bytes,
    ota_session: bytes,
    on_progress: Optional[Callable[[int, int], None]] = None,
    on_check_cancel: Optional[Callable[[], bool]] = None,
    timeout: float = 5.0,
) -> int:
    """传输明文固件（32 字节分片 + 传输变换），返回累计 sdbm_hash。

    plaintext_firmware 是原始明文固件（设备端需要的就是明文）。
    按 32B 分片（不足补 0xFF），每片经 _transform_chunk 变换后发送。

    on_progress(current_index, total_index) 回调报告进度。
    on_check_cancel() 回调返回 True 时中止传输（抛出 OtaError("用户取消")）。
    """
    chunks = list(_iterate_plaintext_chunks(plaintext_firmware, PART_FIRMWARE_SIZE))
    total = len(chunks)
    for i, chunk in enumerate(chunks):
        if on_check_cancel is not None and on_check_cancel():
            raise OtaError("用户取消升级")
        transformed = _transform_chunk(chunk, session_key, ota_session)
        payload = bytes([0x01, OTA_TYPE_UPDATE]) + transformed
        err, data = device.send_and_wait(Command.WRITE_DATA, payload, timeout=timeout)
        if err != ErrCode.SUCCESS:
            raise OtaError(f"UPDATE_OTA 第 {i} 片失败 err={err}")
        if on_progress:
            on_progress(i + 1, total)
    # 最终累计 hash 在最后一个 ACK 的 data 里（4 字节 LE）
    return bytes_to_u32(data[:4]) if data else 0


def end_ota(device, expected_crc32: int, enable: int = 1, timeout: float = 5.0) -> int:
    """结束 OTA，校验累计 CRC32。

    enable=1 表示验证通过后让设备启用新固件。
    """
    payload = bytes([0x01, OTA_TYPE_END, enable & 0xFF]) + u32_to_bytes(expected_crc32)
    err, _ = device.send_and_wait(Command.WRITE_DATA, payload, timeout=timeout)
    return err


def compute_firmware_crc32(plaintext_firmware: bytes) -> int:
    """计算明文固件的累计 CRC32（用于 END_OTA 校验）。

    对齐设备端 identification_firmware 和主机端 _update_ota_data 的 CRC 累计方式：
    按 32 字节分片（不足补 0xFF），对每个明文 chunk 单独计算 crc32c 后累加。

    注意：传入的是原始明文固件（不是加密包）。
    """
    from comm_lib.protocol.frame import crc32c

    crc_sum = 0
    for chunk in _iterate_plaintext_chunks(plaintext_firmware, PART_FIRMWARE_SIZE):
        crc_sum = (crc_sum + crc32c(chunk)) & 0xFFFFFFFF
    return crc_sum
