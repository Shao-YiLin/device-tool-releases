# -*- coding: utf-8 -*-
"""多设备并发 OTA 回归测试。

覆盖多设备并发升级所依赖的三件事：
1. dongle 转发帧带上来源子设备 mac（ProtocolFrame.source_mac）
2. dongle 子设备 adapter 按 mac 隔离应答——多子设备并发请求不串台
3. UsbComm 写串行化——并发发送时 USB 写不交错

另含一条 GUI 层用例：批量升级对话框对多台设备并发执行升级。

运行：python madao_device_tool/test/test_ota_multi_device.py
"""
import os
import struct
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from unittest.mock import MagicMock

from comm_lib.channels.usb_comm import UsbComm
from comm_lib.protocol.cmd import Command
from comm_lib.protocol.frame import ProtocolFrame, build_frame
from comm_lib.protocol.prop_key import PropKey
from device_mgr import _DongleSubCommAdapter
from device_model import MouseDevice


MAC_A = bytes.fromhex("aabbccddeeff")
MAC_B = bytes.fromhex("112233445566")


def _make_dongle_comm() -> UsbComm:
    """构造一个假连接的 dongle UsbComm（不打真 USB）。"""
    comm = UsbComm()
    comm._dev = MagicMock()
    comm._mode = UsbComm.Mode.DONGLE
    comm._ep_in = 0x83
    comm._ep_out = 0x06
    return comm


def _make_device(comm, mac) -> MouseDevice:
    return MouseDevice(comm, address=mac.hex(":"), name="dev-" + mac.hex(":")[:2],
                       transport="usb", connection_type="usb_dongle")


def _write_data_resp_payload(mac: bytes, inner: bytes) -> bytes:
    """构造 WRITE_DATA_RESP 的 payload：[type][mac[6]][子设备私有帧]。"""
    return bytes([0x02]) + mac + inner


def _ack_frame(seq: int, err: int = 1, data: bytes = b"1042") -> bytes:
    """构造 ACK 帧（err_code + data）。"""
    return build_frame(Command.ACK, struct.pack("<I", err) + data, seq)


# ---------------------------------------------------------------------------
# 1. 协议层：帧默认无 source_mac
# ---------------------------------------------------------------------------

def test_direct_frame_has_no_source_mac():
    """直连 / 自身帧不带 source_mac（None）。"""
    frame = ProtocolFrame(0xFE, 0, Command.ACK, b"")
    assert frame.source_mac is None


# ---------------------------------------------------------------------------
# 2. 通道层：dongle 内层帧打上来源 mac
# ---------------------------------------------------------------------------

def test_dongle_write_data_resp_tags_source_mac():
    """WRITE_DATA_RESP 的内层帧应带上来源子设备 mac。"""
    comm = _make_dongle_comm()
    got = []
    comm.on_frame = got.append

    inner = _ack_frame(0)
    outer = ProtocolFrame(0xFE, 0, 0x47, _write_data_resp_payload(MAC_A, inner))
    comm._handle_dongle_frame(outer)

    assert len(got) == 1, "内层帧应被上抛"
    assert got[0].source_mac == MAC_A, "内层帧应标记来源子设备 mac"


def test_dongle_device_event_tags_source_mac():
    """DEVICE_EVENT 透传的内层帧同样应带 source_mac。"""
    comm = _make_dongle_comm()
    got = []
    comm.on_frame = got.append

    inner = _ack_frame(3)
    # payload=[type=0x01][version][state][mac[6]][data...]
    payload = bytes([0x01, 0x10, 0x01]) + MAC_B + inner
    outer = ProtocolFrame(0xFE, 0, 0x49, payload)
    comm._handle_dongle_frame(outer)

    assert len(got) == 1
    assert got[0].source_mac == MAC_B


# ---------------------------------------------------------------------------
# 3. 管理层：adapter 按 mac 隔离应答（并发升级不串台）
# ---------------------------------------------------------------------------

def test_adapter_routes_only_own_mac():
    """A 子设备的应答不应送到 B 子设备的 adapter。"""
    comm = _make_dongle_comm()
    adapter_a = _DongleSubCommAdapter(comm, MAC_A)
    adapter_b = _DongleSubCommAdapter(comm, MAC_B)
    got_a, got_b = [], []
    adapter_a.on_frame = got_a.append
    adapter_b.on_frame = got_b.append
    adapter_a.install_frame_router()
    adapter_b.install_frame_router()

    inner = _ack_frame(0)
    outer = ProtocolFrame(0xFE, 0, 0x47, _write_data_resp_payload(MAC_A, inner))
    comm._handle_dongle_frame(outer)

    assert len(got_a) == 1, "A 应收到自己的应答"
    assert len(got_b) == 0, "B 不应收到 A 的应答"


