# -*- coding: utf-8 -*-
"""多联接收器子设备状态同步回归测试。

覆盖修复的三个缺陷：
1. usb_comm._dispatch_dongle_event 收到 CONNECTED_DEVICE 事件时同步 _sub_macs
2. device_mgr._connect_dongle_sub 幂等（已挂载不重复 attach）
3. device_mgr.drop_dongle_sub 掉线清理并回调
"""
import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from unittest.mock import MagicMock, patch

from comm_lib.channels.usb_comm import UsbComm
from device_mgr import DeviceManager, _DongleSubCommAdapter


def _make_dongle_comm() -> UsbComm:
    """构造一个假连接的 dongle UsbComm（不打真 USB）。"""
    comm = UsbComm()
    comm._dev = MagicMock()
    comm._mode = UsbComm.Mode.DONGLE
    comm._ep_in = 0x83
    comm._ep_out = 0x06
    return comm


def test_connected_event_updates_sub_macs():
    """GET_CONNECTED_DEVICE 响应（state=1）应把 mac 加入 _sub_macs。"""
    comm = _make_dongle_comm()
    mac = bytes.fromhex("2c81a7e049ff")
    # 事件 payload: [type=0x01][version][state=1][mac[6]]...（对齐真实帧）
    payload = bytes([0x01, 0x10, 0x01]) + mac + b"\x00"
    comm._dispatch_dongle_event(event_type=0x01, state=0x01, mac=mac, inner=b"\x00")
    assert mac in comm._sub_macs, f"state=1 事件后 _sub_macs 应包含 {mac.hex()}"


def test_connected_event_no_duplicate():
    comm = _make_dongle_comm()
    mac = bytes.fromhex("2c81a7e049ff")
    comm._dispatch_dongle_event(0x01, 0x01, mac, b"")
    comm._dispatch_dongle_event(0x01, 0x01, mac, b"")
    assert comm._sub_macs.count(mac) == 1


def test_paired_not_connected_event_removes():
    """state!=1（已配对未连接）应从 _sub_macs 移除。"""
    comm = _make_dongle_comm()
    mac = bytes.fromhex("2c81a7e049ff")
    comm._sub_macs.append(mac)
    comm._dispatch_dongle_event(0x01, 0x00, mac, b"")
    assert mac not in comm._sub_macs


def test_disconnect_event_removes():
    comm = _make_dongle_comm()
    mac = bytes.fromhex("2c81a7e049ff")
    comm._sub_macs.append(mac)
    comm._dispatch_dongle_event(0x02, 0x00, mac, b"")
    assert mac not in comm._sub_macs


def test_connect_dongle_sub_idempotent():
    """已在池中的子设备再次 connect 不重复 attach。"""
    mgr = DeviceManager()
    mac = bytes.fromhex("2c81a7e049ff")
    address = mac.hex(":")

    fake_dongle = _make_dongle_comm()
    fake_dongle._sub_macs = [mac]  # 模拟 dongle 已挂载该子设备
    mgr._dongle = fake_dongle

    with patch.object(UsbComm, "Mode", create=True):
        info = type("I", (), {
            "transport": "usb", "address": address, "name": "",
            "product_no": None, "device_type": 2, "extra": {"mac": mac},
        })()
        # 用真实路径但跳过 UsbComm 构造：直接测幂等性
        # 第一次：走 adapter 注册
        adapter = _DongleSubCommAdapter(fake_dongle, mac)
        from device_model import MouseDevice
        dev = MouseDevice(adapter, address=address, name="t",
                          transport="usb", connection_type="usb_dongle")
        mgr._register(dev, address)
        # 第二次连接同一 mac：应直接返回池中实例
        got = mgr.connect_dongle_sub(mac)
        assert got is dev, "connect_dongle_sub 应幂等返回池中实例"


def test_drop_dongle_sub_notifies():
    """drop_dongle_sub 应清池并触发回调。"""
    mgr = DeviceManager()
    mac = bytes.fromhex("2c81a7e049ff")
    address = mac.hex(":")
    fake_dongle = _make_dongle_comm()
    adapter = _DongleSubCommAdapter(fake_dongle, mac)
    from device_model import MouseDevice
    dev = MouseDevice(adapter, address=address, name="t",
                      transport="usb", connection_type="usb_dongle")
    mgr._register(dev, address)
    mgr._all_comms.append(adapter)

    notified = []
    mgr.on_dongle_sub_disconnected = notified.append
    mgr.drop_dongle_sub(mac)
    assert notified == [address], "掉线回调应收到 address"
    assert mgr.get(address) is None, "掉线后应从池中移除"


def test_drop_dongle_sub_unknown_silent():
    """掉线未知子设备不应抛异常、不应误回调。"""
    mgr = DeviceManager()
    notified = []
    mgr.on_dongle_sub_disconnected = notified.append
    mgr.drop_dongle_sub(bytes.fromhex("aabbccddeeff"))
    assert notified == []


def test_scan_event_routed_via_device_event():
    """扫描应答走 DEVICE_EVENT type=0x03，应触发 on_device_found 并带设备名。"""
    comm = _make_dongle_comm()
    mac = bytes.fromhex("2c81a7f049ff")
    # 真机帧：03 01 25 2c81a7f049ff "T8(2C:81)"\x00...
    name = b"T8(2C:81)"
    payload = bytes([0x03, 0x01, 0x25]) + mac + name + b"\x00"
    outer = type("F", (), {"cmd": 0x49, "payload": payload})()
    found = []
    comm.on_device_found = found.append
    comm._handle_dongle_frame(outer)
    assert len(found) == 1, "扫描事件应触发 on_device_found"
    info = found[0]
    assert info.address == mac.hex(":")
    assert info.name == "T8(2C:81)", f"应提取广播名, got {info.name!r}"
    assert info.extra["rssi"] == 0x25
    # 扫描结果不应混进 _sub_macs
    assert mac not in comm._sub_macs


def test_adapter_disconnect_does_not_detach():
    """程序关闭/被动清理时 adapter.disconnect 不应 detach 子设备。"""
    comm = _make_dongle_comm()
    mac = bytes.fromhex("2c81a7e049ff")
    adapter = _DongleSubCommAdapter(comm, mac)
    adapter.disconnect()
    # detach 会写 USB —— 用 mock dev 断言没有写发生
    assert comm._dev.write.call_count == 0, "adapter.disconnect 不应发送 REMOVE_DEVICE"


if __name__ == "__main__":
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
    sys.exit(1 if failed else 0)
