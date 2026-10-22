"""鉴权握手协议。

完整实现设备私有协议的 4 阶段鉴权握手，对齐：
  - src_archive/windows/libProtocolFramework/lib_device_manager.c::_start_authentication
  - src_archive/windows/libProtocolFramework/protocol_logic_manager.c::_set_authenticate
  - src_archive/T8_light/.../mblue_identification_service.c（设备端配合逻辑）

时序：
  准备: 上位机获取 device_mac/device_version/device_finger，计算 client_finger、client_random、session_key
  step1 set_client_finger: SET_SLAVE_PROP [CLIENT_FINGER][client_finger(16)]
  step2 get_device_finger: GET_SLAVE_PROP [FINGER] -> device_finger(16)
  step3 get_device_mac:    GET_SLAVE_PROP [MAC_MOURSE] -> device_mac(6)
  step4 get_device_version:GET_SLAVE_PROP [VERSION_MOURSE] -> device_version(4)
  step5 handshake:         HANDSHAKE -> 设备触发鉴权流程，主动发 AUTHENTICATE(phase1)
  step6 处理 phase1: 解密 device challenge -> 构造 phase2 应答（AUTHENTICATE 帧）
  step7 设备验证 phase2 后发 phase3 -> 上位机验证 phase3 -> 鉴权成功/失败

注：设备端可能因 flash 中鉴权标志已置位而跳过流程，handshake 的 ACK err_code 会带 0x10000 标志。
"""

from __future__ import annotations

import platform
import subprocess

from comm_lib.crypto import aes_ecb_decrypt, aes_ecb_encrypt, md5_digest, sdbm_mem

# Windows 下 GUI 程序调子进程时不弹黑窗；其他平台无此常量则置 0
_NO_WINDOW = getattr(subprocess, "CREATE_NO_WINDOW", 0)

PHASE1 = 0x1
PHASE2 = 0x2
PHASE3 = 0x3
PHASE4 = 0x4

AUTH_AUTHENTICATED_FLAG = 0x10000  # handshake ACK 带此标志表示设备已鉴权（跳过流程）


def get_client_finger() -> bytes:
    """获取客户端设备指纹（16 字节 MD5）。

    对齐 C: lib_device_manager.c::_get_client_finger -> MD5(主板序列号)。
    Windows: wmic/CIM BaseBoard SerialNumber；Linux: /etc/machine-id；Mac: ioreg IOPlatformUUID。
    返回 16 字节 MD5 摘要。
    """
    raw = _get_board_serial()
    return md5_digest(raw)


def _get_board_serial() -> bytes:
    """获取主板/机器唯一标识（128 字节缓冲，对齐 C board_uuid[128]）。"""
    system = platform.system()
    try:
        if system == "Windows":
            # 对齐 C: wmic BaseBoard get SerialNumber
            out = subprocess.check_output(
                ["wmic", "BaseBoard", "get", "SerialNumber"],
                stderr=subprocess.DEVNULL, timeout=5,
                creationflags=_NO_WINDOW,
            ).decode("utf-8", errors="ignore")
            # 提取 SerialNumber 后的内容，去除空白
            parts = out.split("SerialNumber", 1)
            serial = parts[1] if len(parts) == 2 else out
            serial = serial.strip()
        elif system == "Linux":
            try:
                serial = open("/etc/machine-id").read().strip()
            except Exception:
                serial = ""
        elif system == "Darwin":
            out = subprocess.check_output(
                ["ioreg", "-rd1", "-c", "IOPlatformExpertDevice"],
                stderr=subprocess.DEVNULL, timeout=5,
                creationflags=_NO_WINDOW,
            ).decode("utf-8", errors="ignore")
            for line in out.splitlines():
                if "IOPlatformUUID" in line:
                    serial = line.split('"')[-2]
                    break
            else:
                serial = ""
        else:
            serial = ""
    except Exception:
        serial = ""

    # C 用 board_uuid[128] 缓冲，不足补 0。Python 这里也补齐到 128 字节保持一致。
    raw = (serial or "").encode("utf-8")
    return raw.ljust(128, b"\x00")[:128]


def derive_session_key(device_mac: bytes, device_version: bytes) -> bytes:
    """派生 session_key（16 字节）。

    对齐 C: lib_device_manager.c::_get_common_sessionkey:
      key = mac[6] + '{' + version[4] + '}' + sdbm_hash(前12字节)[4 LE]
    """
    if len(device_mac) != 6 or len(device_version) != 4:
        raise ValueError("mac 必须 6 字节，version 必须 4 字节")
    prefix = bytes(device_mac) + b"{" + bytes(device_version) + b"}"
    h = sdbm_mem(prefix)
    return prefix + h.to_bytes(4, "little")


def build_phase2_response(challenge1_payload: bytes, session_key: bytes,
                          device_finger: bytes, client_finger: bytes,
                          client_random: bytes) -> bytes:
    """处理设备 phase1 challenge，构造 phase2 应答（33 字节）。

    对齐 C: protocol_logic_manager.c::_set_authenticate。
    challenge1_payload = phase1(1B) + AES(session_key, device_finger XOR device_random)(16B)（共17B，phase字节在外层解析后传入16B密文）
    返回: phase2(1B) + salt1(16B) + AES(session_key, client_finger XOR client_random)(16B)
    """
    if len(challenge1_payload) != 16:
        raise ValueError(f"phase1 challenge 密文必须 16 字节，收到 {len(challenge1_payload)}")
    if len(device_finger) != 16 or len(client_finger) != 16 or len(client_random) != 16:
        raise ValueError("finger/random 都必须 16 字节")

    # 1. 解密 challenge1 -> XOR device_finger -> device_random
    decrypted = aes_ecb_decrypt(session_key, challenge1_payload)
    device_random = bytes(a ^ b for a, b in zip(decrypted, device_finger))

    # 2. salt1 = MD5(sdbm(device_finger)[4 LE] + device_random)
    hash_finger = sdbm_mem(device_finger)
    temp = hash_finger.to_bytes(4, "little") + device_random
    salt1 = md5_digest(temp)

    # 3. AES(session_key, client_finger XOR client_random)
    xor_val = bytes(a ^ b for a, b in zip(client_finger, client_random))
    encrypted = aes_ecb_encrypt(session_key, xor_val)

    return bytes([PHASE2]) + salt1 + encrypted


def verify_phase3(phase3_data: bytes, client_finger: bytes,
                  client_random: bytes) -> bool:
    """验证设备 phase3 应答。

    对齐 C: lib_device_manager.c::___authentication_action(phase==PHASE3):
      期望 salt3 = MD5(sdbm(client_finger)[4 LE] + client_random)
    phase3_data: 设备发来的 16 字节 salt
    """
    if len(phase3_data) != 16:
        return False
    hash_finger = sdbm_mem(client_finger)
    temp = hash_finger.to_bytes(4, "little") + client_random
    expected = md5_digest(temp)
    return phase3_data == expected
