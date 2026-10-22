

#include "protocol_define.h"
#include "../hash/mblue_hash.h"
#include "../crc/mblue_crc.h"
#include "protocol_manager.h"
//#include "string.h"

extern void *  memset(void * d, int c, unsigned int  n);
extern void *  memcpy(void * des_ptr, const void * src_ptr, unsigned int);

static u8 ucGlobalSequence = 0;
static struct command_data xCommand;
static struct common_header xPreHeader;
static struct protocol_header xProHeader;
//u8 device_secret[16];

//extern uint8_t *global_device_id;
//extern uint8_t *global_aes_key;
#define PROTOCOL_HEADER_PRE_SIZE ( 0x7 )

static u8 prvGetGlobalSequence()
{
    if (ucGlobalSequence != 0)
    	ucGlobalSequence = (ucGlobalSequence % 256);
    return ucGlobalSequence++;
}

//static void prvSetGlobalSequenceReset()
//{
//	ucGlobalSequence = 0;
//}

static void intToByte(u32 data, u8 *byteArray)
{
    //  uint8_t *p = byteArray;
    *(byteArray) = (data & 0xff);
    *(byteArray + 1) = (data >> 8 & 0xff);
    *(byteArray + 2) = (data >> 16 & 0xff);
    *(byteArray + 3) = (data >> 24 & 0xff);
}

