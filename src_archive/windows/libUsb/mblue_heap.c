/*
 * =====================================================================================
 *
 *       Filename:  heap.c
 *
 *    Description:  abstractive description of heap
 *    NOTE:You should reimplement this file when you port mblue to a new platform
 *
 *        Version:  1.0
 *        Created:  2017/4/10 17:56:40
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  gzzhu (), gzzhu@leqibike.com
 *   Organization:  
 *
 * =====================================================================================
 */
#include <string.h>
#include <malloc.h>
#include "mblue_heap.h"

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  heap_init
 *  Description:  init wrapper of heap
 * =====================================================================================
 */
/*static int heap_init(struct mblue_heap *heap)
{
	return 0;
}*/

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  heap_malloc
 *  Description:  malloc wrapper of heap
 * =====================================================================================
 */
static void *heap_malloc(struct mblue_heap *heap, uint32_t size)
{
	return malloc(size);
}

/* 
 * ===  function  ======================================================================
 *         name:  heap_malloc
 *  description:  free wrapper of heap
 * =====================================================================================
 */
static void heap_free(struct mblue_heap *heap, void *p)
{
	free(p);
}

/* 
 * ===  function  ======================================================================
 *         name:  heap_malloc
 *  description:  uninit wrapper of heap
 * =====================================================================================
 */
/*static int heap_uninit(struct mblue_heap *heap)
{
	return 0;
}*/

static struct mblue_heap mheap = {
	._malloc = heap_malloc,
	._free = heap_free,
};

void *mblue_malloc(uint32_t size)
{
	void *p = mheap._malloc(&mheap, MBLUE_ALIGN(size));

	if (p) {
		memset(p, 0, size);
		return p;
	}
	return NULL;
}

void mblue_free(void *p)
{
	mheap._free(&mheap, p);
}
