"""AES-128-ECB 加解密。

对齐 src_archive/windows/libProtocolFramework/mblue_aes.c（标准 AES-128，
ECB 模式，10 轮，128-bit key）。设备端驱动库 aes.h 也是同一标准实现。
pycryptodome 的 AES 通过 FIPS-197 标准向量校验，与 C 实现完全等价。

注意：ECB 模式不安全（无 IV），仅用于设备私有协议鉴权/OTA，不用于通用加密。
"""

from __future__ import annotations

from Crypto.Cipher import AES

_AES_BLOCK = 16  # AES-128 块大小


def _check_key(key: bytes) -> None:
    if len(key) != _AES_BLOCK:
        raise ValueError(f"AES-128 key 必须 16 字节，收到 {len(key)}")


def aes_ecb_encrypt(key: bytes, data: bytes) -> bytes:
    """AES-128-ECB 加密。

    对齐 C: mblue_aes._ecb_encrypt -> Cipher(buf, round_key)。
    data 长度必须是 16 的倍数（ECB 无填充，调用方负责补齐）。
    """
    _check_key(key)
    if len(data) % _AES_BLOCK != 0:
        raise ValueError(f"明文长度必须是 16 的倍数，收到 {len(data)}")
    return AES.new(key, AES.MODE_ECB).encrypt(data)


def aes_ecb_decrypt(key: bytes, data: bytes) -> bytes:
    """AES-128-ECB 解密。

    对齐 C: mblue_aes._ecb_decrypt -> InvCipher(buf, round_key)。
    """
    _check_key(key)
    if len(data) % _AES_BLOCK != 0:
        raise ValueError(f"密文长度必须是 16 的倍数，收到 {len(data)}")
    return AES.new(key, AES.MODE_ECB).decrypt(data)
