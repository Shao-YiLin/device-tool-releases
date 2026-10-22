#ifndef PROTOCOL_DEFINE_INC
#define PROTOCOL_DEFINE_INC

enum ERR_CODE
{
    ERR_FAIL = 0,
    ERR_SUCCESS = 1,
    ERR_DISCONNECT = 2,
    ERR_UNKNOW = 99
};

enum PROTOCOL_MAGIC
{
    COMAMND_MAGIC = 0xFE,
};

enum PROTOCOL_COMMAND
{
    HANDSHAKE = 0x0,    // ??¨º?
    AUTHENTICATE = 0x1, // ¨º¨²¨¨¡§?¨¦?¡è
    START_RECORD = 0x2, // ?a¨º???¨°?
    STOP_RECORD = 0x3,  // ?¨¢¨º???¨°?
    UPDATE_CONN_PARAM = 0x4,
    SET_SLAVE_PROP = 0x5, // ¨¦¨¨??¨º?D?
    GET_SLAVE_PROP = 0x6, // ??¨¨?¨º?D?
    WRITE_DATA = 0x7,
    READ_DATA = 0x8,
    EVENT = 0x9,            // ¨º¡À??¨¦?¡À¡§
    LARGE_EVENT = 0x0a,
    SCAN = 0x41,            // ¨¦¡§?¨¨ble¨ªa¨¦¨¨
    CONNECT = 0x42,         // ¨¢??¨®???¡§¨ªa¨¦¨¨
    SET_MASTER_PROP = 0x43, // ¨¦¨¨???¡Â?¨²¨º?D?
    GET_MASTER_PROP = 0x44, // ??¨¨??¡Â?¨²¨º?D?
    GET_MF_INS = 0x4a, //产线指令
    ACK = 0x81,             // ack
    DONGLE_RESP = 0x86,     // dongle respond
    ASYNC_DATA = 0xa0,
};

enum MF_INS_TYPE
{
	SINCHECK_IFLY = 0x1
};

enum PROROCOL_OTA_TYPE{
	START_OTA = 1,
	UPTATE_OTA,
	END_OTA
};

enum PROTOCOL_PROP_KEY
{
    LANGUAGE = 0x1,
    DPI = 0x2,
    PRODUCT = 0x3, // 2¨²?¡¤?¦Ì¨¢D¡À¨¤??
    RECORD_CHANNEL = 0x4,
    VERSION_DONGLE = 0x5,
    MAC_DONGLE = 0x6,
    CLIENT_FINGER = 0x7,    // ?��?��??������?????
    PROTOCOL_VERSION = 0x8, // ������?D-������?��?o?
    VERSION_MOURSE = 0x9,   // ������?��?��?o?
    MAC_MOURSE = 0xa,       // ������?mac��??��
    BATTERY = 0xb,          // ��?3?��?��?
    FINGER = 0xc,           // ������?????
    IFLY_AUTHEN = 0xd,
    FORBID_AHTUENTICATE = 0xe,
    OS_TYPE = 0xf,
    SLEEP_TIME = 0x10,
    IFLY_ANDROID=0x11,
    RGB_EFFECTS=0x12,
    CUSTOM_BUTTON=0x13,
    MOTOR_WIBRAWAIT=0x14,
    GET_THEME = 0x15,  // get rbg value of theme
    MOUSE_FREQUENCY = 0x1b,
    DEVICE_ACTION = 0x21,
};

enum PROTOCOL_LANGUAGE_TYPE
{
    ENGLISH = 0x1,
    CHINESE = 0x2
};

enum PROTOCOL_EVENT
{
    TYPE_KEY = 0x1,
    TYPE_BATTERY = 0x2,
    TYPE_BLE = 0x3,
    TYPE_WEB = 0x4,
    TYPE_SINCHECK = 0x5,
    TYPE_POLLING_RATE = 0x6,
    KEY_UP = 0x0,
    KEY_DOWN = 0x1
};

enum PROTOCOL_RECORD_TYPE
{
    PLAIN_PCM = 0x1,
    ICOV1_PCM = 0x2,
    ICOV2_PCM = 0x3,

    RECORD_CHANNEL_0 = 0x0,
    RECORD_CHANNEL_1 = 0x1
};

enum PROTOCOL_RECORD_ORG_TYPE
{
    DICTATION_TYPE = 0x1,
    TRANSLATION_TYPE = 0x2
};

enum PROTOCOL_AUTHENTICATE_PHASE
{
    PHASE1 = 0x1,
    PHASE2 = 0x2,
    PHASE3 = 0x3,
    PHASE4 = 0x4
};

enum PROTOCOL_OS_TYPE
{
    OS_TYPE_WINDOWS = 0x1,
    OS_TYPE_ANDROID = 0x2,
    OS_TYPE_MAC = 0x3,
    OS_TYPE_IOS = 0x4
};

#endif
