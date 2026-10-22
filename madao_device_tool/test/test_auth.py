"""device_model.auth 鉴权握手单元测试。

用确定的 mock 数据验证 phase1->phase2->phase3 的算法链路，
对照 C 源码 _set_authenticate / ___authentication_action 的字节级逻辑。

运行：python madao_device_tool/test/test_auth.py
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from device_model.auth import (
    PHASE2,
    derive_session_key,
    build_phase2_response,
    verify_phase3,
    get_client_finger,
)


# 固定测试数据（确定性）
SESSION_KEY = bytes(range(16))  # 00..0f
DEVICE_FINGER = bytes(range(0x10, 0x20))  # 10..1f
DEVICE_RANDOM = bytes(range(0x20, 0x30))  # 20..2f
CLIENT_FINGER = bytes(range(0x30, 0x40))  # 30..3f
CLIENT_RANDOM = bytes(range(0x40, 0x50))  # 40..4f


def _simulate_device_phase1():
    """模拟设备端生成 phase1 challenge（vGetIdentificationRandom）。"""
    from comm_lib.crypto import aes_ecb_encrypt
    xor_val = bytes(a ^ b for a, b in zip(DEVICE_FINGER, DEVICE_RANDOM))
    return aes_ecb_encrypt(SESSION_KEY, xor_val)


def _simulate_device_verify_phase2(phase2_payload: bytes):
    """模拟设备端验证 phase2 并返回 client_random（vSetIdentificationClientRandom）。"""
    from comm_lib.crypto import aes_ecb_decrypt, md5_digest, sdbm_mem
    assert len(phase2_payload) == 33
    assert phase2_payload[0] == PHASE2
    salt1 = phase2_payload[1:17]
    encrypted_client = phase2_payload[17:33]

    # 设备验证 salt1
    hash_finger = sdbm_mem(DEVICE_FINGER)
    temp = hash_finger.to_bytes(4, "little") + DEVICE_RANDOM
    expected_salt1 = md5_digest(temp)
    assert salt1 == expected_salt1, "salt1 校验失败"

    # 设备解密 client_random
    decrypted = aes_ecb_decrypt(SESSION_KEY, encrypted_client)
    # 设备此时已知 CLIENT_FINGER（前面 set_client_finger 步骤存的）
    recovered = bytes(a ^ b for a, b in zip(decrypted, CLIENT_FINGER))
    return recovered


def _simulate_device_phase3(client_random):
    """模拟设备端生成 phase3（vGetIdentificationSalt）。"""
    from comm_lib.crypto import md5_digest, sdbm_mem
    hash_finger = sdbm_mem(CLIENT_FINGER)
    temp = hash_finger.to_bytes(4, "little") + client_random
    return md5_digest(temp)


def test_full_auth_flow_roundtrip():
    """完整鉴权流程：phase1 -> phase2 -> phase3 验证。"""
    # 上位机收到 phase1 challenge
    challenge1 = _simulate_device_phase1()

    # 上位机构造 phase2 应答
    phase2 = build_phase2_response(
        challenge1, SESSION_KEY, DEVICE_FINGER, CLIENT_FINGER, CLIENT_RANDOM
    )
    assert len(phase2) == 33
    assert phase2[0] == PHASE2

    # 设备端验证 phase2，恢复 client_random
    recovered_client_random = _simulate_device_verify_phase2(phase2)
    assert recovered_client_random == CLIENT_RANDOM

    # 设备发 phase3，上位机验证
    phase3_data = _simulate_device_phase3(recovered_client_random)
    assert verify_phase3(phase3_data, CLIENT_FINGER, CLIENT_RANDOM)


def test_phase2_wrong_fingerprint_fails():
    """device_finger 错误时，phase2 的 salt1 不会被设备接受。"""
    challenge1 = _simulate_device_phase1()
    wrong_finger = bytes(0xFF if b != 0x10 else b for b in DEVICE_FINGER)
    phase2 = build_phase2_response(
        challenge1, SESSION_KEY, wrong_finger, CLIENT_FINGER, CLIENT_RANDOM
    )
    # 设备端验证 salt1 应失败
    from comm_lib.crypto import md5_digest, sdbm_mem
    hash_finger = sdbm_mem(DEVICE_FINGER)
    temp = hash_finger.to_bytes(4, "little") + DEVICE_RANDOM
    expected_salt1 = md5_digest(temp)
    assert phase2[1:17] != expected_salt1


def test_phase3_verify_wrong_data():
    assert not verify_phase3(b"\x00" * 16, CLIENT_FINGER, CLIENT_RANDOM)
    assert not verify_phase3(b"\x00" * 15, CLIENT_FINGER, CLIENT_RANDOM)  # 长度错


def test_session_key_derivation_structure():
    mac = bytes([1, 2, 3, 4, 5, 6])
    version = bytes([7, 8, 9, 10])
    key = derive_session_key(mac, version)
    assert len(key) == 16
    assert key[:6] == mac
    assert key[6:7] == b"{"
    assert key[7:11] == version
    assert key[11:12] == b"}"


def test_get_client_finger_returns_16_bytes():
    finger = get_client_finger()
    assert len(finger) == 16
    # 同一台机器两次调用应一致
    assert get_client_finger() == finger


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