u32 byteToInt(u8 *data, int len)
{
	u32 int_data = 0;
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

static u32 prvGetCrc32(u8 *data, int len)
{
    return calculate_crc32(data, len);
}

//static u8 *get_device_secret(u8 *device_id, int device_id_len, u32 random, u8 *key, int key_len)
//{
//
//    return NULL;
    //    if (NULL == device_id || NULL == key)
    //        return NULL;
    //
    //    u32 hash_device_id = sdbm_mem(device_id, device_id_len);
    //    u8 buffer[16] = {0};
    //    u8 temp[12] = {0};
    //    memcpy(temp, &hash_device_id, sizeof(hash_device_id));
    //    memcpy(temp + sizeof(hash_device_id), &random, sizeof(random));
    //    u32 xor_device_id = hash_device_id ^ random;
    //    memcpy(temp + sizeof(hash_device_id) + sizeof(random), &xor_device_id, sizeof(xor_device_id));
    //    u32 compose_hash = sdbm_hash(temp);
    //    memcpy(buffer, temp, sizeof(temp));
    //    memcpy(buffer + sizeof(temp), &compose_hash, sizeof(compose_hash));
    //
    //    if (aes128_ecb_encrypt(buffer, key, device_secret))
    //        return device_secret;
    //
    //    return device_secret;
//}

static struct command_data *prvGetProtocolCommand(u8 ucCmd, u8 ucSequence, u8 *pcData, u32 ulLength)
{
    u32 ulTotalSize = PROTOCOL_HEADER_PRE_SIZE;
    if (ulLength > 0)
    {
    	ulTotalSize = ulTotalSize + ulLength + 4;
    }

    memset(&xPreHeader, 0x0, sizeof(xPreHeader));
    xPreHeader.ucMagic = COMAMND_MAGIC;
    xPreHeader.ucSequence = ucSequence;
    xPreHeader.ucCmd = ucCmd;
    u8 ucPayloadLength[4] = {0};
    intToByte(ulLength, ucPayloadLength);

    memset(xCommand.pcData, 0x0, sizeof(xCommand.pcData));
    xCommand.ucLen = ulTotalSize;
    memcpy(xCommand.pcData, &xPreHeader, 3);
    memcpy(xCommand.pcData + 3, ucPayloadLength, sizeof(ucPayloadLength));
    if (ulLength > 0)
    {
        memcpy(xCommand.pcData + PROTOCOL_HEADER_PRE_SIZE, pcData, ulLength);
        u32 ulSign = prvGetCrc32(pcData, ulLength);
        memcpy(xCommand.pcData + PROTOCOL_HEADER_PRE_SIZE + ulLength, &ulSign, sizeof(ulSign));
    }

    return &xCommand;
}

// static struct command_data *_get_login_pack()
// {

// 	return get_protocol_command(LOGIN, NULL, 0);
// //	u32 random = os_sys_time_get();
// //	u8 *secret = get_device_secret(global_device_id, 16, random, global_aes_key, 16);
// //    if (NULL == secret)
// //        return NULL;
// //    u8 data[33] = {0};
// //    data[0] = 1;
// //    memcpy(data + 1, global_device_id, 16);
// //    memcpy(data + 1 + 16, secret, 16);
// //    return get_protocol_command(LOGIN, data, sizeof(data));
// }

static struct command_data *prvGetEventPack(u32 ulType, u32 ulValue)
{
    u8 pcBuffer[5] = {0};
    pcBuffer[0] = ulType;
    intToByte(ulValue, &pcBuffer[1]);

    return prvGetProtocolCommand(EVENT, prvGetGlobalSequence(), pcBuffer, sizeof(pcBuffer));
}

static struct command_data *prvRecordPack(u8 ucMode, u8 *pcData, u32 ulLen)
{
    if (NULL == pcData || ulLen == 0)
        return NULL;

    u8 pcBuffer[256] = {0};
    memset(pcBuffer, 0x0, sizeof(pcBuffer));
    pcBuffer[0] = ICOV1_PCM;
    pcBuffer[1] = ucMode;
    memcpy(pcBuffer + 2, pcData, ulLen);
    return prvGetProtocolCommand(ASYNC_DATA, prvGetGlobalSequence(), pcBuffer, ulLen + 2);
}

static struct protocol_header *prvParsePack(u8 *pcData, u32 ulLen)
{
    if (NULL == pcData || ulLen == 0)
        return NULL;

    u8 ucMagic = *pcData;
    if (COMAMND_MAGIC != ucMagic)
        return NULL;

    memset(&xProHeader, 0x0, sizeof(xProHeader));

    if (ulLen >= PROTOCOL_HEADER_PRE_SIZE)
    {
    	xProHeader.xHead.ucMagic = *pcData;
    	xProHeader.xHead.ucSequence = *(pcData + 1);
    	xProHeader.xHead.ucCmd = *(pcData + 2);
    	xProHeader.xHead.ulLength = byteToInt(pcData + 3, 4);
        if (xProHeader.xHead.ulLength > 0)
        {

            if (ulLen - PROTOCOL_HEADER_PRE_SIZE - 4 == xProHeader.xHead.ulLength)
            {
                memcpy(xProHeader.pcPayload, pcData + PROTOCOL_HEADER_PRE_SIZE, xProHeader.xHead.ulLength);
            }else{
                return NULL;
            }

            xProHeader.ulSign = byteToInt(pcData + PROTOCOL_HEADER_PRE_SIZE + xProHeader.xHead.ulLength, 4);
            u32 ulPayloadCrc = prvGetCrc32(xProHeader.pcPayload, xProHeader.xHead.ulLength);

            if (xProHeader.ulSign != ulPayloadCrc)
            {
                return NULL;
            }
        }
    }
    else
    {
        return NULL;
    }

    return &xProHeader;
}

static struct command_data *prvCommonAck(u8 ucSequence, u32 ulErrcode, u8 *pcData, u32 ulLen)
{
    u8 pcBuffer[4 + ulLen];
    memset(pcBuffer, 0, sizeof(pcBuffer));
    intToByte(ulErrcode, &pcBuffer[0]);
    if(ulLen > 0 && NULL != pcData){
    	memcpy(pcBuffer + 4, pcData, ulLen);
    }

    return prvGetProtocolCommand(ACK, ucSequence, pcBuffer, 4 + ulLen);
}

static struct command_data *prvGetPropResp(u8 ucSequence, u32 ulErrcode, u8 *pcData, u32 ulLen)
{
    u8 pcBuffer[64] = {0};
  //  memset(pcBuffer, 0x0, sizeof(pcBuffer));
    intToByte(ulErrcode, &pcBuffer[0]);
    memcpy(pcBuffer + 4, pcData, ulLen);

    return prvGetProtocolCommand(ACK, ucSequence, pcBuffer, 4 + ulLen);
}

static struct command_data *prvAuthenticate(u8 ucSequence, u8 ucPhase, u8 *pcData, u32 ulLen)
{
    // u8 buffer[1 + len];
    // memset(buffer, 0x0, sizeof(buffer));
    u8 pcBuffer[64] = {0};
    pcBuffer[0] = ucPhase;
    memcpy(pcBuffer + 1, pcData, ulLen);
    return prvGetProtocolCommand(AUTHENTICATE, ucSequence, pcBuffer, 1 + ulLen);
}

static struct protocol_manager *pxInstance;
static struct protocol_manager xManager = {
    // .login_pack = _get_login_pack,
    .event_pack = prvGetEventPack,
    .record = prvRecordPack,
    .parse_pack = prvParsePack,
    .ack = prvCommonAck,
    .get_prop_resp = prvGetPropResp,
    .authenticate = prvAuthenticate,
};

struct protocol_manager *get_protocol_manager_instance(void)
{
    if (NULL == pxInstance)
    {
    	pxInstance = &xManager;
    }

    return pxInstance;
}
