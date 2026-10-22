#ifndef _VOICE_PROCESS_H_
#define _VOICE_PROCESS_H_

#include "ico/ivCodecOne.h"

int codec_init(void);

void codec_reset(void);

int codec_encode(char* in, char *out, short *outLen);

int codec_encode1(char* in, char *out, short *outLen);

#endif
