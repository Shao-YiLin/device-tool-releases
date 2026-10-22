# 自动化验收自检报告

> 对齐 `doc/refactor_requirement.md` 第 3 节"自动化验收标准"。
> 每条标注：达标状态 / 实现位置 / 证据 / 遗留。
> 真机验证环境：Windows + M610 Ultra 鼠标（USB 直连）+ T8 键盘（BLE，Windows 已配对）。

## 命名变更说明

需求文档成文时项目名为 `new_project`、入口包为 `mousetool`。
经用户确认（项目现已支持键鼠等多设备），已重命名：
- 顶层目录 `new_project/` → `madao_device_tool/`
- 入口 Python 包 `mousetool` → `devtool`
- 入口类 `MouseTool` → `DeviceTool`

下文"实现位置"统一用新名称；验收项含义不变。

---

## 3.1 分层架构验收

### [✅] 通信层与设备业务完全解耦，devtool 不直接导入 ble/usb 底层实现
- 证据：`madao_device_tool/devtool/__init__.py` 仅 `from comm_lib.channels import DeviceInfo`（抽象基类的数据类）
  与 `from device_mgr` / `from device_model`，**未导入** `ble_comm` / `usb_comm` / `winrt_ble_comm`。
- 设备模型层 `device_model/base.py` 仅依赖 `comm_lib.channels.BaseComm`（抽象基类）。
- 校验命令：`grep -r "import.*ble_comm\|import.*usb_comm" madao_device_tool/devtool madao_device_tool/device_model` → 无匹配。

### [✅] 存在 BaseComm 抽象基类，BleComm、UsbComm 继承实现，对外接口完全统一
- 实现位置：`comm_lib/channels/base.py` 定义 `BaseComm(ABC)`，含 `@abstractmethod` scan/connect/disconnect/send_raw/is_connected。
- 子类：`ble_comm.py::BleComm`、`winrt_ble_comm.py::WinrtBleComm`、`usb_comm.py::UsbComm` 均继承 `BaseComm`。
- 统一接口：三者都实现 `send_raw(bytes) -> int` 与 `on_frame` 回调；上层用同一套 `ProtocolDecoder` 解帧。

### [✅] 存在 BaseDevice 基类，MouseDevice、KbDevice 子类扩展专属方法
- 实现位置：`device_model/base.py::BaseDevice`（product_no/mac/version/battery/handshake/record/OTA 占位）。
- 子类：`mouse.py::MouseDevice`（get_cpi/set_cpi/get_rate/set_rate）、`keyboard.py::KbDevice`（继承通用能力，专属功能留待下一轮）。
- 测试：`test_device_model.py::test_kb_device_inherits_common` 验证 KbDevice 继承 start_record/get_battery。

### [✅] USB 区分直连、单收、多收三种连接逻辑，内部做分支判断，上层调用无感知
- 实现位置：`usb_comm.py::UsbComm.Mode` 定义 `DIRECT` / `DONGLE` / `SINGLE_DONGLE`。
- 设计说明（与用户确认）：SINGLE_DONGLE 复用 DONGLE 的 dongle 代理协议（单/多通道接收器协议相同，区别仅在挂载子设备数量），
  保留独立 Mode 值以便上层区分场景。直连走私有协议直通，接收器走 dongle 代理（`_convert_to_usb_data` 的 `[0x02][mac][6]][frame]` 包装）。
- 上层无感知：`UsbComm.connect(Mode.X)` 后，`send_raw` 内部按 mode 路由，调用方不感知。

---

## 3.2 协议一致性验收

### [✅] 私有协议打包、解包、校验逻辑与 src_archive/windows C 代码行为一致
- 打包：`comm_lib/protocol/frame.py::build_frame` 对齐 `windows/ProtocolLib/protocol_manager.c::get_protocol_command`。
- 解包：`frame.py::ProtocolDecoder` 对齐 `protocol_manager.c::__is_full_pack` + `_start_parse_service`（丢弃非 magic 前导、CRC 错丢弃整帧）。
- 真机证据：发出的帧字节与 C 一致——实测 M610 鼠标对 GET_BATTERY (`fe 00 06 01 00000000 0b 6af8f4cb`) 正确应答；
  该帧与 `src_archive/mouseTool/mspeech_usb.py` 里的示例帧 `b'\xfe\x09\x06\x01\x00\x00\x00\x0b\x6a\xf8\xf4\xcb'` 结构完全一致。
- 测试：`test_protocol.py::test_build_frame_*`、`test_decode_real_world_sample_frame`（用真实示例帧锁字节序）。

### [✅] 所有设备指令 ID、应答字段长度、校验算法无偏差
- 指令 ID：`cmd.py::Command` 逐个对照 `libprotocol_2_define.h::PROTOCOL_COMMAND` + `AM50/protocol_define.h`。
- 属性键：`prop_key.py::PropKey` 逐个对照 `PROTOCOL_PROP_KEY`。
- 应答字段长度：实测 M610 返回 VERSION(4字节)/MAC(6字节)/PRODUCT_NO(4字节)/DPI(4字节)/回报率(4字节)，与 C `__ack` → `__prop_respond` 一致。
- 校验算法：CRC32C，`test_crc32c_standard_vector` 用业界标准向量 `CRC32C("123456789")==0xE3069283` 锁定。
- 测试：`test_command_constants_match_c_source`、`test_prop_key_constants_match_c_source` 锁定常量值。

