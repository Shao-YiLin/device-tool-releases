
// #include "include/commondefine.h"
#include <stdint.h>
#include "libspeech_voice.h"
//#include "voice_manager.h"
#include "mblue_heap.h"
//#include "voice_base.h"
#include "opus_manager.h"
#include "ico_manager.h"

static int _init(struct libspeech_voice *voice, int encodeType, int channels)
{
	int ret = 0;

	if (encodeType == ENCODE_ICO) {
		voice->handle = ico_manager_construct();
		voice->base = &((struct ico_manager *)voice->handle)->base;
	}
	else {
		voice->handle = opus_manager_construct();
		voice->base = &((struct opus_manager *)voice->handle)->base;
	}
	
	ret = ((struct voice_base *)voice->base)->init(voice->handle, channels);
	return ret;
}
static void _reset(struct libspeech_voice *voice)
{
	struct voice_base *vm = voice->base;
    if (!vm)
        return;
    vm->reset(voice->handle);
}

static int _decode(struct libspeech_voice *voice, unsigned short *in, unsigned short *out, unsigned short *outLen)
{
    struct voice_base *vm = voice->base;
    if (!vm)
        return 0;
    return vm->decode(voice->handle, in, out, outLen);
}

static void _uninit(struct libspeech_voice *voice)
{
    if (!voice->handle)
    {
		voice->reset(voice);
        mblue_free(voice->handle);
        voice->handle = 0;
    }
}

static void __install_libspeech_voice_operation(struct libspeech_voice *voice)
{
    if (!voice)
        return;
    voice->init = _init;
    voice->reset = _reset;
    voice->decode = _decode;
    voice->uninit = _uninit;

}

void libspeech_voice_construct(struct libspeech_voice *voice)
{
    __install_libspeech_voice_operation(voice);
}
