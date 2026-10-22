"""固件 OTA 加密工具。

对齐 src_archive/windows/libProtocolFramework/lib_device_manager.c 的固件加密格式：
  加密格式: product_NO(4) + file_size(4, 小端) + AES-128-ECB 密文(16 字节倍数)
  file_size = 密文长度（即 padded plaintext 长度），不是原始明文长度。
  AES key 由结构化布局派生（见 derive_firmware_key）。

设备端校验（lib_device_manager.c::_check_the_firmware）会用 AES-128-ECB 解密校验固件，
且要求 file_size == 总包长 - 8（即密文长度）。
"""

from __future__ import annotations

from comm_lib.crypto import aes_ecb_decrypt, aes_ecb_encrypt, sdbm_mem

_HEADER_SIZE = 8  # product_NO(4) + file_size(4)
_PAD_BYTE = 0xFF  # 明文不足 16 字节倍数时的填充字节（对齐 C 端 __read_part_firmware 的 0xff 补齐）


def derive_firmware_key(product_no: bytes, file_size: int) -> bytes:
    """从 product_NO + file_size 派生 16 字节 AES key。

    对齐 lib_device_manager.c::get_firmware_key 的结构化布局：
      key[0]     = 0x7B ('{')
      key[1:5]   = file_size (4 字节小端)
      key[5]     = 0x7B ('{')
      key[6:10]  = product_no (4 字节)
      key[10]    = 0x7D ('}')
      key[11:15] = sdbm(key[1:11]) (4 字节小端)
      key[15]    = 0x7D ('}')
    """
    if len(product_no) != 4:
        raise ValueError("product_NO 必须 4 字节")
    key = bytearray(16)
    key[0] = 0x7B
    key[1:5] = int(file_size).to_bytes(4, "little")
    key[5] = 0x7B
    key[6:10] = bytes(product_no)[:4]
    key[10] = 0x7D
    key[11:15] = sdbm_mem(bytes(key[1:11])).to_bytes(4, "little")
    key[15] = 0x7D
    return bytes(key)


def encrypt_firmware(plaintext_firmware: bytes, product_no: bytes) -> bytes:
    """把明文固件加密成 OTA 传输格式。

    返回: product_NO(4) + file_size(4 LE) + AES-128-ECB 密文（16 字节倍数）。

    file_size = 密文长度（= padded plaintext 长度），对齐 C 端 _check_the_firmware
    的校验：file_size 必须等于总包长 - 8。
    """
    if len(product_no) != 4:
        raise ValueError("product_NO 必须 4 字节")
    # AES-128-ECB 要求明文 16 字节倍数，用 0xFF 填充（对齐 C 端 0xff 补齐）
    pad_len = (-len(plaintext_firmware)) % 16
    padded = bytes(plaintext_firmware) + bytes([_PAD_BYTE] * pad_len)
    file_size = len(padded)  # 密文长度（= padded 明文长度）
    key = derive_firmware_key(product_no, file_size)
    ciphertext = aes_ecb_encrypt(key, padded)
    return bytes(product_no) + file_size.to_bytes(4, "little") + ciphertext


def decrypt_firmware(encrypted_package: bytes) -> tuple[bytes, int, bytes]:
    """解密 OTA 包，返回 (product_NO, file_size, padded 明文固件)。

    file_size 是密文长度（不是原始明文长度），返回的明文包含 0xFF 填充。
    调用方如需原始明文长度需自行记录。
    """
    if len(encrypted_package) < _HEADER_SIZE:
        raise ValueError("OTA 包过短")
    product_no = encrypted_package[:4]
    file_size = int.from_bytes(encrypted_package[4:8], "little")
    ciphertext = encrypted_package[8:]
    if len(ciphertext) % 16 != 0:
        raise ValueError("密文长度不是 16 的倍数")
    if file_size != len(ciphertext):
        raise ValueError(f"file_size({file_size}) != 密文长度({len(ciphertext)})")
    key = derive_firmware_key(product_no, file_size)
    plaintext = aes_ecb_decrypt(key, ciphertext)
    return product_no, file_size, plaintext


def iterate_chunks(encrypted_package: bytes, chunk_size: int = 32):
    """把加密固件包（去掉 8 字节头后的密文部分）按 chunk_size 切片迭代。

    对齐 mspeech_usb_manager.c::_get_byte_stream_data 的 32 字节分片逻辑：
    不足 32 字节用 0xFF 补齐。
    """
    payload = encrypted_package[_HEADER_SIZE:]
    total = len(payload)
    i = 0
    while i < total:
        chunk = payload[i:i + chunk_size]
        if len(chunk) < chunk_size:
            chunk = chunk + b"\xff" * (chunk_size - len(chunk))
        yield chunk
        i += chunk_size
