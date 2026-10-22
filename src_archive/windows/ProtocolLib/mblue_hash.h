#ifndef  mblue_hash_INC
#define  mblue_hash_INC
#include <stdint.h>

uint32_t sdbm_hash(char *str);

uint32_t sdbm_mem(uint8_t *str, uint32_t len);

#endif   /* ----- #ifndef mblue_hash_INC  ----- */