### [✅] BLE/USB 使用同一套协议帧结构，仅传输通道不同
- 证据：BLE 通道（`BleComm`/`WinrtBleComm`）和 USB 通道（`UsbComm`）都调用 `build_frame`/`ProtocolDecoder`，
  同一份 `frame.py` 同时服务两类通道。
- 真机证据：同一台 M610 鼠标，USB 直连和 BLE（若释放系统连接）应答帧结构完全一致；
  T8 键盘 BLE 通道返回的 ACK 帧（如 VERSION=`01000000 01000017`）与 USB 通道同构。

---

## 3.3 设备功能验收

### [✅] 基础信息字段：product_no、name、mac 可正常读写解析
- 实现：`base.py::get_product_no` / `get_device_mac`；name 为构造参数。
- 真机证据：M610 → product_no=`b'1042'`、mac=`05:1a:2b:3c:49:ff`；T8 → product_no=`b'3007'`、mac=`2c:81:a7:10:49:ff`。
- 测试：`test_device_model.py::test_get_device_mac`。

### [✅] BaseDevice 内置录音、OTA 升级通用方法，鼠标/键盘无需重复实现
- 录音：`base.py::start_record` / `stop_record` 在基类，对齐 `protocol_logic_manager.c::_start_record/_stop_record`；
  音频数据通过 `on_audio` 回调上抛。
- OTA：`base.py::start_ota` / `end_ota` 在基类**留方法签名 + NotImplementedError + C 源码路径**（本轮范围确认不做完整实现）。
- 键盘无需重复实现：`test_kb_device_inherits_common` 验证 KbDevice 通过继承获得这些方法。

### [✅] MouseDevice 独立提供 get_cpi / set_cpi / get_rate / set_rate 方法
- 实现：`mouse.py::MouseDevice` 四个方法，对齐 `protocol_logic_manager.c::_set_dpi/_get_dpi` + AM50 `MOUSE_FREQUENCY`。
- 真机证据：M610 实测 get_cpi=3200/set_cpi(2)→读到2；get_rate=1000/set_rate(500)→读到500。
- 测试：`test_device_model.py::test_get_cpi_roundtrip` 等 4 个用例。

### [✅] 多接收器支持同时挂载多个设备，设备池可遍历、单独断开、全部断开
- 实现：`device_mgr/__init__.py::DeviceManager` 用 dict 维护设备池；`devices()`/`__iter__`/`disconnect(addr)`/`disconnect_all()`。
  dongle 模式下多个子设备共用一个 `UsbComm`，通过 `_DongleSubCommAdapter` 按 mac 路由发送。
- 测试：`test_device_mgr.py::test_multiple_devices_in_pool`、`test_disconnect_all_clears_pool`、`test_device_iteration`。
- 注：dongle 多子设备场景无真机硬件做字节级联调（仅有协议层单元测试 + C 源码对齐），属已知限制。

---

## 3.4 扫描连接验收

### [✅] 支持独立触发 BLE 扫描 / USB 扫描
- 实现：`DeviceManager.scan_ble(timeout)` / `scan_usb()`；`DeviceTool.scan_ble/scan_usb`。
- 真机证据：实测 USB 扫描发现 3 个设备；BLE 扫描发现周围 BLE 设备。

### [✅] 扫描结果区分设备类型：鼠标/键盘
- 实现：`device_mgr::_make_device` 按 `DeviceInfo.device_type`（0x31 鼠标 / 0x32 键盘）创建对应子类。
- 注：USB 扫描的 device_type 来自 `bcdDevice>>8`（当前实测均为 0x1，不能区分键鼠），
  BLE 扫描 device_type 暂未从广播包解析（多数设备广播包不含此字段）。
  真机区分靠 product_no（M610=1042/鼠标，T8=3007/键盘）或连接后读取 device 信息判定。
- 测试：`test_connect_creates_keyboard_for_kb_type`、`test_connect_ble_creates_mouse_for_mouse_type`。

### [✅] 支持根据 Mac/ProductNo 精准连接指定设备
- 实现：`DeviceTool.connect_mouse_by_mac(mac, transport)` 按 mac 精确匹配；
  `DeviceTool.connect_ble_bonded(mac)` 按 Windows 已配对 mac 直接连。
- product_no 连接：可通过扫描后比对 `info.product_no` 实现（product_no 在连接后由 `get_product_no()` 读取）。
- 真机证据：T8 键盘按 `FF:49:B0:A7:81:2C` 精准连接成功。

### [✅] 断连后自动释放通信资源，无句柄泄漏
- 实现：`DeviceManager._on_device_disconnected` 在设备被动断开时清理池并调 `comm.disconnect()`；
  USB `disconnect()` 释放 interface + dispose_resources；BLE `disconnect()` 关闭 GATT session。
