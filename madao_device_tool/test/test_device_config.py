"""device_config 单元测试。

验证 infer_device_type 的前缀判定、get_device_config 对已知 product_no
返回正确配置、未知 product_no 返回 None。

运行：python madao_device_tool/test/test_device_config.py
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from device_config import get_device_config, infer_device_type


# ---- infer_device_type ----
def test_infer_mouse_prefix():
    for pn in ["1000", "1018", "1123", "1999"]:
        assert infer_device_type(pn) == "mouse", pn


def test_infer_keyboard_prefix():
    for pn in ["3000", "3007", "3014", "3999"]:
        assert infer_device_type(pn) == "keyboard", pn


def test_infer_other_prefix():
    # 4xxx / 9xxx -> other
    for pn in ["4000", "4001", "9007"]:
        assert infer_device_type(pn) == "other", pn


def test_infer_empty_or_invalid():
    assert infer_device_type("") == "other"
    assert infer_device_type("M110") == "other"  # 非数字首字符


# ---- get_device_config ----
def test_get_known_feishu_s():
    cfg = get_device_config("1018")
    assert cfg is not None
    assert cfg["name"] == "FeiShu-S"
    assert cfg["type"] == "mouse"
    assert "cpi" in cfg["capabilities"]
    assert "polling_rate" in cfg["capabilities"]


def test_get_known_at80():
    cfg = get_device_config("3007")
    assert cfg is not None
    assert cfg["name"] == "AT80"
    assert cfg["type"] == "keyboard"
    assert "rgb" in cfg["capabilities"]
    assert "custom_key" in cfg["capabilities"]


def test_get_known_t8():
    cfg = get_device_config("3000")
    assert cfg is not None
    assert cfg["name"] == "T8"
    assert cfg["type"] == "keyboard"


def test_get_known_other():
    cfg = get_device_config("9007")
    assert cfg is not None
    assert cfg["name"] == "X1"
    assert cfg["type"] == "other"
    assert cfg["capabilities"] == []


def test_get_unknown_returns_none():
    assert get_device_config("9999") is None
    assert get_device_config("") is None
    assert get_device_config("1234") is None  # 不在配置表


def test_config_structure_consistency():
    """所有配置条目都含 name/type/capabilities 三字段。"""
    import device_config as dc
    for pn, cfg in dc._DEVICES.items():
        assert "name" in cfg, pn
        assert "type" in cfg, pn
        assert "capabilities" in cfg, pn
        assert isinstance(cfg["capabilities"], list), pn
        # type 必须与 infer_device_type 一致
        assert cfg["type"] == infer_device_type(pn), f"{pn}: {cfg['type']}"


def test_run_all():
    tests = [v for k, v in sorted(globals().items())
             if k.startswith("test_") and callable(v) and k != "test_run_all"]
    passed = 0
    for t in tests:
        try:
            t()
            print(f"  [PASS] {t.__name__}")
            passed += 1
        except AssertionError as e:
            print(f"  [FAIL] {t.__name__}: {e}")
            raise
    print(f"\n{passed}/{len(tests)} tests passed.")


if __name__ == "__main__":
    test_run_all()
