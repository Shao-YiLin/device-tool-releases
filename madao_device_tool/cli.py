"""madao_device_tool 命令行工具。

支持两种用法：
1. 交互式 REPL（推荐，更实用）：
       python madao_device_tool/cli.py interactive
   进入后支持 scan-usb / scan-ble / connect-usb / connect-ble / connect-dongle /
   list / info / set-cpi / set-rate / set-rgb-mode / set-rgb-brightness /
   auth / disconnect / disconnect-all 等子命令，带 tab 补全。

2. 非交互单命令（每条命令独立进程，内部自动 scan+connect+operate+disconnect）：
       python madao_device_tool/cli.py scan-usb
       python madao_device_tool/cli.py set-cpi 4:9 3
       python madao_device_tool/cli.py set-rgb-mode AA:BB:CC:DD:EE:FF 5

约束：设备池状态不跨进程持久。非交互模式下每条命令重新建立连接再释放。
"""

from __future__ import annotations

import argparse
import cmd
import os
import sys
from typing import List, Optional

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import _bootstrap  # noqa: F401  加载 libusb DLL

from comm_lib.channels import CommError, DeviceInfo
from comm_lib.channels.usb_comm import UsbComm, UsbDeviceInfo
from devtool import DeviceTool
from device_model import BaseDevice, DeviceError, KbDevice, MouseDevice


# 设备类型 -> 友好名
_DEVICE_TYPE_NAME = {0x31: "mouse", 0x32: "keyboard"}


def _device_type_name(dev: BaseDevice) -> str:
    return _DEVICE_TYPE_NAME.get(getattr(dev, "DEVICE_TYPE", None), "device")


def _fmt_mac(b: Optional[bytes]) -> str:
    return b.hex(":") if b else "-"


def _try(msg: str, fn, *args, **kwargs):
    """执行 fn，捕获 DeviceError/CommError/Exception，友好提示。返回 True 表示成功。"""
    try:
        result = fn(*args, **kwargs)
        return True, result
    except DeviceError as e:
        print(f"[错误] {msg}: {e}")
    except CommError as e:
        print(f"[通信错误] {msg}: {e}")
    except Exception as e:
        print(f"[异常] {msg}: {e}")
    return False, None


# ---- 输出辅助 ----
def _print_scan_result(infos: List[DeviceInfo]) -> None:
    if not infos:
        print("(无)")
        return
    for i in infos:
        dtype = _DEVICE_TYPE_NAME.get(i.device_type, "?")
        via = " (dongle)" if getattr(i, "via_dongle", False) else ""
        print(f"  {i.address:<22} {dtype:<10} {i.name}{via}")


def _print_devices(devs: List[BaseDevice]) -> None:
    if not devs:
        print("(无已连接设备)")
        return
    for d in devs:
        kind = type(d).__name__
        print(f"  {d.address:<22} {kind:<13} {d.name}")


def _print_info(dev: BaseDevice) -> None:
    kind = type(dev).__name__
    print(f"address     : {dev.address}")
    print(f"name        : {dev.name}")
    print(f"type        : {kind} ({_device_type_name(dev)})")
    print(f"connected   : {dev.is_connected}")
    ok, _ = _try("读取 product_no", dev.get_product_no)
    if ok:
        print(f"product_no  : {_fmt_mac(dev.product_no)}")
    ok, _ = _try("读取 device_mac", dev.get_device_mac)
    if ok:
        print(f"device_mac  : {_fmt_mac(dev.device_mac)}")
    ok, ver = _try("读取 device_version", dev.get_device_version)
    if ok:
        print(f"version     : {hex(ver) if ver else '-'}")
    ok, batt = _try("读取 battery", dev.get_battery)
    if ok:
        print(f"battery     : {batt} mV")
    if isinstance(dev, MouseDevice):
        ok, cpi = _try("读取 CPI", dev.get_cpi)
        if ok:
            print(f"cpi_level   : {cpi}")
        ok, rate = _try("读取 polling_rate", dev.get_rate)
        if ok:
            print(f"polling_rate: {rate} Hz")
    elif isinstance(dev, KbDevice):
        ok, mode = _try("读取 rgb_mode", dev.get_rgb_mode)
        if ok:
            print(f"rgb_mode    : {mode}")
        ok, br = _try("读取 rgb_brightness", dev.get_rgb_brightness)
        if ok:
            print(f"rgb_brightness: {br}")


# =====================================================================
# 交互式 REPL
# =====================================================================

