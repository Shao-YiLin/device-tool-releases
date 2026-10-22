/*
 * @Author: xuhan
 * @Date: 2020-02-07 14:34:19
 * @LastEditors  : xuhan
 * @LastEditTime : 2020-02-08 10:42:17
 * @Description: 
 */
#ifndef PROTOCOL_DEFINE_INC
#define PROTOCOL_DEFINE_INC

// enum PROTOCOL_MAGIC
// {
//     COMAMND_MAGIC = 0xFE,
// };

// enum PROTOCOL_PROP_KEY
// {
//     PROP_KEY_LANGUAGE = 0x1,
//     PROP_KEY_DPI = 0x2,
//     PROP_KEY_VERSION_MOURSE = 0x3,
//     PROP_KEY_MAC_MOURSE = 0x4,
//     PROP_KEY_VERSION_DONGLE = 0x5,
//     PROP_KEY_MAC_DONGLE = 0x6,
//     PROP_KEY_BATTERY = 0x7,
//     PROP_KEY_RECORD_CHANNEL = 0x8
// };

enum PROTOCOL_LANGUAGE_TYPE
{
    ENGLISH = 0x1,
    CHINESE = 0x2
};

enum PROTOCOL_EVENT
{
    EVENT_TYPE_KEY = 0x1,
    EVENT_TYPE_BATTERY = 0x2,
    EVENT_TYPE_BLE = 0x3,
    EVENT_KEY_UP = 0x0,
    EVENT_KEY_DOWN = 0x1
};

enum PROTOCOL_RECORD_TYPE
{
    PLAIN_PCM = 0x1,
    ICOV1_PCM = 0x2,
    ICOV2_PCM = 0x3,
};

enum PROTOCOL_RECORD_CHANNEL
{
    RECORD_CHANNEL_0 = 0x0,
    RECORD_CHANNEL_1 = 0x1
};

enum PROTOCOL_RECORD_ORG_TYPE
{
    DICTATION_TYPE = 0x1,
    TRANSLATION_TYPE = 0x2
};

static void intToByte(unsigned int data, unsigned char *byteArray)
{
    //  uint8_t *p = byteArray;
    *(byteArray) = (data & 0xff);
    *(byteArray + 1) = (data >> 8 & 0xff);
    *(byteArray + 2) = (data >> 16 & 0xff);
    *(byteArray + 3) = (data >> 24 & 0xff);
}

static unsigned int byteToInt(unsigned char *data, int len)
{
    unsigned int int_data = 0;
    if (!data || len == 0)
        return int_data;

    if (len == 1)
        int_data = *data;
    else if (len == 2)
        int_data = *data | (*(data + 1)) << 8;
    else if (len == 3)
        int_data = *data | (*(data + 1)) << 8 | (*(data + 2)) << 16;
    else if (len == 4)
        int_data = *data | (*(data + 1)) << 8 | (*(data + 2)) << 16 | (*(data + 3)) << 24;

    return int_data;
}

#endif
