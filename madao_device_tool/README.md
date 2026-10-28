# madao_device_tool

设备私有通信库 + 设备管理器（Python 纯实现）。

由 src_archive 下的 C 工程（windows / AM50 / T8_light / mouseTool）和《马道设备通信协议.docx》重构而来，
协议帧、校验、指令 ID 与 C 源码 1:1 对齐。支持鼠标（MouseDevice）和键盘（KbDevice）。

## 目录结构

```
madao_device_tool/
├── comm_lib/              通信底层
│   ├── protocol/          私有协议打包/解包/校验（与硬件通道解耦）
│   │   ├── frame.py       帧结构 + CRC32C + 流式解码器
│   │   ├── cmd.py         命令码常量（对齐 libprotocol_2_define.h / protocol_define.h）
│   │   └── prop_key.py    属性键常量（对齐 PROTOCOL_PROP_KEY）
│   ├── crypto/            加密算法（AES-128-ECB / MD5 / sdbm hash）
│   ├── firmware_crypto.py 固件 OTA 加密工具
│   └── channels/          硬件通道
│       ├── base.py        BaseComm 抽象基类 + DeviceInfo + 自动重连机制
│       ├── ble_comm.py    BLE 通道（bleak，需设备广播）
│       ├── winrt_ble_comm.py  BLE 通道（WinRT 原生，复用系统 bonding）
│       └── usb_comm.py    USB 通道（pyusb，支持直连 + 多通道接收器）
├── device_model/          设备模型
│   ├── base.py            BaseDevice（基础信息、录音、OTA、鉴权）
│   ├── auth.py            鉴权握手协议（4阶段加密交换）
│   ├── ota.py             OTA 升级流程编排
│   ├── mouse.py           MouseDevice（CPI、回报率）
│   └── keyboard.py        KbDevice（RGB灯光、按键自定义、滚轮功能）
├── device_mgr/            设备管理器（扫描/连接/多设备池/智能识别）
├── devtool/               上层业务入口（DeviceTool）
├── device_config.py       设备配置库（product_no → 名称/类型/能力集）
├── gui/                   PySide6 GUI（设备列表 + 鼠标面板 + 键盘面板）
│   ├── main.py            GUI 入口
│   ├── main_window.py     主窗口
│   ├── device_worker.py   后台设备操作线程
│   ├── mouse_panel.py     鼠标设置面板
│   └── keyboard_panel.py  键盘设置面板
├── test/                  单元测试（可独立运行）
│   ├── test_protocol.py   协议帧/CRC32C（18项）
│   ├── test_crypto.py     加密算法标准向量（14项）
│   ├── test_auth.py       鉴权握手流程（5项）
│   ├── test_ota.py        OTA 加密/分片/流程（5项）
│   ├── test_device_model.py  设备模型请求-应答（18项）
│   ├── test_device_mgr.py    设备管理器（8项）
│   ├── test_keyboard.py      键盘专属功能（10项）
│   ├── test_device_config.py 设备配置识别（10项）
│   ├── test_ota_multi_device.py 多设备并发 OTA（7项）
│   ├── test_slave_version.py 从机版本号接口（10项）
│   ├── test_record_test.py   录音压测引擎（10项）
│   └── test_device_worker_pool.py 并行 worker 池桶路由（4项）
│   └── test_passive_disconnect.py 设备被动断开自动刷新（12项）
├── cli.py                 命令行工具（交互式 REPL + 非交互模式）
├── exceptions.py          统一异常体系
├── logging_config.py      日志配置
├── demo_connect_mouse.py  真机联调 demo
└── requirements.txt
```

## 协议帧格式

```
[0]    magic    = 0xFE
[1]    sequence = u8（发送端递增 mod 256）
[2]    cmd      = u8
[3..7] length   = u32 小端（payload 字节数）
[7..]  payload
[7+L]  sign     = u32 小端（CRC32C(payload)，payload 为空时不附加）
```

CRC32C：Castagnoli 多项式（0x1EDC6F41 reflected = 0x82F63B78），INIT/XOROUT=0xFFFFFFFF。
标准向量校验：`CRC32C("123456789") == 0xE3069283`。

## 安装

```bash
pip install -r madao_device_tool/requirements.txt
```

USB 在 Windows 下还需要用 Zadig 把设备 interface 1 的驱动替换成 WinUSB/libusb。

## 运行测试

