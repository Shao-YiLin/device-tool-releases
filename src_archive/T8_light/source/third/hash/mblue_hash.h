/*
 * =====================================================================================
 *
 *       Filename:  mblue_hash.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2017/10/8 10:45:17
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  gzzhu (), gzzhu@leqibike.com
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef  mblue_hash_INC
#define  mblue_hash_INC

#include "../../common/types.h"

u32 sdbm_hash(char *str);

u32 sdbm_mem(u8 *str, u32 len);

#endif   /* ----- #ifndef mblue_hash_INC  ----- */