class _DeviceCli(cmd.Cmd):
    """设备操作 REPL。共享一个 DeviceTool 实例（设备池在本进程内持久）。"""

    intro = "madao device tool - 输入 help 查看命令，exit 退出。"
    prompt = "> "

    def __init__(self) -> None:
        super().__init__()
        self.tool = DeviceTool()

    # ---- 生命周期 ----
    def _close(self) -> None:
        try:
            self.tool.close()
        except Exception:
            pass

    # ---- 解析辅助 ----
    @staticmethod
    def _split(arg: str) -> List[str]:
        return arg.split()

    def _need_dev(self, address: str) -> Optional[BaseDevice]:
        dev = self.tool.get_device(address)
        if dev is None:
            print(f"[错误] 未找到已连接设备: {address}（请先 connect-*）")
        return dev

    # ---- 命令：扫描 ----
    def do_scan_usb(self, arg: str) -> None:
        """scan-usb：扫描直连 USB 设备"""
        ok, infos = _try("扫描 USB", self.tool.scan_usb)
        if ok:
            _print_scan_result(infos)

    def do_scan_ble(self, arg: str) -> None:
        """scan-ble [timeout]：扫描 BLE 设备"""
        timeout = 5.0
        args = self._split(arg)
        if args:
            try:
                timeout = float(args[0])
            except ValueError:
                print("[错误] timeout 必须是数字")
                return
        ok, infos = _try("扫描 BLE", self.tool.scan_ble, timeout)
        if ok:
            _print_scan_result(infos)

    # ---- 命令：连接 ----
    def _connect_usb_by_addr(self, address: str, dongle: bool = False) -> None:
        """通过 bus:addr 连接 USB 直连设备（需要重新扫描匹配到 UsbDeviceInfo）。"""
        infos = self.tool.scan_usb()
        info = next((i for i in infos if i.address == address), None)
        if info is None:
            print(f"[错误] 未在 USB 扫描结果中找到 {address}")
            return
        ok, dev = _try("连接 USB", self.tool.connect, info)
        if ok:
            print(f"已连接: {dev.address} ({type(dev).__name__})")

    def do_connect_usb(self, arg: str) -> None:
        """connect-usb <bus:addr>：连接 USB 直连设备"""
        args = self._split(arg)
        if not args:
            print("用法: connect-usb <bus:addr>")
            return
        self._connect_usb_by_addr(args[0], dongle=False)

    def do_connect_ble(self, arg: str) -> None:
        """connect-ble <mac> [mouse|keyboard]：连接 Windows 已配对的 BLE 设备"""
        args = self._split(arg)
        if not args:
            print("用法: connect-ble <mac> [mouse|keyboard]")
            return
        mac = args[0]
        device_type = 0x31  # 默认鼠标
        if len(args) > 1 and args[1].lower().startswith("key"):
            device_type = 0x32
        ok, dev = _try("连接 BLE", self.tool.connect_ble_bonded, mac, device_type=device_type)
        if ok:
            print(f"已连接: {dev.address} ({type(dev).__name__})")

    def do_connect_dongle(self, arg: str) -> None:
        """connect-dongle <bus:addr>：连接 dongle 并列出已挂载子设备"""
        args = self._split(arg)
        if not args:
            print("用法: connect-dongle <bus:addr>")
            return
        address = args[0]
        infos = self.tool.scan_usb()
        info = next((i for i in infos if i.address == address), None)
        if info is None:
            print(f"[错误] 未找到 USB 设备 {address}")
            return
        ok, _ = _try("连接 dongle", self.tool.connect, info)
        if not ok:
            return
        # 触发 dongle 查询已挂载子设备
        dongle_comm = self.tool.manager._dongle
        if dongle_comm is None:
            print("dongle 未建立")
            return
        collected: List[DeviceInfo] = []
        dongle_comm.on_device_found = lambda i: collected.append(i)
        _try("查询 dongle 子设备", dongle_comm.dongle_query_connected)
        import time
        time.sleep(1.0)  # 等待 dongle 异步上报
        print("已挂载子设备:")
        _print_scan_result(collected)

    # ---- 命令：设备池 ----
    def do_list(self, arg: str) -> None:
        """list：列出已连接设备"""
        _print_devices(self.tool.devices())

    def do_info(self, arg: str) -> None:
        """info <address>：显示设备详细信息"""
        args = self._split(arg)
        if not args:
            print("用法: info <address>")
            return
        dev = self._need_dev(args[0])
        if dev is None:
            return
        _print_info(dev)

    # ---- 命令：鼠标操作 ----
    def do_set_cpi(self, arg: str) -> None:
        """set-cpi <address> <level>：设置鼠标 CPI 等级"""
        args = self._split(arg)
        if len(args) < 2:
            print("用法: set-cpi <address> <level>")
            return
        dev = self._need_dev(args[0])
        if dev is None:
            return
        if not isinstance(dev, MouseDevice):
            print(f"[错误] {args[0]} 是 {_device_type_name(dev)}，不支持 CPI")
            return
        try:
            level = int(args[1])
        except ValueError:
            print("[错误] level 必须是整数")
            return
        ok, _ = _try("设置 CPI", dev.set_cpi, level)
        if ok:
            print(f"CPI 已设置为 {level}")

    def do_set_rate(self, arg: str) -> None:
        """set-rate <address> <hz>：设置鼠标回报率（Hz）"""
        args = self._split(arg)
        if len(args) < 2:
            print("用法: set-rate <address> <hz>")
            return
        dev = self._need_dev(args[0])
        if dev is None:
            return
        if not isinstance(dev, MouseDevice):
            print(f"[错误] {args[0]} 是 {_device_type_name(dev)}，不支持回报率")
            return
        try:
            hz = int(args[1])
        except ValueError:
            print("[错误] hz 必须是整数")
            return
        ok, _ = _try("设置回报率", dev.set_rate, hz)
        if ok:
            print(f"回报率已设置为 {hz} Hz")

    # ---- 命令：键盘操作 ----
    def do_set_rgb_mode(self, arg: str) -> None:
        """set-rgb-mode <address> <mode>：设置键盘 RGB 灯效模式"""
        args = self._split(arg)
        if len(args) < 2:
            print("用法: set-rgb-mode <address> <mode>")
            return
        dev = self._need_dev(args[0])
        if dev is None:
            return
        if not isinstance(dev, KbDevice):
            print(f"[错误] {args[0]} 是 {_device_type_name(dev)}，不支持 RGB")
            return
        try:
            mode = int(args[1])
        except ValueError:
            print("[错误] mode 必须是整数")
            return
        ok, _ = _try("设置 RGB 模式", dev.set_rgb_mode, mode)
        if ok:
            print(f"RGB 模式已设置为 {mode}")

    def do_set_rgb_brightness(self, arg: str) -> None:
        """set-rgb-brightness <address> <level>：设置键盘 RGB 亮度（0~3）"""
        args = self._split(arg)
        if len(args) < 2:
            print("用法: set-rgb-brightness <address> <level>")
            return
        dev = self._need_dev(args[0])
        if dev is None:
            return
        if not isinstance(dev, KbDevice):
            print(f"[错误] {args[0]} 是 {_device_type_name(dev)}，不支持 RGB")
            return
        try:
            level = int(args[1])
        except ValueError:
            print("[错误] level 必须是整数")
            return
        ok, _ = _try("设置 RGB 亮度", dev.set_rgb_brightness, level)
        if ok:
            print(f"RGB 亮度已设置为 {level}")

    # ---- 命令：鉴权 ----
    def do_auth(self, arg: str) -> None:
        """auth <address>：执行鉴权握手"""
        args = self._split(arg)
        if not args:
            print("用法: auth <address>")
            return
        dev = self._need_dev(args[0])
        if dev is None:
            return
        ok, result = _try("鉴权", dev.authenticate)
        if ok:
            print(f"鉴权结果: {'成功' if result else '失败'}")

    # ---- 命令：断开 ----
    def do_disconnect(self, arg: str) -> None:
        """disconnect <address>：断开指定设备"""
        args = self._split(arg)
        if not args:
            print("用法: disconnect <address>")
            return
        if self.tool.disconnect(args[0]):
            print(f"已断开: {args[0]}")
        else:
            print(f"[错误] 未找到已连接设备: {args[0]}")

    def do_disconnect_all(self, arg: str) -> None:
        """disconnect-all：断开所有设备"""
        self.tool.disconnect_all()
        print("已断开所有设备")

    # ---- 退出 / 帮助 ----
    def do_exit(self, arg: str) -> bool:
        """exit：退出"""
        self._close()
        print("bye")
        return True

    do_quit = do_exit
    do_EOF = do_exit

    # ---- 把下划线命令名映射成连字符（用户友好）----
    # cmd 模块用 do_<name> 派发，name 中不能含 '-'，所以用户输入连字符时，
    # 重写 precmd 把 scan-usb -> scan_usb。
    def precmd(self, line: str) -> str:
        # 仅替换已知命令前缀中的连字符
        aliases = {
            "scan-usb": "scan_usb",
            "scan-ble": "scan_ble",
            "connect-usb": "connect_usb",
            "connect-ble": "connect_ble",
            "connect-dongle": "connect_dongle",
            "disconnect-all": "disconnect_all",
            "set-cpi": "set_cpi",
            "set-rate": "set_rate",
            "set-rgb-mode": "set_rgb_mode",
            "set-rgb-brightness": "set_rgb_brightness",
            "disconnect-all": "disconnect_all",
        }
        stripped = line.lstrip()
        if not stripped:
            return line
        cmd_name = stripped.split(None, 1)[0]
        if cmd_name in aliases:
            # 替换第一个 token
            idx = line.index(cmd_name)
            return line[:idx] + aliases[cmd_name] + line[idx + len(cmd_name):]
        return line