```bash
# 全部测试（不依赖真实硬件，用推导向量 + Mock 通道）
python madao_device_tool/test/test_protocol.py
python madao_device_tool/test/test_crypto.py
python madao_device_tool/test/test_auth.py
python madao_device_tool/test/test_ota.py
python madao_device_tool/test/test_device_model.py
python madao_device_tool/test/test_device_mgr.py
python madao_device_tool/test/test_keyboard.py
python madao_device_tool/test/test_device_config.py
python madao_device_tool/test/test_dongle_sub_sync.py
python madao_device_tool/test/test_ota_multi_device.py
```

## 使用方式

### GUI（需要 Python 3.11 64位 + PySide6）
```bash
py -3.11 madao_device_tool/gui/main.py
```
启动后：工具栏扫描 USB/BLE → 双击设备连接 → 右侧面板操作 CPI/灯光/鉴权。

### CLI（交互式 REPL）
```bash
python madao_device_tool/cli.py
# 进入交互模式：
> scan-usb
> connect-usb 4:9
> info 4:9
> set-cpi 4:9 3
> disconnect 4:9
> exit
```

### 非交互模式（单条命令）
```bash
python madao_device_tool/cli.py scan-usb
python madao_device_tool/cli.py set-cpi 4:9 3
```

## 真机联调

```bash
python madao_device_tool/demo_connect_mouse.py
```

### 已验证（USB 直连，AM60 鼠标）

实测通过：扫描、连接（自动探测 EP_OUT=0x04）、product_no、mac、version、battery、
CPI（get/set）、回报率（get/set）、handshake。详见 `demo_connect_mouse.py` 输出。

### 已验证（BLE，T8 键盘，WinrtBleComm）

实测通过：连接 Windows 已配对的 T8（`FF:49:B0:A7:81:2C`）、product_no=`3007`、
mac=`2C:81:A7:10:49:FF`、version、battery、protocol_version、handshake。

T8 键盘走 BLE 私有协议（GATT 服务 `0xEAE0`，write=`0xEAE1`，notify=`0xEAE2`），
帧结构与 USB 完全一致（同一套 0xFE 协议 + CRC32C）。

**两套 BLE 通道实现**：
- `BleComm`（基于 bleak）：需要设备主动广播、自己建立连接。适合未配对设备的发现/配对场景。
- `WinrtBleComm`（基于 bleak_winrt 原生 API）：复用 Windows 系统已建立的 bonding 连接，
  用 `BluetoothCacheMode.CACHED` 拿 services，**不需要设备重新广播**。适合设备已在使用的场景
  （如 T8 键盘正作为系统键盘工作）。用法：
  ```python
  from devtool import DeviceTool
  tool = DeviceTool()
  kb = tool.connect_ble_bonded('FF:49:B0:A7:81:2C', device_type=0x32)  # 0x32=keyboard
  ```

### Windows USB 驱动准备

pyusb 需要 libusb 后端。Windows 下：
1. `pip install pyusb libusb`
2. 把 `libusb/_platform/windows/x86/libusb-1.0.dll`（32位Python）或 x64 复制到 Python 安装目录
3. 用 Zadig 把鼠标 interface 1 的驱动替换成 WinUSB（会临时失去系统鼠标功能，调试完可还原）

## 功能实现

已实现（本轮 + 上一轮）：
- 通信底层（协议打包/解包/校验 + BLE/USB 通道 + USB 直连/dongle 两种模式）
- 设备模型基类 + 鼠标子类（CPI、回报率）+ 键盘子类（RGB灯光、按键自定义、滚轮功能）
- 设备管理器（扫描/连接/断开/多设备池）
- 业务入口 DeviceTool
- 基础信息读写（product_no、mac、version、battery）
- 录音指令发送（音频数据通过 on_audio 回调上抛）
- **鉴权握手完整实现**（AES-128-ECB + MD5 + sdbm hash + 4 阶段加密交换）
- **OTA 升级完整实现**（固件 AES-128-ECB 加密 + 32 字节分片 + CRC 校验 + 进度回调）
- **多设备并发 OTA 升级**（非模态升级窗口，多台设备各自独立进度/日志，后台并发执行）
- **设备被动断开自动刷新**（USB 拔出 / BLE 掉线时设备列表自动移除，无需手动刷新）
- **从机版本号读取**（AM60 鼠标 / AT80 键盘，GET_SLAVE_PROP[0x1F]）
- **AM60 未同步文件数 获取/设置**（GET/SET_SLAVE_PROP[0x29]，设置用于同步流程调试）
- **快传 WiFi 凭据**（打开快传后按 C 端同款算法生成热点名/密码展示，方便连接做传输测试）

## 性能优化（多设备场景）

