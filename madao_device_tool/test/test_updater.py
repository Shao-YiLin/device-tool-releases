# -*- coding: utf-8 -*-
"""在线更新客户端（updater.py）单元测试。

用 mock 替换网络请求，验证：
1. 版本比较（v 前缀 / 补齐 / 数字序）
2. check() 正确解析 Release 并返回新版本
3. check() 已是最新返回 None
4. 缺少资产时报错
5. sha256 校验
"""
import hashlib
import os
import sys
import tempfile
from unittest.mock import patch

_PKG_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, _PKG_ROOT)

from updater import Updater, _cmp_version, current_version  # noqa: E402


def test_version_compare():
    assert _cmp_version("1.0.1", "1.0.0") > 0
    assert _cmp_version("1.0.0", "1.0.0") == 0
    assert _cmp_version("0.9.9", "1.0.0") < 0
    assert _cmp_version("v1.2", "1.1.9") > 0
    assert _cmp_version("1.0.10", "1.0.9") > 0
    assert _cmp_version("1.0.2", "1.0.1") > 0


def test_check_returns_update():
    release = {
        "tag_name": "v9.9.9",
        "body": "fix bug",
        "assets": [{"name": "MadaoDeviceTool.zip",
                    "browser_download_url": "http://example.com/x.zip",
                    "size": 1234}],
    }
    up = Updater()
    with patch.object(up, "_fetch_json", return_value=release):
        info = up.check()
    assert info is not None
    assert info.version == "9.9.9"
    assert info.download_url == "http://example.com/x.zip"
    assert info.size == 1234


def test_check_latest_returns_none():
    # 与本地当前版本相同 → 不视为新版本
    release = {"tag_name": f"v{current_version()}", "assets": []}
    up = Updater()
    with patch.object(up, "_fetch_json", return_value=release):
        assert up.check() is None


def test_check_missing_asset_raises():
    release = {"tag_name": "v9.9.9", "assets": []}
    up = Updater()
    with patch.object(up, "_fetch_json", return_value=release):
        try:
            up.check()
            assert False, "缺少资产应抛异常"
        except RuntimeError as e:
            assert "未找到资产" in str(e)


def test_sha256_verify():
    with tempfile.NamedTemporaryFile(delete=False) as f:
        f.write(b"hello")
        path = f.name
    expected = hashlib.sha256(b"hello").hexdigest()
    assert Updater.verify_sha256(path, expected)
    assert not Updater.verify_sha256(path, "deadbeef")
    os.unlink(path)


def test_current_version_fallback():
    # 无 version.txt 时返回 0.0.0（不抛异常）
    v = current_version()
    assert isinstance(v, str) and v


if __name__ == "__main__":
    fns = [v for k, v in sorted(globals().items()) if k.startswith("test_")]
    passed = 0
    for fn in fns:
        try:
            fn()
            print(f"PASS: {fn.__name__}")
            passed += 1
        except AssertionError as e:
            print(f"FAIL: {fn.__name__}: {e}")
        except Exception as e:
            print(f"ERROR: {fn.__name__}: {type(e).__name__}: {e}")
    print(f"\n{'=' * 40}\n通过: {passed}, 失败: {len(fns) - passed}")
    sys.exit(0 if passed == len(fns) else 1)