# =====================================================================
# 非交互式单命令（argparse 子命令）
# =====================================================================

def _resolve_usb_info(tool: DeviceTool, address: str) -> Optional[UsbDeviceInfo]:
    """扫描 USB 找到 address 对应的 DeviceInfo。"""
    infos = tool.scan_usb()
    return next((i for i in infos if i.address == address), None)


def _run_single(args: argparse.Namespace) -> int:
    """单命令模式：每条命令独立建立 DeviceTool，结束时 close。"""
    tool = DeviceTool()
    rc = 0
    try:
        name = args.command
        if name == "scan-usb":
            ok, infos = _try("扫描 USB", tool.scan_usb)
            if ok:
                _print_scan_result(infos)
            rc = 0 if ok else 1

        elif name == "scan-ble":
            ok, infos = _try("扫描 BLE", tool.scan_ble, args.timeout)
            if ok:
                _print_scan_result(infos)
            rc = 0 if ok else 1

        elif name == "connect-usb":
            info = _resolve_usb_info(tool, args.address)
            if info is None:
                print(f"[错误] 未找到 USB 设备 {args.address}")
                return 1
            ok, dev = _try("连接 USB", tool.connect, info)
            if ok:
                print(f"已连接: {dev.address} ({type(dev).__name__})")
                _print_info(dev)
            rc = 0 if ok else 1

        elif name == "connect-ble":
            ok, dev = _try("连接 BLE", tool.connect_ble_bonded, args.mac,
                           device_type=args.type)
            if ok:
                print(f"已连接: {dev.address} ({type(dev).__name__})")
                _print_info(dev)
            rc = 0 if ok else 1

        elif name == "connect-dongle":
            info = _resolve_usb_info(tool, args.address)
            if info is None:
                print(f"[错误] 未找到 USB 设备 {args.address}")
                return 1
            ok, _ = _try("连接 dongle", tool.connect, info)
            if not ok:
                return 1
            dongle_comm = tool.manager._dongle
            collected: List[DeviceInfo] = []
            if dongle_comm is not None:
                dongle_comm.on_device_found = lambda i: collected.append(i)
                _try("查询 dongle 子设备", dongle_comm.dongle_query_connected)
                import time
                time.sleep(1.0)
            print("已挂载子设备:")
            _print_scan_result(collected)

        elif name == "list":
            _print_devices(tool.devices())

        elif name == "info":
            # 非交互模式下 info 要先连接
            info = _resolve_usb_info(tool, args.address)
            if info is not None:
                ok, dev = _try("连接", tool.connect, info)
            else:
                # 当作 BLE mac 处理（已配对）
                dtype = 0x32 if args.ble_type.lower().startswith("key") else 0x31
                ok, dev = _try("连接 BLE", tool.connect_ble_bonded, args.address,
                               device_type=dtype)
            if ok and dev is not None:
                _print_info(dev)
            rc = 0 if ok else 1

        elif name in ("set-cpi", "set-rate", "set-rgb-mode",
                      "set-rgb-brightness", "auth"):
            # 这些操作命令内部自动连接再断开
            rc = _run_op_command(tool, args)
    finally:
        tool.close()
    return rc


