// IflySm.cpp : 定义 DLL 应用程序的导出函数。
//

#include "IflySm.h"
#include <stdio.h>
#include "mblue_md5.h"
#include <Windows.h>
#include "AESCrypt.h"

static byte SB_ORG[] = { 0xe1, 0xb7, 0x9a, 0x22, 0xe3, 0x47, 0xf, 0xd6, 0xab, 0xa3, 0x84, 0x72, 0xbb, 0xea, 0x46, 0xb9 }; 

static void Decrypt(byte* toEncryptArray, int len, byte *outArray, int outLen)
{
	AESCrypt *aes = new AESCrypt(SB_ORG, sizeof(SB_ORG));
	aes->decrypt(toEncryptArray, outArray, outLen);
	delete aes;
}

static void SmartXor(byte *des, byte * src, int len)
{
	for (int i = 0; i < len; i++)
	{
		des[i] ^= src[i];
	}
}

void sm_adapter(void *pData, int len, int tag, sm_cb_fn cb) {
	if (NULL == pData || NULL == cb)
		return;

	byte outData[96] = { 0 };
	Decrypt((byte *)pData, 96, outData, 96);
	byte *p = outData;

	byte timeStampBuffer[16] = { 0 };
	byte ctrlBuffer[16] = { 0 };
	byte randomBuffer[16] = { 0 };
	byte identifierBuffer[32] = { 0 };
	memcpy(timeStampBuffer, p, 16);
	memcpy(ctrlBuffer, p + 16, 16);
	memcpy(randomBuffer, p + 32, 16);
	memcpy(identifierBuffer, p + 48, 32);

	byte timeStamp[16] = { 0 };
	Decrypt(timeStampBuffer, 16, timeStamp, 16);
	byte ctrl[16] = { 0 };
	Decrypt(ctrlBuffer, 16, ctrl, 16);
	byte random[16] = { 0 };
	Decrypt(randomBuffer, 16, random, 16);
	byte identifier[32] = { 0 };
	Decrypt(identifierBuffer, 32, identifier, 32);
	 
	byte timeTemp[8] = { 0 };
	memcpy(timeTemp, timeStamp, 8);

	byte randomTemp[8] = { 0 };
	memcpy(randomTemp, random, 8);

	byte ctrlTemp[8] = { 0 };
	memcpy(ctrlTemp, ctrl, 8);

	byte identifierTemp[20] = { 0 };
	memcpy(identifierTemp, identifier, 20);

	SmartXor(timeTemp, randomTemp, 8);
	SmartXor(ctrlTemp, timeStamp, 8);
	SmartXor(identifierTemp, timeStamp, 8);

	byte inputBuffer[28] = { 0 };
	memcpy(inputBuffer, identifierTemp, 20);
	memcpy(inputBuffer + 20, randomTemp, 8);

	md5_ctx md5Cxt;
	md5_init(&md5Cxt);
	md5_update(&md5Cxt, inputBuffer, sizeof(inputBuffer));
	byte outBuffer[16] = { 0 };
	md5_final(&md5Cxt, outBuffer);

	cb(outBuffer, sizeof(outBuffer), tag);
}