def test_concurrent_requests_do_not_cross_talk():
    """两台子设备并发请求时，A 的应答不能唤醒 B 的等待。

    各设备的 sequence 计数独立、会撞号，必须靠 source_mac 隔离。
    """
    comm = _make_dongle_comm()
    comm._sub_macs = [MAC_A, MAC_B]  # 两台子设备都已挂载在 dongle 上
    adapter_a = _DongleSubCommAdapter(comm, MAC_A)
    adapter_b = _DongleSubCommAdapter(comm, MAC_B)
    adapter_a.install_frame_router()
    adapter_b.install_frame_router()
    dev_a = _make_device(adapter_a, MAC_A)
    dev_b = _make_device(adapter_b, MAC_B)

    results = {}
    errors = {}

    def request(dev, key):
        try:
            results[key] = dev.send_and_wait(
                Command.GET_SLAVE_PROP, bytes([PropKey.PRODUCT_NO]), timeout=3.0)
        except Exception as e:
            errors[key] = e

    ta = threading.Thread(target=request, args=(dev_a, "a"))
    tb = threading.Thread(target=request, args=(dev_b, "b"))
    ta.start()
    tb.start()
    time.sleep(0.2)  # 等两个请求都挂起

    assert dev_a._pending and dev_b._pending, "两个请求都应处于等待状态"
    seq_a = list(dev_a._pending.keys())[0]
    seq_b = list(dev_b._pending.keys())[0]

    # 只回 A 的应答
    inner = _ack_frame(seq_a, 1, b"AAAA")
    outer = ProtocolFrame(0xFE, 0, 0x47, _write_data_resp_payload(MAC_A, inner))
    comm._handle_dongle_frame(outer)

    ta.join(timeout=2)
    assert "a" in results, "A 应收到应答"
    assert results["a"][0] == 1 and results["a"][1] == b"AAAA"
    assert "b" not in results, "B 不应被 A 的应答唤醒"
    assert dev_b._pending, "B 的请求应仍在等待"

    # 再回 B 的应答
    inner = _ack_frame(seq_b, 1, b"BBBB")
    outer = ProtocolFrame(0xFE, 0, 0x47, _write_data_resp_payload(MAC_B, inner))
    comm._handle_dongle_frame(outer)

    tb.join(timeout=2)
    assert "b" in results, "B 应收到自己的应答"
    assert results["b"][1] == b"BBBB"


# ---------------------------------------------------------------------------
# 4. 通道层：USB 写串行化
# ---------------------------------------------------------------------------

def test_usb_writes_are_serialized():
    """并发写 USB 时不能交错（否则协议帧损坏）。"""
    comm = _make_dongle_comm()
    concurrent = {"now": 0, "max": 0}
    lock = threading.Lock()

    def fake_write(ep, data, timeout=None):
        with lock:
            concurrent["now"] += 1
            concurrent["max"] = max(concurrent["max"], concurrent["now"])
        time.sleep(0.02)
        with lock:
            concurrent["now"] -= 1
        return len(data)

    comm._dev.write.side_effect = fake_write

    threads = [threading.Thread(target=comm._write_raw_ep, args=(b"\xfe" * 32,))
               for _ in range(4)]
    for t in threads:
        t.start()
    for t in threads:
        t.join(timeout=3)

    assert concurrent["max"] == 1, f"USB 写应串行化，最大并发 {concurrent['max']} > 1"


# ---------------------------------------------------------------------------
# 5. GUI 层：批量升级对话框并发升级多台设备
# ---------------------------------------------------------------------------

class _FakeDevice:
    """假设备：模拟鉴权已完成 + OTA 升级过程。"""

    def __init__(self, address, name):
        self.address = address
        self.name = name
        self.product_no = b"1042"
        self._auth_session_key = b"k" * 16  # 已鉴权，跳过鉴权流程
        self.started = threading.Event()
        self.finished = threading.Event()
        self.t0 = 0.0
        self.t1 = 0.0

    def perform_ota(self, firmware, on_progress=None, on_check_cancel=None):
        self.t0 = time.time()
        self.started.set()
        total = 4
        for i in range(total):
            time.sleep(0.15)
            if on_progress:
                on_progress((i + 1) * 8, total * 8)
        self.t1 = time.time()
        self.finished.set()
        return True


def test_ota_dialog_upgrades_multiple_devices_concurrently():
    """批量升级对话框应对多台设备并发执行升级。"""
    os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
    from PySide6.QtWidgets import QApplication
    from gui.ota_dialog import OtaDialog

    app = QApplication.instance() or QApplication([])

    dev_a = _FakeDevice("04:aa", "M610-A")
    dev_b = _FakeDevice("04:bb", "M610-B")
    dlg = OtaDialog([dev_a, dev_b])

    # 给两台设备准备固件文件
    import tempfile
    fw_path = os.path.join(tempfile.gettempdir(), "ota_multi_test.bin")
    with open(fw_path, "wb") as f:
        f.write(b"\x00" * 256)
    try:
        for row in dlg._rows:
            row.firmware_path = fw_path
            row.fw_edit.setText(fw_path)

        dlg._on_start()
        assert dev_a.started.wait(3), "设备 A 应开始升级"
        assert dev_b.started.wait(3), "设备 B 应开始升级"
        assert dev_a.finished.wait(5) and dev_b.finished.wait(5), "两台设备都应升级完成"

        # 并发验证：两台设备的升级区间应有重叠
        overlap = min(dev_a.t1, dev_b.t1) - max(dev_a.t0, dev_b.t0)
        assert overlap > 0, "两台设备应并发升级（时间区间重叠）"

        # 结果回填到表格（done 信号经队列回主线程，需要转事件循环）
        deadline = time.time() + 5
        while time.time() < deadline and not all(r.finished for r in dlg._rows):
            app.processEvents()
            time.sleep(0.02)
        for row in dlg._rows:
            assert row.status_label.text() == "升级成功", row.status_label.text()
            assert row.progress.value() == 100
    finally:
        dlg.close()
        os.remove(fw_path)


def _run_all():
    fns = [v for k, v in sorted(globals().items()) if k.startswith("test_")]
    failed = 0
    for fn in fns:
        try:
            fn()
            print(f"PASS: {fn.__name__}")
        except AssertionError as e:
            failed += 1
            print(f"FAIL: {fn.__name__}: {e}")
        except Exception as e:
            failed += 1
            print(f"ERROR: {fn.__name__}: {type(e).__name__}: {e}")
    print(f"\n{'=' * 40}\n通过: {len(fns) - failed}, 失败: {failed}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(_run_all())
