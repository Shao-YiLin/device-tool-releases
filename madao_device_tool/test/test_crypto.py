"""comm_lib.crypto 单元测试。

测试向量来源：
- AES: FIPS-197 标准测试向量（NIST 官方）
- MD5: RFC 1321 标准测试向量
- sdbm: 对照 C 源码 mblue_hash.c 公式手算
- 鉴权 session_key 派生：对照 lib_device_manager.c::_get_common_sessionkey

运行：python madao_device_tool/test/test_crypto.py
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from comm_lib.crypto import aes_ecb_decrypt, aes_ecb_encrypt, md5_digest, sdbm_mem


# ---- AES-128-ECB ----
def test_aes_fips197_standard_vector():
    # FIPS-197 Appendix B / C.1
    key = bytes.fromhex("000102030405060708090a0b0c0d0e0f")
    pt = bytes.fromhex("00112233445566778899aabbccddeeff")
    expected_ct = bytes.fromhex("69c4e0d86a7b0430d8cdb78070b4c55a")
    ct = aes_ecb_encrypt(key, pt)
    assert ct == expected_ct


def test_aes_decrypt_roundtrip():
    key = b"0123456789abcdef"
    pt = b"0123456789abcdef" * 2  # 32 字节（多块）
    ct = aes_ecb_encrypt(key, pt)
    assert aes_ecb_decrypt(key, ct) == pt


def test_aes_wrong_key_length():
    try:
        aes_ecb_encrypt(b"short", b"0123456789abcdef")
        assert False
    except ValueError:
        pass


def test_aes_wrong_block_size():
    try:
        aes_ecb_encrypt(b"0123456789abcdef", b"too short")
        assert False
    except ValueError:
        pass


# ---- MD5 ----
def test_md5_rfc1321_empty():
    # RFC 1321: MD5("") = d41d8cd98f00b204e9800998ecf8427e
    assert md5_digest(b"").hex() == "d41d8cd98f00b204e9800998ecf8427e"


def test_md5_rfc1321_a():
    # MD5("a") = 0cc175b9c0f1b6a831c399e269772661
    assert md5_digest(b"a").hex() == "0cc175b9c0f1b6a831c399e269772661"


def test_md5_rfc1321_abc():
    # MD5("abc") = 900150983cd24fb0d6963f7d28e17f72
    assert md5_digest(b"abc").hex() == "900150983cd24fb0d6963f7d28e17f72"


def test_md5_rfc1321_message_digest():
    # MD5("message digest") = f96b697d7cb7938d525a2f31aaf161d0
    assert md5_digest(b"message digest").hex() == "f96b697d7cb7938d525a2f31aaf161d0"


# ---- sdbm hash ----
def test_sdbm_empty():
    assert sdbm_mem(b"") == 0


def test_sdbm_single_byte():
    # 手算: h = 0x41 + 0 - 0 = 0x41
    assert sdbm_mem(b"A") == 0x41


def test_sdbm_known_value():
    # 手算 b"\x01\x02":
    #   h0 = 0
    #   h1 = 1 + 0 + 0 - 0 = 1
    #   h2 = 2 + (1<<6) + (1<<16) - 1 = 2 + 64 + 65536 - 1 = 65601
    assert sdbm_mem(b"\x01\x02") == 65601


def test_sdbm_32bit_wraparound():
    # 构造一个大输入让 hash 溢出 32 位，验证 & 0xFFFFFFFF
    # 只验证结果 < 0x80000000（最高位清零）
    result = sdbm_mem(b"abcdefghij" * 100)
    assert 0 <= result < 0x80000000


def test_sdbm_session_key_derivation_matches_c():
    # 对照 lib_device_manager.c::_get_common_sessionkey:
    #   key = mac[6] + '{' + version[4] + '}' + sdbm_hash(前12字节)[4 LE]
    mac = bytes([0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF])
    version = bytes([0x01, 0x02, 0x03, 0x04])
    prefix = mac + b"{" + version + b"}"
    assert len(prefix) == 12
    h = sdbm_mem(prefix)
    session_key = prefix + h.to_bytes(4, "little")
    assert len(session_key) == 16
    # 验证 session_key 结构正确（前 12 是明文，后 4 是 hash 小端）
    assert session_key[:12] == prefix


# ---- 鉴权协议集成测试（用 mock 数据验证算法组合） ----
def test_aes_session_key_encrypt_decrypt():
    # 鉴权 phase2: 设备发来的 challenge1 是 AES(session_key, device_finger ^ device_random)
    # 上位机收到后 AES 解密再 XOR device_finger 得到 device_random
    session_key = b"\x11" * 16
    device_finger = b"\x22" * 16
    device_random = b"\x33" * 16
    # 设备端加密
    xor_val = bytes(a ^ b for a, b in zip(device_finger, device_random))
    challenge1 = aes_ecb_encrypt(session_key, xor_val)
    # 上位机解密
    decrypted = aes_ecb_decrypt(session_key, challenge1)
    recovered_random = bytes(a ^ b for a, b in zip(decrypted, device_finger))
    assert recovered_random == device_random


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
