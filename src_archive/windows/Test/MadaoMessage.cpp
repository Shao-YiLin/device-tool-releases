#include "MadaoMessage.h"

MadaoMessage::MadaoMessage()
{
	InitializeCriticalSection(&mLock);
}

MadaoMessage::~MadaoMessage()
{
	DeleteCriticalSection(&mLock);
}

void MadaoMessage::send(unsigned char * data, int size)
{
	EnterCriticalSection(&mLock);
	MessageInfo info;
	info.len = size;
	memcpy(info.data, data, size);
	mQueue.push(info);
	LeaveCriticalSection(&mLock);
}

int MadaoMessage::recv(MessageInfo * data)
{
	int ret = 0;
	EnterCriticalSection(&mLock);
	if (mQueue.size() > 0) {
		MessageInfo ele = mQueue.front();
		if (data) {
			memcpy(data->data, ele.data, ele.len);
			data->len = ele.len;
			ret = 1;
		}
		mQueue.pop();
	}
	LeaveCriticalSection(&mLock);
	return ret;
}
