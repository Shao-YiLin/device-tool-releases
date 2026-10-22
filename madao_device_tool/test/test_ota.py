"""OTA 升级单元测试。

用 Mock 设备验证：
- firmware_crypto 加密/解密 roundtrip（仅用于 _check_the_firmware 校验格式）
- firmware key 派生（对齐 C 源码 get_firmware_key 结构化布局）
- file_size 字段 = 密文长度（对齐 C 端 _check_the_firmware 校验）
- ota.py 流程编排（START/UPDATE/END 三阶段）
- 明文分片逻辑（32B + 0xFF 补齐）
- per-chunk 传输变换（XOR session_key + AES ota_session）及设备端逆变换
- CRC 按 32B chunk 分片累计（对齐设备端 identification_firmware）

运行：python madao_device_tool/test/test_ota.py
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from comm_lib.firmware_crypto import (
    _HEADER_SIZE,
    decrypt_firmware,
    derive_firmware_key,
    encrypt_firmware,
    iterate_chunks,
)
from comm_lib.crypto import md5_digest, sdbm_mem, aes_ecb_decrypt, aes_ecb_encrypt
from device_model.ota import (
    OTA_TYPE_END,
    OTA_TYPE_START,
    OTA_TYPE_UPDATE,
    PART_FIRMWARE_SIZE,
    OtaError,
    _iterate_plaintext_chunks,
    _transform_chunk,
    end_ota,
    start_ota_session,
    transfer_firmware,
)


# ---- firmware_crypto: key 派生 ----
def test_firmware_key_matches_c_layout():
    """验证 key 派生对齐 C 源码 get_firmware_key 的结构化布局。"""
    product_no = b"1044"
    file_size = 1024
    key = derive_firmware_key(product_no, file_size)
    assert len(key) == 16
    # 结构化布局校验
    assert key[0] == 0x7B  # '{'
    assert key[1:5] == file_size.to_bytes(4, "little")
    assert key[5] == 0x7B  # '{'
    assert key[6:10] == product_no
    assert key[10] == 0x7D  # '}'
    expected_hash = sdbm_mem(key[1:11]).to_bytes(4, "little")
    assert key[11:15] == expected_hash
    assert key[15] == 0x7D  # '}'


def test_firmware_key_deterministic():
    k1 = derive_firmware_key(b"1044", 1024)
    k2 = derive_firmware_key(b"1044", 1024)
    assert k1 == k2
    assert len(k1) == 16
    # 不同 product_no 应得不同 key
    assert derive_firmware_key(b"1044", 1024) != derive_firmware_key(b"1045", 1024)
    # 不同 file_size 应得不同 key
    assert derive_firmware_key(b"1044", 1024) != derive_firmware_key(b"1044", 1025)


# ---- firmware_crypto: 加密/解密（仅用于 _check_the_firmware 校验格式）----
def test_firmware_encrypt_decrypt_roundtrip():
    product_no = b"1044"
    firmware = bytes(range(256)) * 4  # 1024 字节（16 对齐）
    encrypted = encrypt_firmware(firmware, product_no)
    assert encrypted[:4] == product_no
    # file_size = 密文长度 = padded 明文长度
    file_size = int.from_bytes(encrypted[4:8], "little")
    assert file_size == len(encrypted) - _HEADER_SIZE
    assert file_size == len(firmware)  # 1024 已对齐
    pn, size, plain = decrypt_firmware(encrypted)
    assert pn == product_no
    assert size == file_size
    assert plain == firmware


def test_firmware_file_size_is_ciphertext_length_unaligned():
    """明文非 16 对齐时，file_size 应为 padded 后的密文长度。"""
    product_no = b"1044"
    firmware = bytes(range(70))  # 70 字节，pad 到 80
    encrypted = encrypt_firmware(firmware, product_no)
    file_size = int.from_bytes(encrypted[4:8], "little")
    ciphertext_len = len(encrypted) - _HEADER_SIZE
    assert file_size == ciphertext_len  # 必须等于密文长度
    assert file_size == 80  # 70 pad 到 80
    # C 端校验：file_size == size - 8
    assert file_size == len(encrypted) - 8


def test_firmware_padding():
    """非对齐明文应 0xFF 填充到 16 倍数。"""
    product_no = b"0001"
    firmware = bytes(range(10))  # 10 字节
    encrypted = encrypt_firmware(firmware, product_no)
    pn, size, plain = decrypt_firmware(encrypted)
    assert len(plain) == 16  # padded 到 16
    assert plain[:10] == firmware
    assert plain[10:] == b"\xff" * 6  # 0xFF 填充


def test_iterate_chunks_32byte():
    # firmware 70 字节 -> 加密时补到 80(16倍数) -> 密文 80 字节
    # iterate_chunks 切 80 字节：32+32+16(补0xFF到32) = 3 片
    product_no = b"0001"
    firmware = bytes(range(70))
    encrypted = encrypt_firmware(firmware, product_no)
    chunks = list(iterate_chunks(encrypted, 32))
    assert len(chunks) == 3
    assert all(len(c) == 32 for c in chunks)
    # 最后一片 16 字节真实 + 16 字节 0xFF 补齐
    assert chunks[-1][16:] == b"\xff" * 16


# ---- OTA: 明文分片 ----
def test_iterate_plaintext_chunks():
    """明文按 32B 分片，最后一片不足补 0xFF。"""
    # 32 倍数：无需补齐
    chunks = list(_iterate_plaintext_chunks(bytes(range(64))))
    assert len(chunks) == 2
    assert all(len(c) == 32 for c in chunks)
    # 非 32 倍数：最后一片补齐
    chunks = list(_iterate_plaintext_chunks(bytes(range(50))))
    assert len(chunks) == 2
    assert chunks[0] == bytes(range(32))
    assert chunks[1][:18] == bytes(range(32, 50))
    assert chunks[1][18:] == b"\xff" * 14
    # 空输入
    assert list(_iterate_plaintext_chunks(b"")) == []


# ---- OTA: per-chunk 变换 ----
def test_transform_chunk_roundtrip():
    """验证 _transform_chunk 和设备端逆变换互逆。"""
    session_key = bytes(range(16))
    ota_session = bytes(range(16, 32))
    plaintext_chunk = bytes(range(32))
    transformed = _transform_chunk(plaintext_chunk, session_key, ota_session)
    assert len(transformed) == 32
    # 设备端逆变换：AES-decrypt(ota_session) XOR session_key
    recovered = bytearray(32)
    for half in range(2):
        start = half * 16
        dec = aes_ecb_decrypt(ota_session, transformed[start:start + 16])
        for i in range(16):
            recovered[start + i] = dec[i] ^ session_key[i]
    assert bytes(recovered) == plaintext_chunk


# ---- OTA 流程（Mock 设备）----
class MockDevice:
    """模拟设备，完整实现设备端 OTA 解密验证逻辑。

    对齐 AM50/vendor/8258_dual_mouse/mblue_identification_service.c::identification_firmware：
    - START_OTA: AES-decrypt(session_key, ciphertext) -> ota_pc_session，返回 MD5
    - UPDATE_OTA: 每个 32B chunk 做 AES-decrypt(ota_pc_session) XOR session_key 还原**明文**，
                  累计 sdbm_hash 和 crc32
    - END_OTA: 校验累计 crc32 == expected_crc32

    注意：设备端还原的 out_data 是**明文** chunk（主机端 _update_ota_data 传入的 data）。
    主机端 CRC 在 _update_ota_data 中对 data（明文）累计，与设备端一致。
    """
    def __init__(self, session_key):
        self._session_key = session_key
        self._ota_pc_session = None
        self._plaintext_received = bytearray()  # 还原的明文固件
        self._hash_sum = 0
        self._crc_sum = 0
        self._sent_frames = []

    def send_and_wait(self, cmd, payload, timeout=5.0):
        from comm_lib.protocol.cmd import Command, ErrCode
        from comm_lib.protocol.frame import crc32c
        self._sent_frames.append((cmd, payload))
        assert cmd == Command.WRITE_DATA
        assert payload[0] == 0x01
        ota_type = payload[1]
        ota_data = payload[2:]
        if ota_type == OTA_TYPE_START:
            self._ota_pc_session = aes_ecb_decrypt(self._session_key, ota_data)
            return ErrCode.SUCCESS, md5_digest(self._ota_pc_session)
        elif ota_type == OTA_TYPE_UPDATE:
            # 设备端 identification_firmware 逆变换：AES-decrypt(ota_session) XOR session_key -> 明文
            assert len(ota_data) == PART_FIRMWARE_SIZE
            recovered = bytearray(PART_FIRMWARE_SIZE)
            for half in range(2):
                start = half * 16
                dec = aes_ecb_decrypt(self._ota_pc_session, ota_data[start:start + 16])
                for i in range(16):
                    recovered[start + i] = dec[i] ^ self._session_key[i]
            self._plaintext_received.extend(recovered)
            # 设备端对还原后的明文 chunk 累计 CRC/sdbm
            self._hash_sum = (self._hash_sum + sdbm_mem(bytes(recovered))) & 0xFFFFFFFF
            self._crc_sum = (self._crc_sum + crc32c(bytes(recovered))) & 0xFFFFFFFF
            return ErrCode.SUCCESS, self._hash_sum.to_bytes(4, "little")
        elif ota_type == OTA_TYPE_END:
            enable = ota_data[0]
            expected_crc = int.from_bytes(ota_data[1:5], "little")
            if enable and self._crc_sum != expected_crc:
                return ErrCode.FAIL, b""
            return ErrCode.SUCCESS, b""
        return ErrCode.FAIL, b""


def test_full_ota_flow_with_mock():
    """完整 OTA 流程：明文 -> start -> transfer -> end，用 mock 设备验证。"""
    from device_model.ota import compute_firmware_crc32

    session_key = bytes(range(16))
    plaintext = bytes(range(32)) * 4  # 128 字节

    device = MockDevice(session_key)

    # start
    ota_session = start_ota_session(device, session_key)
    assert len(ota_session) == 16

    # transfer（传入明文 + session_key + ota_session）
    progress = []
    sdbm = transfer_firmware(device, plaintext, session_key, ota_session,
                             on_progress=lambda c, t: progress.append((c, t)))
    assert len(progress) > 0
    assert progress[-1][0] == progress[-1][1]

    # 设备端还原的明文应与原始明文一致
    assert bytes(device._plaintext_received[:len(plaintext)]) == plaintext

    # end（CRC 基于明文按 chunk 累计）
    expected_crc = compute_firmware_crc32(plaintext)
    err = end_ota(device, expected_crc)
    from comm_lib.protocol.cmd import ErrCode
    assert err == ErrCode.SUCCESS


def test_full_ota_flow_unaligned():
    """非 32 对齐明文的完整 OTA 流程。"""
    from device_model.ota import compute_firmware_crc32

    session_key = bytes(range(16))
    plaintext = bytes(range(50))  # 50 字节，分片 32+18(补0xFF到32)

    device = MockDevice(session_key)
    ota_session = start_ota_session(device, session_key)

    transfer_firmware(device, plaintext, session_key, ota_session)

    # 设备端还原的前 50 字节应与明文一致（后面是 0xFF 填充）
    assert bytes(device._plaintext_received[:50]) == plaintext

    # CRC 基于明文按 chunk 累计（含最后一片的 0xFF 补齐）
    expected_crc = compute_firmware_crc32(plaintext)
    err = end_ota(device, expected_crc)
    from comm_lib.protocol.cmd import ErrCode
    assert err == ErrCode.SUCCESS


def test_ota_progress_callback():
    plaintext = bytes(range(64))  # 2 片
    session_key = bytes(range(16))
    device = MockDevice(session_key)
    ota_session = start_ota_session(device, session_key)

    progress = []
    transfer_firmware(device, plaintext, session_key, ota_session,
                      on_progress=lambda c, t: progress.append((c, t)))
    assert all(progress[i][0] <= progress[i + 1][0] for i in range(len(progress) - 1))


def test_ota_crc_mismatch_fails():
    """CRC 不匹配时 END_OTA 应失败。"""
    plaintext = bytes(range(32))
    session_key = bytes(range(16))
    device = MockDevice(session_key)
    ota_session = start_ota_session(device, session_key)
    transfer_firmware(device, plaintext, session_key, ota_session)
    err = end_ota(device, 0xDEAD)  # 故意错误的 CRC
    from comm_lib.protocol.cmd import ErrCode
    assert err == ErrCode.FAIL


def test_transfer_cancel():
    """on_check_cancel 返回 True 时 transfer_firmware 应抛出 OtaError。"""
    plaintext = bytes(range(64))  # 2 片
    session_key = bytes(range(16))
    device = MockDevice(session_key)
    ota_session = start_ota_session(device, session_key)
    # 第 2 片时取消
    call_count = [0]
    def check_cancel():
        call_count[0] += 1
        return call_count[0] >= 2
    try:
        transfer_firmware(device, plaintext, session_key, ota_session,
                          on_check_cancel=check_cancel)
        assert False, "应抛出 OtaError"
    except OtaError as e:
        assert "取消" in str(e)
    # 第 1 片应已发送成功
    assert len(device._plaintext_received) == 32


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