def _run_op_command(tool: DeviceTool, args: argparse.Namespace) -> int:
    """操作类命令：自动 USB/BLE 连接 -> 操作 -> 自动断开。"""
    address = args.address
    # 1. 尝试 USB（bus:addr 格式）
    dev: Optional[BaseDevice] = None
    info = _resolve_usb_info(tool, address)
    if info is not None:
        ok, dev = _try("连接", tool.connect, info)
        if not ok:
            return 1
    else:
        # 当作 BLE 已配对 mac，默认鼠标
        dtype = 0x32 if getattr(args, "kb", False) else 0x31
        ok, dev = _try("连接 BLE", tool.connect_ble_bonded, address, device_type=dtype)
        if not ok:
            return 1

    name = args.command
    rc = 0
    if name == "set-cpi":
        if not isinstance(dev, MouseDevice):
            print(f"[错误] {_device_type_name(dev)} 不支持 CPI")
            return 1
        ok, _ = _try("设置 CPI", dev.set_cpi, args.level)
        if ok:
            print(f"CPI 已设置为 {args.level}")
        rc = 0 if ok else 1

    elif name == "set-rate":
        if not isinstance(dev, MouseDevice):
            print(f"[错误] {_device_type_name(dev)} 不支持回报率")
            return 1
        ok, _ = _try("设置回报率", dev.set_rate, args.hz)
        if ok:
            print(f"回报率已设置为 {args.hz} Hz")
        rc = 0 if ok else 1

    elif name == "set-rgb-mode":
        if not isinstance(dev, KbDevice):
            print(f"[错误] {_device_type_name(dev)} 不支持 RGB")
            return 1
        ok, _ = _try("设置 RGB 模式", dev.set_rgb_mode, args.mode)
        if ok:
            print(f"RGB 模式已设置为 {args.mode}")
        rc = 0 if ok else 1

    elif name == "set-rgb-brightness":
        if not isinstance(dev, KbDevice):
            print(f"[错误] {_device_type_name(dev)} 不支持 RGB")
            return 1
        ok, _ = _try("设置 RGB 亮度", dev.set_rgb_brightness, args.level)
        if ok:
            print(f"RGB 亮度已设置为 {args.level}")
        rc = 0 if ok else 1

    elif name == "auth":
        ok, result = _try("鉴权", dev.authenticate)
        if ok:
            print(f"鉴权结果: {'成功' if result else '失败'}")
        rc = 0 if ok else 1

    tool.disconnect(dev.address)
    return rc


