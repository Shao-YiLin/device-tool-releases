#ifndef  heap_INC
#define  heap_INC

#include "include/commondefine.h"


struct mblue_heap {
	//void *data;

	//int (* init)(struct mblue_heap *heap);
	void* (*_malloc)(struct mblue_heap *heap, uint32_t size);
	void (* _free)(struct mblue_heap *heap, void *p);
	//int (* uninit)(struct mblue_heap *heap);
};

void *mblue_malloc(uint32_t size);
void mblue_free(void *p);

#endif   /* ----- #ifndef heap_INC  ----- */
