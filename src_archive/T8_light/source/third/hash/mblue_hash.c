/*
 * =====================================================================================
 *
 *       Filename:  mblue_hash.c
 *
 *    Description:  simple string hash interface
 *
 *        Version:  1.0
 *        Created:  2017/10/8 10:36:05
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  gzzhu (), gzzhu@leqibike.com
 *   Organization:  
 *
 * =====================================================================================
 */
#include "mblue_hash.h"

u32 sdbm_hash(char *str)
{
	u32 hash = 0;
	while (*str)
	{
		// equivalent to: hash = 65599*hash + (*str++);
		hash = (*str++) + (hash << 6) + (hash << 16) - hash;
	}

	return (hash & 0x7FFFFFFF);
}

u32 sdbm_mem(u8 *str, u32 len)
{
	u32 hash = 0;
	while (len--)
	{
		// equivalent to: hash = 65599*hash + (*str++);
		hash = (*str++) + (hash << 6) + (hash << 16) - hash;
	}

	return (hash & 0x7FFFFFFF);
}