def _build_arg_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="cli.py",
        description="madao device tool CLI",
    )
    sub = p.add_subparsers(dest="command", required=True)

    sub.add_parser("scan-usb", help="扫描 USB 设备")
    sb = sub.add_parser("scan-ble", help="扫描 BLE 设备")
    sb.add_argument("--timeout", type=float, default=5.0, help="扫描超时（秒）")

    cu = sub.add_parser("connect-usb", help="连接 USB 直连设备")
    cu.add_argument("address", help="bus:addr，如 4:9")
    cb = sub.add_parser("connect-ble", help="连接 BLE 已配对设备")
    cb.add_argument("mac", help="设备 mac")
    cb.add_argument("--type", type=lambda s: int(s, 0), default=0x31,
                    help="设备类型 0x31=mouse 0x32=keyboard，默认 0x31")

    cd = sub.add_parser("connect-dongle", help="连接 dongle 并列出子设备")
    cd.add_argument("address", help="bus:addr")

    sub.add_parser("list", help="列出已连接设备（单命令模式下始终为空）")

    info = sub.add_parser("info", help="显示设备详细信息")
    info.add_argument("address", help="bus:addr 或 BLE mac")
    info.add_argument("--ble-type", default="mouse", help="mouse/keyboard")

    sc = sub.add_parser("set-cpi", help="设置鼠标 CPI")
    sc.add_argument("address")
    sc.add_argument("level", type=int)
    sc.add_argument("--kb", action="store_true", help="连接 BLE 时强制按键盘")

    sr = sub.add_parser("set-rate", help="设置鼠标回报率")
    sr.add_argument("address")
    sr.add_argument("hz", type=int)
    sr.add_argument("--kb", action="store_true")

    sm = sub.add_parser("set-rgb-mode", help="设置键盘 RGB 模式")
    sm.add_argument("address")
    sm.add_argument("mode", type=int)
    sm.add_argument("--kb", action="store_true")

    sb2 = sub.add_parser("set-rgb-brightness", help="设置键盘 RGB 亮度")
    sb2.add_argument("address")
    sb2.add_argument("level", type=int)
    sb2.add_argument("--kb", action="store_true")

    au = sub.add_parser("auth", help="执行鉴权")
    au.add_argument("address")
    au.add_argument("--kb", action="store_true")

    return p


def main(argv: Optional[List[str]] = None) -> int:
    argv = argv if argv is not None else sys.argv[1:]
    # 单独的 interactive 子命令
    if argv and argv[0] == "interactive":
        cli = _DeviceCli()
        try:
            cli.cmdloop()
        except KeyboardInterrupt:
            print()
            cli._close()
            print("bye")
        return 0

    parser = _build_arg_parser()
    args = parser.parse_args(argv)
    return _run_single(args)


if __name__ == "__main__":
    sys.exit(main())
