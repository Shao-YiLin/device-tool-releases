"""device_mgr 单元测试。

用 MockComm 验证扫描分发、设备类型判定、连接入池、断开出池、
多设备管理、dongle 子设备路由等。

运行：python new_project/test/test_device_mgr.py
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from comm_lib.channels import BaseComm, DeviceInfo
from device_mgr import (
    DEVICE_TYPE_KEYBOARD,
    DEVICE_TYPE_MOUSE,
    DeviceManager,
)
from device_model import KbDevice, MouseDevice


# ---- Mock 工厂：patch BleComm/UsbComm 避免真实硬件依赖 ----
class _MockBleComm(BaseComm):
    def __init__(self, scan_results=None):
        super().__init__()
        self._scan_results = scan_results or []
        self._connected = False
        self.connect_target = None

    def scan(self):
        for info in self._scan_results:
            if self.on_device_found:
                self.on_device_found(info)

    def connect(self, target):
        self.connect_target = target
        self._connected = True

    def disconnect(self):
        self._connected = False

    def send_raw(self, frame):
        return len(frame)

    @property
    def is_connected(self):
        return self._connected


class _MonkeyCtx:
    def __init__(self):
        self.items = []

    def set(self, module, name, value):
        orig = getattr(module, name)
        self.items.append((module, name, orig))
        setattr(module, name, value)

    def restore(self):
        for module, name, orig in self.items:
            setattr(module, name, orig)
        self.items.clear()


def test_scan_ble_distributes_results():
    import comm_lib.channels.ble_comm as ble_mod

    ctx = _MonkeyCtx()
    results = [DeviceInfo("ble", "AA:BB", "M110", device_type=DEVICE_TYPE_MOUSE)]

    class _FakeBle(_MockBleComm):
        def __init__(self, scan_timeout=5.0):
            super().__init__(results)

    ctx.set(ble_mod, "BleComm", _FakeBle)

    try:
        mgr = DeviceManager()
        out = mgr.scan_ble()
        assert out == results
    finally:
        ctx.restore()


def test_connect_ble_creates_mouse_for_mouse_type():
    import comm_lib.channels.ble_comm as ble_mod

    ctx = _MonkeyCtx()
    info = DeviceInfo("ble", "AA:BB", "M110", device_type=DEVICE_TYPE_MOUSE)

    class _FakeBle(_MockBleComm):
        def __init__(self, scan_timeout=5.0):
            super().__init__([info])

    ctx.set(ble_mod, "BleComm", _FakeBle)
    try:
        mgr = DeviceManager()
        dev = mgr.connect(info)
        assert isinstance(dev, MouseDevice)
        assert len(mgr) == 1
    finally:
        ctx.restore()


def test_connect_creates_keyboard_for_kb_type():
    import comm_lib.channels.ble_comm as ble_mod

    ctx = _MonkeyCtx()
    info = DeviceInfo("ble", "CC:DD", "K320", device_type=DEVICE_TYPE_KEYBOARD)

    class _FakeBle(_MockBleComm):
        def __init__(self, scan_timeout=5.0):
            super().__init__([info])

    ctx.set(ble_mod, "BleComm", _FakeBle)
    try:
        mgr = DeviceManager()
        dev = mgr.connect(info)
        assert isinstance(dev, KbDevice)
    finally:
        ctx.restore()


def test_disconnect_removes_from_pool():
    import comm_lib.channels.ble_comm as ble_mod

    ctx = _MonkeyCtx()
    info = DeviceInfo("ble", "AA:BB", "M110", device_type=DEVICE_TYPE_MOUSE)

    class _FakeBle(_MockBleComm):
        def __init__(self, scan_timeout=5.0):
            super().__init__([info])

    ctx.set(ble_mod, "BleComm", _FakeBle)
    try:
        mgr = DeviceManager()
        mgr.connect(info)
        assert len(mgr) == 1
        assert mgr.disconnect("AA:BB") is True
        assert len(mgr) == 0
        assert mgr.disconnect("not-exist") is False
    finally:
        ctx.restore()


def test_multiple_devices_in_pool():
    import comm_lib.channels.ble_comm as ble_mod

    ctx = _MonkeyCtx()
    info1 = DeviceInfo("ble", "AA:01", "M110", device_type=DEVICE_TYPE_MOUSE)
    info2 = DeviceInfo("ble", "AA:02", "K320", device_type=DEVICE_TYPE_KEYBOARD)

    class _FakeBle(_MockBleComm):
        def __init__(self, scan_timeout=5.0):
            super().__init__([info1, info2])

    ctx.set(ble_mod, "BleComm", _FakeBle)
    try:
        mgr = DeviceManager()
        mgr.connect(info1)
        mgr.connect(info2)
        assert len(mgr) == 2
        devs = mgr.devices()
        types = sorted(d.DEVICE_TYPE for d in devs)
        assert types == [0x31, 0x32]
    finally:
        ctx.restore()


def test_disconnect_all_clears_pool():
    import comm_lib.channels.ble_comm as ble_mod

    ctx = _MonkeyCtx()
    info1 = DeviceInfo("ble", "AA:01", "M110", device_type=DEVICE_TYPE_MOUSE)
    info2 = DeviceInfo("ble", "AA:02", "M111", device_type=DEVICE_TYPE_MOUSE)

    class _FakeBle(_MockBleComm):
        def __init__(self, scan_timeout=5.0):
            super().__init__([info1, info2])

    ctx.set(ble_mod, "BleComm", _FakeBle)
    try:
        mgr = DeviceManager()
        mgr.connect(info1)
        mgr.connect(info2)
        mgr.disconnect_all()
        assert len(mgr) == 0
    finally:
        ctx.restore()


def test_device_iteration():
    import comm_lib.channels.ble_comm as ble_mod

    ctx = _MonkeyCtx()
    info = DeviceInfo("ble", "AA:01", "M110", device_type=DEVICE_TYPE_MOUSE)

    class _FakeBle(_MockBleComm):
        def __init__(self, scan_timeout=5.0):
            super().__init__([info])

    ctx.set(ble_mod, "BleComm", _FakeBle)
    try:
        mgr = DeviceManager()
        mgr.connect(info)
        devs = list(mgr)
        assert len(devs) == 1
    finally:
        ctx.restore()


def test_get_by_address():
    mgr = DeviceManager()
    # 直接塞入一个 mock device
    from device_model import MouseDevice

    class _C(BaseComm):
        def scan(self): pass
        def connect(self, t): pass
        def disconnect(self): pass
        def send_raw(self, f): return 0
        @property
        def is_connected(self): return True

    dev = MouseDevice(_C(), address="X")
    mgr._devices["X"] = dev
    assert mgr.get("X") is dev
    assert mgr.get("Y") is None


def test_is_known_device_name():
    """BLE 自动连接的名字过滤：本厂设备名通过，耳机/空名被排除。"""
    from comm_lib.channels.ble_device_enumeration import is_known_device_name
    # 键盘关键字
    assert is_known_device_name("T8 键盘")
    assert is_known_device_name("AT80")
    # 鼠标前缀
    assert is_known_device_name("MD_7541B2D09AB7")
    assert is_known_device_name("AM60")
    # 非本厂 / 空名 / None
    assert not is_known_device_name("WH-1000XM4")
    assert not is_known_device_name("")
    assert not is_known_device_name(None)


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
