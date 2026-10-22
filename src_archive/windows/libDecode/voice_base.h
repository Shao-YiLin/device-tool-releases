#ifndef VOICE_BASE_H_
#define VOICE_BASE_H_

#include <stdint.h>

struct voice_base {
	
	int(*init)(void *handle, int channels);
	void(*reset)(void *handle);
	int(*decode)(void *handle, unsigned short *in, unsigned short *out, unsigned short *outLen);
};

#endif // !VOICE_BASE_H_

