#include "MadaoMessage.h"

static void _init(struct MadaoMessage *msg)
{
	InitializeCriticalSection(&msg->mLock);
	if(NULL == msg->pFifo)
		msg->pFifo = mblue_fifo_alloc(sizeof(MessageInfo) * 16);
}

static void _uninit(struct MadaoMessage *msg)
{
	DeleteCriticalSection(&msg->mLock);
	if (msg->pFifo) {
		mblue_fifo_free(msg->pFifo);
		msg->pFifo = NULL;
	}
}

static void _send(struct MadaoMessage *msg, MessageInfo *data, int size)
{
	EnterCriticalSection(&msg->mLock);
	if (NULL != data && size > 0) {
		
		for (int i = 0; i < size; i++)
		{
			if (msg->pFifo != NULL) {
				int len = msg->pFifo->put(msg->pFifo, (unsigned char *)(data + i), sizeof(MessageInfo));
				
			}
		}
	}
	LeaveCriticalSection(&msg->mLock);
}

static int _recv(struct MadaoMessage *msg, MessageInfo *data)
{
	int ret = 0;
	EnterCriticalSection(&msg->mLock);
	if (NULL != msg->pFifo && data) {
		if (msg->pFifo->len(msg->pFifo) >= sizeof(MessageInfo)) {
			int len = msg->pFifo->get(msg->pFifo, (unsigned char*)data, sizeof(MessageInfo));
			if (len) {
				ret = 1;
			}
		}
	}
	LeaveCriticalSection(&msg->mLock);
	return ret;
}


void MadaoMessageConstruct(struct MadaoMessage *msg) {
	if (msg) {
		msg->init = _init;
		msg->uninit = _uninit;
		msg->send = _send;
		msg->recv = _recv;
	}
}
