"""sdbm hash（非标准哈希，用于鉴权 session_key 派生）。

对齐 src_archive/windows/libProtocolFramework/mblue_hash.c::_sdbm_mem：
    hash = (*str++) + (hash << 6) + (hash << 16) - hash;
    return hash & 0x7FFFFFFF;

注意 C 的 unsigned int 是 32 位无符号回绕，Python int 无限精度，
必须显式 & 0xFFFFFFFF 模拟 32 位溢出。
"""

from __future__ import annotations


def sdbm_mem(data: bytes) -> int:
    """sdbm hash，返回 32 位无符号整数（最高位清零，与 C 一致）。

    对齐 C: mblue_hash._sdbm_mem(data, len)。
    """
    h = 0
    for b in data:
        h = (b + (h << 6) + (h << 16) - h) & 0xFFFFFFFF
    return h & 0x7FFFFFFF
