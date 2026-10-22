"""MD5 摘要。

对齐 src_archive/windows/libProtocolFramework/mblue_md5.c（标准 RFC 1321 MD5）。
Python hashlib.md5 与之完全等价（同样的初始化常量、padding、变换）。
"""

from __future__ import annotations

import hashlib


def md5_digest(data: bytes) -> bytes:
    """计算 MD5 摘要，返回 16 字节。

    对齐 C: mblue_md5._init + _update + _final。
    """
    return hashlib.md5(data).digest()
