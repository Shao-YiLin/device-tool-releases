#pragma once

#include <iostream>
#include <queue>
#include <Windows.h>
using namespace std;

typedef struct{
	unsigned char data[64];
	unsigned int len;
}MessageInfo;

class MadaoMessage
{
public:
	MadaoMessage();
	~MadaoMessage();

private:
	queue<MessageInfo> mQueue;
	CRITICAL_SECTION mLock;

public:
	void send(unsigned char *data, int size);
	int  recv(MessageInfo *data);
};

