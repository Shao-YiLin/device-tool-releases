"""DeviceWorkerPool（按桶并行 worker 池）测试。

桶路由规则：
  - 连接/断开/接收器 -> "connect" 桶（全局串行）
  - 扫描 -> "scan" 桶
  - 设备读写 -> 每设备一桶；多联接收器子设备（connection_type=usb_dongle）
    共享 dongle 通道，统一归 "__dongle__" 桶
"""

import os
import sys

_PKG_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
for _p in (_PKG_ROOT, os.path.join(_PKG_ROOT, "gui")):
    if _p not in sys.path:
        sys.path.insert(0, _p)
os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from PySide6.QtWidgets import QApplication

app = QApplication.instance() or QApplication([])

from device_worker import DeviceWorker, DeviceWorkerPool


class _StubTool:
    def __init__(self):
        self.devices = {}

    def get_device(self, address):
        return self.devices.get(address)


class _FakeDev:
    def __init__(self, connection_type=""):
        self.connection_type = connection_type


def test_bucket_key_dongle_sub_serialized():
    """多联接收器子设备归 __dongle__ 桶（共享通道必须串行），其他设备各自一桶。"""
    tool = _StubTool()
    tool.devices["04:bb"] = _FakeDev("usb_dongle")
    tool.devices["04:aa"] = _FakeDev("usb_direct")
    pool = DeviceWorkerPool(tool=tool)
    try:
        assert pool._bucket_key_for_address("04:bb") == "__dongle__"
        assert pool._bucket_key_for_address("04:aa") == "04:aa"
        assert pool._bucket_key_for_address("未注册设备") == "未注册设备"
        assert pool._bucket_key_for_address("") == "misc"
    finally:
        pool.shutdown()


def test_set_tool_propagates_to_buckets():
    tool = _StubTool()
    pool = DeviceWorkerPool(tool=tool)
    try:
        tool2 = _StubTool()
        pool.set_tool(tool2)
        for _thread, worker in pool._buckets.values():
            assert worker._tool is tool2
    finally:
        pool.shutdown()


def test_async_op_routes_per_address():
    """async_op 按地址分桶：同地址同桶（串行），不同地址不同桶（并行）。"""
    pool = DeviceWorkerPool(tool=_StubTool())
    try:
        pool.async_op("op1", "4:8", "get_battery")
        pool.async_op("op2", "4:8", "get_battery")
        pool.async_op("op3", "4:9", "get_battery")
        keys = set(pool._buckets.keys())
        assert "4:8" in keys and "4:9" in keys
    finally:
        pool.shutdown()


def test_exec_connect_dongle_signals():
    """dongle 通道连接在 worker 槽内执行，成功/失败都发对应信号。"""
    captured = {}

    class _OkTool:
        def connect_dongle_only(self, info):
            captured["info"] = info
            return "DONGLE"

    w = DeviceWorker(_OkTool())
    w.dongle_connected.connect(lambda d: captured.setdefault("dongle", d))
    w._exec_connect_dongle("INFO")
    assert captured.get("dongle") == "DONGLE"

    class _FailTool:
        def connect_dongle_only(self, info):
            raise RuntimeError("boom")

    w2 = DeviceWorker(_FailTool())
    errs = []
    w2.error.connect(lambda op, msg: errs.append((op, msg)))
    w2._exec_connect_dongle("INFO")
    assert errs and errs[0][0] == "connect_dongle" and "boom" in errs[0][1]


if __name__ == "__main__":
    fns = [v for k, v in sorted(globals().items()) if k.startswith("test_")]
    passed = 0
    for fn in fns:
        try:
            fn()
            print(f"PASS {fn.__name__}")
            passed += 1
        except AssertionError as e:
            print(f"FAIL: {fn.__name__}: {e}")
        except Exception as e:
            print(f"ERROR: {fn.__name__}: {type(e).__name__}: {e}")
    print(f"通过: {passed}, 失败: {len(fns) - passed}")
    sys.exit(0 if passed == len(fns) else 1)