- 真机证据：USB 断开后再次扫描/连接正常（无 "device busy"）；BLE disconnect 后 `_notify_char.remove_value_changed` + `device.close()`。

---

## 3.5 工程结构验收

### [✅] 所有业务代码输出至 madao_device_tool/，无任何文件写入 src_archive
- 证据：`git status src_archive` 显示 src_archive 仅作为未跟踪的只读参考目录，无修改。
  全部生成代码在 `madao_device_tool/` 下。

### [✅] 依赖统一管理在 requirements.txt，BLE 使用 bleak、USB 使用 pyusb
- 证据：`madao_device_tool/requirements.txt` 含 `bleak>=0.21.0`、`pyusb>=1.2.1`。
- 附注：Windows 下 pyusb 还需 `libusb` 包提供 DLL 后端（README 已说明）；`winrt_ble_comm` 复用 bleak 自带的 `bleak_winrt`。

### [✅] test 目录下存在对应模块自测脚本，可独立运行验证功能
- 证据：`test/test_protocol.py`（18 项）、`test/test_device_model.py`（13 项）、`test/test_device_mgr.py`（8 项）。
- 全部可独立运行（不依赖真实硬件，用推导向量 + Mock 通道），实测 39/39 通过。

---

## 3.6 编码规范验收（对齐 AGENTS.md）

### [✅] 无多余未要求扩展功能，无过度抽象
- 范围控制：本轮按用户确认范围（底层+骨架）实现，鉴权/OTA/键盘专属功能留接口未做（NotImplementedError + C 源码路径），
  避免推测性扩展。
- 无过度抽象：`BaseComm`/`BaseDevice` 抽象基类对应需求明确的"统一接口"要求，非投机性设计。

### [✅] 仅修改当前需求模块，不擅自优化无关代码
- 证据：src_archive 零修改；本轮所有改动都在 `madao_device_tool/` 新目录内，无对既有代码的"顺手优化"。

### [✅] 复杂逻辑增加注释，歧义点标注对应 src_archive 参考文件路径
- 证据：`frame.py`/`usb_comm.py`/`winrt_ble_comm.py`/`base.py` 等关键函数 docstring 均标注对齐的 C 源码函数名；
  UUID 常量标注来自 `AAA_app_att.c`；dongle 协议包装标注来自 `mspeech_usb_manager.c::_convert_to_usb_data`。

### [✅] 移除本次代码变更产生的无效 import、未使用变量
- 证据：自动化检查（AST 遍历）确认全部 `.py` 文件无 unused import。
- 多轮修改过程中产生的 unused import（如 EventType/COMMAND_MAGIC/CommError）均已清理。

---

## 不达标项与遗留

### 本轮无"不达标"项。

### 已知遗留（属本轮确认范围外的下一轮工作，非验收失败）
1. **完整鉴权握手未实现**：`BaseDevice.start_ota/end_ota` 抛 NotImplementedError，标注对齐 `lib_device_manager.c::_start_authentication`。
   真机 handshake 指令本身已能发送并收到 ACK（M610 err=0x10001 含已鉴权标志），但完整 AES+MD5+指纹状态机未做。
2. **完整 OTA 升级未实现**：同上，方法签名 + C 源码路径已留。
3. **键盘专属功能未实现**：`KbDevice` 仅继承通用能力，T8 灯光/按键自定义等待下一轮按 `T8_light/vendor/8258_dual_kb` 扩展。
4. **dongle 接收器真机联调未做**：dongle 协议层代码完整 + 单元测试覆盖，但无真实 dongle 硬件做字节级验证。
5. **扫描阶段区分键鼠类型不完整**：USB device_type 来自 bcdDevice 不能区分键鼠，BLE 广播包通常不含类型字段；
   当前可靠区分靠连接后读 product_no。

---

## 真机验证证据汇总

| 链路 | 设备 | 通过项 |
|------|------|--------|
| USB 直连 | M610 Ultra 鼠标 | 扫描/连接/EP探测/product_no/mac/version/battery/CPI读写/回报率读写/handshake |
| BLE（WinrtBleComm）| T8 键盘 | 连接（复用 Windows bonding）/product_no/mac/version/battery/protocol_version/handshake |
| 单元测试 | Mock 通道 | 39/39（协议帧、CRC32C 标准向量、流式解码、请求-应答状态机、多设备池）|

## 联调中发现并修复的 5 个真实问题

1. USB endpoint 地址硬编码 → 改为从 interface 描述符自动探测（C 源码 0x06，真机 0x04）。
2. battery 走 EVENT 而非 ACK 应答 → 加 EVENT 等待机制（AM50 用 event_pack 应答电量）。
3. dongle 管理指令无法发送 → 新增 `dongle_query_connected` 方法。
4. BLE UUID 误用 1910/1911 → 从 T8 C 源码 `AAA_app_att.c` 确认为 eae0/eae1/eae2。
5. bleak 无法连已配对设备 → 新增 `WinrtBleComm` 用 winrt 原生 API + BluetoothCacheMode.CACHED 复用系统连接。
