#pragma once

#define AES128 1
//#define AES192 1
//#define AES256 1

#define AES_BLOCKLEN 16 //Block length in bytes AES is 128b block only
#define AES_KEYLEN 16   // Key length in bytes
#define AES_keyExpSize 176

struct aes_ctx
{
	unsigned char round_key[AES_keyExpSize];
};

struct mblue_aes
{
	void (*init_ctx)(struct mblue_aes *aes, struct aes_ctx* ctx, const unsigned char* key);
	void (*ecb_encrypt)(struct mblue_aes *aes, const struct aes_ctx* ctx, unsigned char* buf);
	void (*ecb_decrypt)(struct mblue_aes *aes, const struct aes_ctx* ctx, unsigned char* buf);
};

void mblue_aes_construct(struct mblue_aes *aes);
