#ifndef MADAO_MESSAGE_H_
#define MADAO_MESSAGE_H_

#include <Windows.h>
#include "mblue_fifo.h"

//typedef struct{
//	unsigned char cmd;
//	/*unsigned char mode;*/
//	unsigned int mode;
//	unsigned char channel;
//	unsigned char len;
//	unsigned char data[60];
//}MessageInfo;

typedef struct {
	unsigned int cmd;
	unsigned int mode;
	unsigned int channel;
	unsigned int len;
	unsigned char data[112];
}MessageInfo;

struct MadaoMessage
{
	struct mblue_fifo *pFifo;
	CRITICAL_SECTION mLock;
	void(*init)(struct MadaoMessage *msg);
	void(*uninit)(struct MadaoMessage *msg);
	void (*send)(struct MadaoMessage *msg, MessageInfo *data, int size);
	int  (*recv)(struct MadaoMessage *msg, MessageInfo *data);
};

void MadaoMessageConstruct(struct MadaoMessage *msg);

#endif


