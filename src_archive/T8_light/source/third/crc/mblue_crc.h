/*
 * =====================================================================================
 *
 *       Filename:  crc32.h
 *
 *    Description:  crc32
 *
 *        Version:  1.0
 *        Created:  2017/10/9 10:05:56
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  gzzhu (), gzzhu@leqibike.com
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef  crc32_INC
#define  crc32_INC

#include "../../common/types.h"

u32 calculate_crc32 (void *buffer, u32 size);
u16 calculate_crc16 (u8* buffer, u32 size);

#endif   /* ----- #ifndef crc32_INC  ----- */

