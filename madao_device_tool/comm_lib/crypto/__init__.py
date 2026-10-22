"""comm_lib.crypto 子包：鉴权与 OTA 所需的加密算法。

对齐 src_archive/windows/libProtocolFramework 的 mblue_aes.c / mblue_md5.c / mblue_hash.c。
用成熟库（pycryptodome / hashlib）等价实现——AES/MD5 是标准算法，
C 源码也是标准实现（tiny-aes / RFC1321），无需逐字节翻译。
仅 sdbm hash 自写（非标准算法）。
"""

from .aes import aes_ecb_encrypt, aes_ecb_decrypt
from .md5 import md5_digest
from .hash import sdbm_mem

__all__ = ["aes_ecb_encrypt", "aes_ecb_decrypt", "md5_digest", "sdbm_mem"]