- **并行 worker 池**（device_worker.py::DeviceWorkerPool）：操作按桶并行——
  连接/断开全局串行（一个连接慢不阻塞已连设备）、扫描独立、每设备读写各一桶；
  多联接收器子设备共享通道归 `__dongle__` 桶串行
- **空载接收器探针 2s**：单联接收器连接时的 product_no 探针超时 5s→2s
- **BLE 自动连接过滤**：扫描后只自动连接本厂设备名（键盘关键字/MD/AM 系），
  不再逐个尝试耳机等无关已配对设备；手动双击连接不受限
- **dongle 连接走 worker**：多联接收器通道建立在后台线程，不卡 UI
- **面板刷新分级**：product_no/电池/CPI/回报率立即读，AM60 次要设置项
  （触感/从机版本/屏幕休眠/设备休眠/灯光主题）延后 1.5s 再读

## 快传 WiFi 凭据

对齐 C `generate_wifi_credentials()`（`device_model/mouse.py::compute_wifi_credentials`）：

```
input = 产品编号前 4 字符 + MAC 6 字节大写 HEX     # 如 "1042" + "2C81A7F049FF"
hash  = SHA-256(input)
SSID  = "MD_" + hash[0:6] 的大写 HEX              # 12 字符
密码  = hash[6:14] 的大写 HEX                     # 16 字符
```

界面：鼠标面板「AM60 专属」→ 点「打开快传」成功后自动读取 MAC 并显示
`SSID + 密码`（可选中复制）；关闭快传后清空。
- **录音压测**（随机时长开/关录音循环，长时间运行，实时成功率 + 逐循环日志落盘）

## 录音压测

鼠标面板「录音压测」按钮打开非模态对话框（引擎 `record_test.py::RecordStressTester`
可脱离 GUI 单独使用）：

- 每个循环：`start_record` → 随机时长录音 → `stop_record` → 随机间隔
- 成功定义：start/stop 应答均为 `ErrCode.SUCCESS`；音频字节数/含音频会话单独统计
- 可配置：录音时长范围、间隔范围、总时长（0=不限）、最大循环数（0=不限）
- 停止失败自动重试（默认再试 3 次，间隔 0.2s）：任一次重试成功本循环计入成功；
  全部耗尽才判失败（避免设备仍在录音时连锁失败）
- 连续失败 ≥5 次自动熔断中止（设备掉线后不刷失败）；中途停止会补发 stop 并把
  当次循环计入统计
- 日志：`logs/record_test_<地址>_<时间>.log`，逐循环结果 + 结束统计快照

### 断链判定策略（避免误判掉线）

- **USB**：只有 libusb 报 `NO_DEVICE(-4)` / `IO(-1)`（或 Windows errno 19/4）才判定"设备已拔出"；
  `PIPE(-9)` 等瞬时错误按 C 库 `usb_manager.c::_start_read` 的做法清 halt 后继续读。
  一次端点 stall 不会再把整条连接拆掉（否则 OTA 传输途中会突然变成"设备未连接"）。
- **BLE**：必须"先观察到 `Connected`、之后连续两次 `Disconnected`"才判定掉线。
  Windows CACHED 模式下 `connection_status` 常年是 `Disconnected`（不代表 GATT 会话断开），
  只看它会误杀刚连上的设备。
- **界面刷新**：断开回调跑在 RX 线程（无 Qt 事件循环），跨线程更新 UI 必须用
  `Signal`（自动排队到主线程），不能用 `QTimer.singleShot(0)`——它在没有事件循环的
  线程里永远不会触发，会表现成"拔掉 USB 但设备还留在列表里"。
- **重连保护**：同一地址已连上时 `connect()` 直接复用现有实例；否则同一 USB 设备会被
  两个 comm 抢占，旧 RX 线程与新线程抢 ACK 导致全部超时。

## 从机版本号

带从机的设备（AM60 鼠标、AT80 键盘）可以通过通用属性通道读取从机固件版本：

```python
from devtool import DeviceTool
tool = DeviceTool()
dev = tool.connect(tool.scan_usb()[0])   # 或 tool.connect_ble_bonded(mac, device_type=0x32)
for device_id, version in dev.get_slave_device_version():
    print(hex(device_id), ".".join(str(b) for b in version))
```

- 指令：`GET_SLAVE_PROP`，payload = `[0x1F]`（`PropKey.SLAVE_DEVICE_VERSION`）
- 应答有两种形态（真机实测）：
  - 6 字节整条：2 字节从机设备号 + 4 字节版本号，可有多条（AM60）
  - 短应答：整段就是版本号、没有设备号（AT80 实测回 2 字节 `0x0157`）
