"""协议常量定义。

对齐 src_archive/windows/libProtocolFramework/libprotocol_2_define.h
与 src_archive/AM50/third/protocol/protocol_define.h。
两处定义一致，本模块集中维护上位机/设备端共用的命令码与属性键。
"""


class Command:
    """PROTOCOL_COMMAND：帧头中的 cmd 字段。"""

    HANDSHAKE = 0x00
    AUTHENTICATE = 0x01
    START_RECORD = 0x02
    STOP_RECORD = 0x03
    UPDATE_CONN_PARAM = 0x04
    SET_SLAVE_PROP = 0x05
    GET_SLAVE_PROP = 0x06
    WRITE_DATA = 0x07
    READ_DATA = 0x08
    EVENT = 0x09
    LARGE_EVENT = 0x0A
    SCAN = 0x41
    CONNECT = 0x42
    SET_MASTER_PROP = 0x43
    GET_MASTER_PROP = 0x44
    MANUFACTURE_INSTRUCTION = 0x4A
    ACK = 0x81
    ASYNC_DATA = 0xA0


class UsbManagerCommand:
    """USB 多通道接收器（dongle）专用的 cmd 字段。

    对齐 src_archive/windows/mspeech_usb_manager/mspeech_usb_manager.h。
    dongle 作为协议代理时，上位机用这套 cmd 与 dongle 通信。
    """

    SCAN_DEVICE = 0x41
    WRITE_DATA = 0x45
    WRITE_DATA_RESP = 0x47  # dongle 透传子设备应答，对齐 mspeech_usb_manager.h
    DEVICE_MANAGER = 0x48
    DEVICE_EVENT = 0x49


class DeviceManagerAction:
    """DEVICE_MANAGER 指令 payload[0]。

    对齐 mspeech_usb_manager.c::DEVICE_MANAGER_ACTION。
    """

    CONNECT_DEVICE = 0x01
    REMOVE_DEVICE = 0x02
    GET_CONNECTED_DEVICE = 0x03
    DONGLE_OTA = 0x04
    RESET_DEVICE = 0x0F


class EventType:
    """EVENT 帧 payload[0]。

    对齐 AM50 protocol_define.h::PROTOCOL_EVENT。
    """

    KEY = 0x01
    BATTERY = 0x02
    BLE = 0x03
    WEB = 0x04
    SINCHECK = 0x05
    POLLING_RATE = 0x06
    RGB_STATUS = 0x07  # T8 键盘扩展，对齐 T8_light protocol_define.h::TYPE_RGB_STATUS


class ErrCode:
    """ACK 帧 payload 前 4 字节（小端）。

    对齐 protocol_define.h::ERR_CODE / libprotocol_logic_define.h::ERR_CODE。
    """

    FAIL = 0x00
    SUCCESS = 0x01
    DISCONNECT = 0x02
    UNKNOW = 0x63
