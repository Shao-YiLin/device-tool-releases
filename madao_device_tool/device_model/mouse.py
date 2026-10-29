"""鼠标设备模型。

差异化能力（验收 3.3 第3条）：
    get_cpi()   -> 当前 CPI 等级
    set_cpi(level)
    get_rate()  -> 当前回报率（polling rate, Hz）
    set_rate(hz)

CPI 与回报率都走 SET_SLAVE_PROP / GET_SLAVE_PROP 通用属性通道，
仅 prop_key 不同（对齐 src_archive/windows/libProtocolFramework/
protocol_logic_manager.c::_set_dpi 与 AM50 logic_protocol_manager.c::MOUSE_FREQUENCY）。
"""

from __future__ import annotations

import hashlib
import time
from typing import Tuple

from comm_lib.protocol.cmd import Command, ErrCode
from comm_lib.protocol.frame import bytes_to_u32, u32_to_bytes
from comm_lib.protocol.prop_key import PropKey

from .base import BaseDevice, DeviceError


# RGB 灯效模式（对齐设备端 device_config_set_rgb）
RGB_MODE_CLOSE = 0
RGB_MODE_NORMAL = 1
RGB_MODE_BREATH = 3
RGB_MODE_TRIGGER = 4


def compute_wifi_credentials(product_no: str, mac: bytes) -> Tuple[str, str]:
    """快传 WiFi 凭据生成（对齐 C generate_wifi_credentials()）。

    input = 产品编号前 4 字符 + MAC 6 字节的大写 HEX（12 字符）；
    SHA-256 后：SSID = "MD_" + hash[0:6] 大写 HEX，密码 = hash[6:14] 大写 HEX。
    """
    pno = str(product_no or "")[:4]
    mac_hex = "".join("%02X" % b for b in mac)
    digest = hashlib.sha256((pno + mac_hex).encode("ascii")).digest()
    ssid = "MD_" + digest[:6].hex().upper()
    pwd = digest[6:14].hex().upper()
    return ssid, pwd