- `version` 是原始字节，用 `".".join(str(b) for b in version)` 拼成 `x.y.z.w`
- 界面：鼠标面板「AM60 专属」、键盘面板「AT80 专属」分组内的「从机版本号 读取」按钮

## 多设备并发 OTA 升级

升级窗口不再阻塞主窗口，可在升级期间继续连接新设备、发起第二批升级。

使用方式：
- **批量升级**：主窗口底部「批量固件升级」按钮（或 工具 → 批量固件升级）→ 打开批量升级窗口，
  窗口内每台设备一行（勾选 + 独立选择固件 + 独立进度条与状态），点「开始升级」后各设备并发升级。
  固件可「应用到全部」，也可逐台单独指定（不同型号不要用同一份固件）。
- **单设备升级**：鼠标/键盘面板里的「固件升级」按钮，打开同样的窗口（只含当前设备），非模态。

并发安全要点（多子设备共用一个接收器时尤其重要）：
- dongle 转发的子设备应答帧带 `source_mac`，各子设备的 adapter 按 mac 隔离应答，
  避免并发请求的 ACK 串台（各设备的 sequence 计数独立、会撞号）。
- `UsbComm` 写端加锁串行化，多个子设备并发发送时 USB 写不会交错。
- 同一台设备不允许被两个升级任务同时升级（全局登记正在升级的设备）。

真机验证（三类通道 + 多设备同时管理）：
- **USB 直连鼠标**（AM60, product_no=1042）：扫描/连接/CPI/回报率/battery/鉴权 全通过
- **多联接收器+T8 键盘**（Madao Device dongle, product_no=3007）：GET_CONNECTED_DEVICE/attach/透传指令/基础信息/鉴权 全通过
- **BLE 鼠标**（FeiShu_S, product_no=1018）：连接/product_no/mac/battery/CPI/鉴权 全通过
- **多设备同时管理**：USB 直连 + dongle 子设备 + BLE 三种通道并存，设备池统一管理
- T8 键盘 USB 直连：RGB 灯光/亮度/滚轮功能读取通过

已知设备差异：
- FeiShu_S 鼠标不支持 POLLING_RATE 属性（err=99），属正常设备能力差异
- BLE 设备 mac 上报的是随机地址，设备真实 mac 通过 GET_SLAVE_PROP 读取

## 协议一致性来源

| 本模块                 | C 参考                                                              |
|------------------------|---------------------------------------------------------------------|
| frame.py 打包          | windows/ProtocolLib/protocol_manager.c::get_protocol_command       |
| frame.py 解包          | protocol_manager.c::__is_full_pack / _parse_command                 |
| frame.py CRC32C        | mouseTool/mblue_crc.py + AM50 third/crc/mblue_crc.c                 |
| cmd.py 命令码          | windows/libProtocolFramework/libprotocol_2_define.h + AM50 protocol_define.h|
| prop_key.py 属性键     | 同上 PROTOCOL_PROP_KEY + 《马道设备通信协议.docx》                 |
| usb_comm.py 直连       | windows/libUsb/usb_manager.c                                        |
| usb_comm.py dongle     | windows/mspeech_usb_manager/mspeech_usb_manager.c                  |
| mouse.py CPI/回报率    | libProtocolFramework/protocol_logic_manager.c + AM50 logic_protocol_manager.c |
| base.py ACK 状态机     | protocol_logic_manager.c::__push_command_map / __ack                |
| ble_comm/winrt_ble_comm UUID | T8_light/AM50 vendor/8258_dual_kb/AAA_app_att.c (PROTOCOL_SERVICE=eae0, write=eae1, notify=eae2) |
| crypto/aes.py         | mblue_aes.c（FIPS-197 标准向量校验等价）                            |
| crypto/md5.py         | mblue_md5.c（RFC 1321 标准向量校验等价）                            |
| crypto/hash.py sdbm   | mblue_hash.c::_sdbm_mem                                             |
| auth.py 鉴权握手      | lib_device_manager.c::_start_authentication / protocol_logic_manager.c::_set_authenticate / mblue_identification_service.c |
| ota.py OTA 流程       | mspeech_usb_manager.c::_transfer_file_data/_updata_ota_func + logic_protocol_manager.c::_ota_logic |
| firmware_crypto.py    | mouseTool CLAUDE.md OTA 加密格式说明                                |
| keyboard.py RGB/按键  | T8_light logic_protocol_manager.c::_prop_resp/_set_prop (RGB_STATUS/CUSTOM_BUTTON/WHEEL_FUNCTION) |
