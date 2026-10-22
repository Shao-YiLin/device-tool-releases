#include "../../third/md5/mblue_md5.h"
#include "../../third/hash/mblue_hash.h"
#include "common/string.h"
#include "../../drivers/8258/aes.h"
#include "../../drivers/8258/random.h"
#include "drivers.h"
#include "../../drivers/8258/flash.h"
#include "AAA_public_config.h"
#define VALID_LEN ( 16 )
static MD5_CTX xMd5Ctx;
static unsigned char pcFingerBuffer[VALID_LEN] = {0};
static unsigned char pcClientFingerBuffer[VALID_LEN] = {0};
static unsigned char pcClientRandomBuffer[VALID_LEN] = {0};
static unsigned char pcRandomBuffer[VALID_LEN] = {0};
//extern void vGetSessionKey(unsigned char *pcKey);


/**
 * @brief  get mouse finger
 * @note   
 * @param  *finger: 
 * @retval 
 */
static void vGetSessionKey(unsigned char *pcKey)
{
	memcpy(pcKey, keyboard_mac_address, 6);
	*(pcKey + 6) = '{';
	memcpy(pcKey + 7, keyboard_version, 4);
	*(pcKey + 11) = '}';
	u32 hash_key = sdbm_mem(pcKey, 12);
	memcpy(pcKey + 12, &hash_key, 4);
}
static u8 prvGetFinger(unsigned char *pcFinger)
{
    unsigned int ulFlashMid;
    unsigned char pcFlashUid[VALID_LEN];
    int flag = flash_read_mid_uid_with_check(&ulFlashMid, pcFlashUid);
    if (flag == 1)
    {
     //   printf("read uid success \r\n");
        MD5Init(&xMd5Ctx);
        MD5Update(&xMd5Ctx, pcFlashUid, sizeof(pcFlashUid));
        MD5Final(&xMd5Ctx, pcFinger);
        return 1;
    }
    return 0;
}

void vMblueIdentificationInit()
{
  //  random_generator_init();
    // generator mouse ginger
	prvGetFinger(pcFingerBuffer);
//	toHex(pcFingerBuffer, VALID_LEN);
}

u8 ucMblueGetFinger(unsigned char *pcFinger)
{
//    if (strlen(pcFingerBuffer) == VALID_LEN)
    {
    //	printf("finger success \n");
        memcpy(pcFinger, pcFingerBuffer, VALID_LEN);
        return VALID_LEN;
    }
//    return 0;
}

/**
 * @brief  get mouse challenge1
 * @note   
 * @param  *challenge: 
 * @retval None
 */
void vGetIdentificationRandom(unsigned char *pcChallenge)
{
    generateRandomNum(VALID_LEN, pcRandomBuffer);
    unsigned char ucXorValue[VALID_LEN] = {0};
    for (size_t i = 0; i < VALID_LEN; i++)
    {
    	ucXorValue[i] = pcFingerBuffer[i] ^ pcRandomBuffer[i];
    }

    unsigned char pcSessionKey[VALID_LEN] = {0};
    vGetSessionKey(pcSessionKey);
    aes_encrypt(pcSessionKey, ucXorValue, pcChallenge);
}

/**
 * @brief  get mouse response2
 * @note   
 * @param  *salt: 
 * @retval None
 */
void vGetIdentificationSalt(unsigned char *pcSalt)
{
    u32 ulHashFinger = sdbm_mem(pcClientFingerBuffer, VALID_LEN);
  //  toHex(pcClientFingerBuffer, 16);
    unsigned char pcTemp[4 + VALID_LEN] = {0};
    memcpy(pcTemp, &ulHashFinger, 4);
    memcpy(pcTemp + 4, pcClientRandomBuffer, VALID_LEN);
  //  toHex(pcClientRandomBuffer, 16);
    MD5Init(&xMd5Ctx);
    MD5Update(&xMd5Ctx, pcTemp, sizeof(pcTemp));
    MD5Final(&xMd5Ctx, pcSalt);
}

/**
 * @brief  pc setting current pc finger
 * @note   
 * @param  *finger: 
 * @param  len: 
 * @retval None
 * 
 */