class MouseDevice(BaseDevice):
    """鼠标设备。"""

    DEVICE_TYPE = 0x31  # 对齐 lib_device_define.h::LIBMOUSE_DEVICE_TYPE_MOUSE

    def __init__(self, comm, address: str = "", name: str = "",
                 transport: str = "", connection_type: str = "") -> None:
        super().__init__(comm, address=address, name=name,
                         transport=transport, connection_type=connection_type)

    def get_cpi(self) -> int:
        """获取当前 CPI 等级。

        对齐 protocol_logic_manager.c::_get_dpi:
          cmd=GET_SLAVE_PROP, payload=[PROP_KEY_DPI]
        设备应答 data 为 4 字节 LE 的 CPI 等级（见 AM50 _prop_resp 的 DPI 分支）。
        """
        err, data = self._get_slave_prop(PropKey.DPI)
        if err != ErrCode.SUCCESS or len(data) < 4:
            raise DeviceError(f"获取 CPI 失败 err={err}")
        return bytes_to_u32(data[:4])

    def set_cpi(self, level: int) -> int:
        """设置 CPI 等级。

        对齐 protocol_logic_manager.c::_set_dpi:
          cmd=SET_SLAVE_PROP, payload=[PROP_KEY_DPI][level(4 LE)]
        设备端取低 8 位作为 bt_dpi（见 AM50 _set_prop 的 DPI 分支）。
        """
        err, _ = self._set_slave_prop(PropKey.DPI, u32_to_bytes(level))
        if err != ErrCode.SUCCESS:
            raise DeviceError(f"设置 CPI 失败 err={err}")
        return err

    def get_rate(self) -> int:
        """获取当前回报率（Hz）。

        对齐 protocol_logic_manager.c + AM50 logic_protocol_manager.c::MOUSE_FREQUENCY:
          cmd=GET_SLAVE_PROP, payload=[PROP_KEY_POLLING_RATE]
        设备应答 data 为 4 字节 LE 的回报率值（USB 模式下为 usb_frequency，
        BLE 模式为 ble_frequency，见 AM50 _prop_resp 的 MOUSE_FREQUENCY 分支）。
        """
        err, data = self._get_slave_prop(PropKey.POLLING_RATE)
        if err != ErrCode.SUCCESS or len(data) < 4:
            raise DeviceError(f"获取回报率失败 err={err}")
        return bytes_to_u32(data[:4])

    def set_quick_transfer(self, enabled: bool) -> int:
        """设置 AM60 FTP 快传开关。

        对齐马道设备通信协议.docx 2026/01/17:
          cmd=SET_SLAVE_PROP, payload=[0x2A][value(1)]
          value: 0=关闭, 1=打开
        """
        value = bytes([1 if enabled else 0])
        err, _ = self._set_slave_prop(PropKey.QUICK_TRANSFER, value)
        if err != ErrCode.SUCCESS:
            raise DeviceError(f"设置快传开关失败 err={err}")
        return err

    def get_unsynced_file_count(self) -> int:
        """获取 AM60 未同步文件数量。

        对齐马道设备通信协议.docx 2026/01/17:
          cmd=GET_SLAVE_PROP, payload=[0x29]
          应答长度 32 byte，通常前 4 字节 LE 为数量。
        """
        err, data = self._get_slave_prop(PropKey.UNSYNCHED_FILE_COUNT)
        if err != ErrCode.SUCCESS:
            raise DeviceError(f"获取未同步文件数量失败 err={err}")
        if len(data) < 4:
            raise DeviceError(f"未同步文件数量应答长度不足: {len(data)}")
        return bytes_to_u32(data[:4])

    def set_unsynced_file_count(self, count: int) -> int:
        """设置 AM60 未同步文件数量（配合固件/同步流程调试）。

        SET_SLAVE_PROP key=0x29，value = 数量（4 字节 LE），
        与 GET 0x29 应答（前 4 字节 LE 为数量）格式对应。
        """
        err, _ = self._set_slave_prop(PropKey.UNSYNCHED_FILE_COUNT, u32_to_bytes(count))
        if err != ErrCode.SUCCESS:
            raise DeviceError(f"设置未同步文件数量失败 err={err}")
        return err

    def get_wifi_credentials(self) -> Tuple[str, str]:
        """快传 WiFi 凭据（SSID + 密码），打开快传后展示供连接热点。

        对齐 C generate_wifi_credentials()：input = 产品编号前 4 字符 +
        MAC 大写 HEX，SHA-256 后取 hash[0:6] 为 SSID、hash[6:14] 为密码。
        product_no 缺失时现场读一次；MAC 走 GET_SLAVE_PROP[0x0A]。
        """
        pno = str(getattr(self, "product_no", "") or "")
        if len(pno) < 4:
            pno = str(self.get_product_no() or "")
        mac = self.get_device_mac()
        if not mac or len(mac) != 6:
            raise DeviceError(f"获取设备 MAC 失败: {mac!r}")
        return compute_wifi_credentials(pno, mac)

    # ---- RGB 灯光（仅 G50 / AM40 / AM50 系列 / AM60 支持） ----
    def get_rgb_theme(self) -> Tuple[int, int, int, int]:
        """获取当前 RGB 主题颜色与模式。

        获取使用 key=0x15，设置使用 key=0x12。
        返回数据格式（参考设备端 0x15 应答代码）：
          data[0] = rgb_theme
          data[1..4] = cycle_time (4 LE)
          data[5..7] = R, G, B（单色/关闭）或第一个 RGB（呼吸/触发）
          呼吸/触发模式长度可能大于 8，携带多组 RGB。
        """
        err, data = self._get_slave_prop(PropKey.GET_THEME)
        if err != ErrCode.SUCCESS:
            raise DeviceError(f"获取主题色失败 err={err}")
        if len(data) < 1:
            raise DeviceError(f"获取主题色应答长度不足: {len(data)}")
        rgb_type = data[0]
        if rgb_type == RGB_MODE_CLOSE:
            return RGB_MODE_CLOSE, 0, 0, 0
        if len(data) >= 8:
            return rgb_type, data[5], data[6], data[7]
        raise DeviceError(f"获取主题色应答长度不足 8 字节: len={len(data)} data={data.hex()}")

    def set_rgb(self, mode: int, cycle_timer_ms: int, r: int, g: int, b: int) -> int:
        """设置 RGB 灯光。

        对齐设备端设置回调统一格式（key=0x12）：
          data[0] = rgb_type (0/1/2/3/4)
          data[1..4] = cycle_time (4 LE)
          data[5..7] = R, G, B
        所有模式颜色均从 data+5 取 3 字节。
        """
        payload = bytes([mode & 0xFF]) + u32_to_bytes(cycle_timer_ms) + bytes([r & 0xFF, g & 0xFF, b & 0xFF])
        err, _ = self._set_slave_prop(PropKey.RGB, payload)
        if err != ErrCode.SUCCESS:
            raise DeviceError(f"设置 RGB 失败 err={err}")
        return err

    def get_work_mode(self) -> int:
        """获取 M610 工作模式（笔模式 / 鼠标模式）。

        对齐马道设备通信协议.docx 0x19 属性：
          cmd=GET_SLAVE_PROP, payload=[0x19]
          应答 4 字节 LE，1=笔模式，2=鼠标模式。
        """
        err, data = self._get_slave_prop(PropKey.M610_WORK_MODE)
        if err != ErrCode.SUCCESS or len(data) < 4:
            raise DeviceError(f"获取工作模式失败 err={err}")
        return bytes_to_u32(data[:4])

    def get_touch_sensitivity(self) -> int:
        """获取触摸滚动灵敏度（AM60 专属）。

        cmd=GET_SLAVE_PROP, payload=[TOUCH_SENSITIVITY(0x2B)]
        应答 4 字节 LE，范围 1~10。
        """
        err, data = self._get_slave_prop(PropKey.TOUCH_SENSITIVITY)
        if err != ErrCode.SUCCESS or len(data) < 4:
            raise DeviceError(f"获取触摸灵敏度失败 err={err}")
        return bytes_to_u32(data[:4])

    def set_touch_sensitivity(self, value: int) -> int:
        """设置触摸滚动灵敏度（AM60 专属）。

        cmd=SET_SLAVE_PROP, payload=[TOUCH_SENSITIVITY(0x2B)][value(4 LE)]
        value 范围 1~10，超出设备返回失败。
        """
        err, _ = self._set_slave_prop(PropKey.TOUCH_SENSITIVITY, u32_to_bytes(value))
        if err != ErrCode.SUCCESS:
            raise DeviceError(f"设置触摸灵敏度失败 err={err}")
        return err

    def set_slave_screen_sleep(self, seconds: int) -> int:
        """设置从机屏幕休眠时间（AM60 专属）。

        cmd=SET_SLAVE_PROP, payload=[SLAVE_SCREEN_SLEEP(0x2C)][seconds(4 LE)]
        seconds 为休眠秒数。
        """
        err, _ = self._set_slave_prop(PropKey.SLAVE_SCREEN_SLEEP, u32_to_bytes(seconds))
        if err != ErrCode.SUCCESS:
            raise DeviceError(f"设置屏幕休眠时间失败 err={err}")
        return err

    def get_slave_screen_sleep(self) -> int:
        """获取从机屏幕休眠时间（AM60 专属，秒）。

        cmd=GET_SLAVE_PROP, payload=[SLAVE_SCREEN_SLEEP(0x2C)]
        应答 4 字节 LE 秒数。
        """
        err, data = self._get_slave_prop(PropKey.SLAVE_SCREEN_SLEEP)
        if err != ErrCode.SUCCESS or len(data) < 4:
            raise DeviceError(f"获取屏幕休眠时间失败 err={err}")
        return bytes_to_u32(data[:4])

    def sync_time(self, timestamp_s: Optional[int] = None) -> int:
        """时间同步：下发"北京时间秒"（= UTC 秒 + 8h）。

        cmd=SET_SLAVE_PROP, payload=[SET_UTC_TIME(0x28)][seconds(4 LE)]。
        实测设备端把收到的值再加 8h 作为北京时间广播（显示会快 8 小时），
        因此工具端预减 8h 抵消：缺省发 UTC+8h 的本地秒，
        设备显示 = 值 + 8h - 8h = 正确当前时间。
        timestamp_s 缺省取当前时间 + 8h。
        """
        if timestamp_s is None:
            timestamp_s = int(time.time() + 8 * 3600)
        ts = int(timestamp_s)
        if ts <= 0 or ts > 0xFFFFFFFF:
            raise DeviceError(f"时间戳超出 u32 范围: {ts}")
        err, _ = self._set_slave_prop(PropKey.SET_UTC_TIME, u32_to_bytes(ts))
        if err != ErrCode.SUCCESS:
            raise DeviceError(f"时间同步失败 err={err}")
        return err

    def set_rate(self, hz: int) -> int:
        """设置回报率（Hz）。

        对齐 AM50 _set_prop 的 MOUSE_FREQUENCY 分支:
          cmd=SET_SLAVE_PROP, payload=[PROP_KEY_POLLING_RATE][hz(4 LE)]
        设备端会做边界约束：BLE 模式 125~500，USB 模式 125~1000。
        """
        err, _ = self._set_slave_prop(PropKey.POLLING_RATE, u32_to_bytes(hz))
        if err != ErrCode.SUCCESS:
            raise DeviceError(f"设置回报率失败 err={err}")
        return err

    # ---- 按键自定义 ----
    # 与 KbDevice.get/set_custom_button 对齐：
    #   GET: GET_SLAVE_PROP [CUSTOM_KEY][key_index] -> ACK(err, [flag][type][data(8B)])
    #   SET: SET_SLAVE_PROP [CUSTOM_KEY][key_index][flag][type][data(8B)])

    def _get_slave_prop_with_payload(self, payload: bytes, timeout: float = 3.0) -> Tuple[int, bytes]:
        """GET_SLAVE_PROP 带多字节 payload（如 CUSTOM_KEY 需要 [key][index]）。"""
        return self.send_and_wait(Command.GET_SLAVE_PROP, payload, timeout)

    def get_custom_button(self, key_index: int) -> Tuple[int, int, bytes]:
        """获取指定按键的自定义配置。

        返回 (flag, key_type, data(8字节))。
        flag=1 标准按键，2 自定义按键；key_type 0=normal，1=consume。
        """
        err, data = self._get_slave_prop_with_payload(
            bytes([PropKey.CUSTOM_KEY, key_index & 0xFF])
        )
        if err != ErrCode.SUCCESS or len(data) < 10:
            raise DeviceError(f"获取 custom_button key={key_index} 失败 err={err}")
        return data[0], data[1], bytes(data[2:10])

    def set_custom_button(self, key_index: int, flag: int, key_type: int, data: bytes) -> int:
        """设置按键自定义。

        data 是 8 字节的协议数据（normal key 模式）或 2 字节（consume key 模式，补齐到8）。
        """
        if len(data) > 8:
            data = data[:8]
        data = data + b"\x00" * (8 - len(data))
        payload = bytes([PropKey.CUSTOM_KEY, key_index & 0xFF, flag & 0xFF, key_type & 0xFF]) + data
        err, _ = self.send_and_wait(Command.SET_SLAVE_PROP, payload)
        if err != ErrCode.SUCCESS:
            raise DeviceError(f"设置 custom_button key={key_index} 失败 err={err}")
        return err
