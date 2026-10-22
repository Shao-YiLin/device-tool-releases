#pragma once

typedef struct
{
	unsigned int count[2];
	unsigned int state[4];
	unsigned char buffer[64];
} md5_ctx;

#define F(x, y, z) ((x & y) | (~x & z))
#define G(x, y, z) ((x & z) | (y & ~z))
#define H(x, y, z) (x ^ y ^ z)
#define I(x, y, z) (y ^ (x | ~z))
#define ROTATE_LEFT(x, n) ((x << n) | (x >> (32 - n)))
#define FF(a, b, c, d, x, s, ac)  \
    {                             \
        a += F(b, c, d) + x + ac; \
        a = ROTATE_LEFT(a, s);    \
        a += b;                   \
    }
#define GG(a, b, c, d, x, s, ac)  \
    {                             \
        a += G(b, c, d) + x + ac; \
        a = ROTATE_LEFT(a, s);    \
        a += b;                   \
    }
#define HH(a, b, c, d, x, s, ac)  \
    {                             \
        a += H(b, c, d) + x + ac; \
        a = ROTATE_LEFT(a, s);    \
        a += b;                   \
    }
#define II(a, b, c, d, x, s, ac)  \
    {                             \
        a += I(b, c, d) + x + ac; \
        a = ROTATE_LEFT(a, s);    \
        a += b;                   \
    }

struct mblue_md5
{
	void (*init)(struct mblue_md5 *md5, md5_ctx *context);
	void (*update)(struct mblue_md5 *md5, md5_ctx *context, unsigned char *input, unsigned int inputlen);
	void (*final)(struct mblue_md5 *md5, md5_ctx *context, unsigned char digest[16]);
	void (*transform)(struct mblue_md5 *md5, unsigned int state[4], unsigned char block[64]);
	void (*encode)(struct mblue_md5 *md5, unsigned char *output, unsigned int *input, unsigned int len);
	void (*decode)(struct mblue_md5 *md5, unsigned int *output, unsigned char *input, unsigned int len);
};

void mblue_md5_construct(struct mblue_md5 *md5);
