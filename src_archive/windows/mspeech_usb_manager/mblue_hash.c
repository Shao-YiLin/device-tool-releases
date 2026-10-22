#include "mblue_hash.h"

static unsigned int _sdbm_hash(struct mblue_hash *handle, char *str) {
	unsigned int hash = 0;
	while (*str)
	{
		// equivalent to: hash = 65599*hash + (*str++);
		hash = (*str++) + (hash << 6) + (hash << 16) - hash;
	}

	return (hash & 0x7FFFFFFF);
}

static unsigned int _sdbm_mem(struct mblue_hash *handle, unsigned char *str, unsigned int len) {
	unsigned int hash = 0;
	while (len--)
	{
		// equivalent to: hash = 65599*hash + (*str++);
		hash = (*str++) + (hash << 6) + (hash << 16) - hash;
	}

	return (hash & 0x7FFFFFFF);
}

void mblue_hash_construct(struct mblue_hash *hash) {
	if (hash) {
		hash->sdbm_hash = _sdbm_hash;
		hash->sdbm_mem = _sdbm_mem;
	}
}