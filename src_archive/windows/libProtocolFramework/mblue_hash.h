#pragma once

struct mblue_hash {
	unsigned int (*sdbm_hash)(struct mblue_hash *hash, char *str);
	unsigned int (*sdbm_mem)(struct mblue_hash *hash, unsigned char *str, unsigned int len);
};

void mblue_hash_construct(struct mblue_hash *hash);