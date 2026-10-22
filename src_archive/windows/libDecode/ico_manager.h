#ifndef ICO_MANAGER_H_
#define ICO_MANAGER_H_

#include "voice_base.h"
#include "include/ico/ivCodecOne.h"

#define BUFFER_SIZE 5000

struct ico_manager
{
	unsigned char ico_buffer[BUFFER_SIZE];
	ivHandle hICOHandleEnc;
	TICOInitParam ico_tParam;
	struct voice_base base;
};

struct ico_manager *ico_manager_construct();


#endif // !ICO_MANAGER_H_