void vSetIdentificationClientFinger(unsigned char *pcFinger, unsigned int ulLength)
{
    if (ulLength != VALID_LEN)
        return;

    memcpy(pcClientFingerBuffer, pcFinger, ulLength);
}
/**
 * @brief  pc setting response1
 * @note   
 * @param  *salt: 
 * @param  len: 
 * @retval 
 */
u8 ucCheckIdentificationClientSalt(unsigned char *pcSalt, unsigned int ulLength)
{
    if (NULL == pcSalt || ulLength == 0)
        return 0;

    u32 ulHashFinger = sdbm_mem(pcFingerBuffer, VALID_LEN);
    unsigned char pcTemp[4 + VALID_LEN] = {0};
    memcpy(pcTemp, &ulHashFinger, 4);
    memcpy(pcTemp + 4, pcRandomBuffer, VALID_LEN);
    MD5Init(&xMd5Ctx);
    MD5Update(&xMd5Ctx, pcTemp, sizeof(pcTemp));
    unsigned char pcDecryptBuf[VALID_LEN] = {0};
    MD5Final(&xMd5Ctx, pcDecryptBuf);
    if (!memcmp(pcSalt, pcDecryptBuf, VALID_LEN))
        return 1;
    else
        return 0;
}

/**
 * @brief  pc setting challenge2
 * @note   
 * @param  *random: 
 * @param  len: 
 * @retval None
 */
void vSetIdentificationClientRandom(unsigned char *pcRandom, unsigned int ulLength)
{
    if (ulLength != VALID_LEN)
        return;

    unsigned char pcDecryptBuf[VALID_LEN] = {0};
    unsigned char pcSessionKey[VALID_LEN] = {0};
    vGetSessionKey(pcSessionKey);
    aes_decrypt(pcSessionKey, pcRandom, pcDecryptBuf);
    // a = a^b^b
    for (size_t i = 0; i < VALID_LEN; i++)
    {
        pcClientRandomBuffer[i] = pcDecryptBuf[i] ^ pcClientFingerBuffer[i];
    }
}
#if PROTOCOL_OTA_ENABLE
struct ota_items{
	unsigned char ota_pc_session[VALID_LEN];
	unsigned char ota_pc_session_key[VALID_LEN];
	unsigned int firmware_hash_sum;
	unsigned int firmware_crc32_sum;
};
static struct ota_items ota_param;

unsigned char identification_client_session(unsigned char *in_data, unsigned int ulLength, unsigned char *out_data)
{
	 if (ulLength != VALID_LEN)
		return 0;
	 vGetSessionKey(ota_param.ota_pc_session_key);
	 ota_param.firmware_hash_sum = 0;
	 ota_param.firmware_crc32_sum = 0;
	 aes_decrypt(ota_param.ota_pc_session_key, in_data, ota_param.ota_pc_session);
	 MD5Init(&xMd5Ctx);
	 MD5Update(&xMd5Ctx, ota_param.ota_pc_session, VALID_LEN);
	 MD5Final(&xMd5Ctx, out_data);
	 return 1;
}

unsigned int identification_firmware(unsigned char *in_data, unsigned int ulLength, unsigned char *out_data)
{
	if (ulLength != (VALID_LEN * 2))
		return 0;
	unsigned char firmwareKey[VALID_LEN] = {0};
	aes_decrypt(ota_param.ota_pc_session, in_data, firmwareKey);
	for (size_t i = 0; i < VALID_LEN; i++)
	{
		*(out_data + i) = firmwareKey[i] ^ ota_param.ota_pc_session_key[i];
	}
	aes_decrypt(ota_param.ota_pc_session, in_data + VALID_LEN, firmwareKey);
	for (size_t i = 0; i < VALID_LEN; i++)
	{
		*(out_data + i + VALID_LEN) = firmwareKey[i] ^ ota_param.ota_pc_session_key[i];
	}
	ota_param.firmware_hash_sum += sdbm_mem(out_data, VALID_LEN * 2);
	ota_param.firmware_crc32_sum += calculate_crc32(out_data, VALID_LEN *2);
	return ota_param.firmware_hash_sum;
}

unsigned int get_firmware_crc32_sum()
{
	return ota_param.firmware_crc32_sum;
}
#endif

