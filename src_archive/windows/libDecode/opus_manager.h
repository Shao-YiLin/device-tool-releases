#ifndef  OPUS_MANAGER_H_
#define OPUS_MANAGER_H_

#include "include/opus/opus.h"
#include "voice_base.h"

struct opus_manager
{
	struct voice_base base;
	int channels;
	OpusDecoder *decoderHandle;
};

struct opus_manager *opus_manager_construct();

#endif // ! OPUS_MANAGER_H_
