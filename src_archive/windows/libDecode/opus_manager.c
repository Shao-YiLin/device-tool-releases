#include "opus_manager.h"
#include "mblue_heap.h"

#define ENC_BITRATE 16000 /*encode bps*/
#define ENC_OPUS_ENCODE_FRAME_SIZE 40

static int _init(struct opus_manager *vm, int channels)
{
	if (NULL == vm)
		return 0;

	vm->channels = channels;
	int error;
	vm->decoderHandle = opus_decoder_create(ENC_BITRATE, channels, &error);
	if (OPUS_OK != error) {
		return 0;
	}
	
	return 1;
}

static void _reset(struct opus_manager *vm)
{
	if (NULL == vm || NULL == vm->decoderHandle)
		return;

	opus_decoder_destroy(vm->decoderHandle);
}

static int _decode(struct opus_manager *vm, unsigned short *in, unsigned short *out, unsigned short *outLen)
{
	if (NULL == vm)
		return 0;

	if (NULL == vm->decoderHandle)
		return 0;

//	opus_int16 out_buffer[640] = { 0 };
	int out_sample = opus_decode(vm->decoderHandle, (unsigned char *)in, 40 * vm->channels, out, 320*vm->channels, 0);
	if (out_sample > 0) {
	//	memcpy(out, out_buffer, out_sample * 2 * vm->channels);
		*outLen = out_sample * vm->channels;
	}else {
		printf("opus_decode error:%d\r\n", out_sample);
		return 0;
	}

	return 1;
}


static void __install_opus_manager_operation(struct opus_manager *vm)
{
	if (NULL == vm)
		return;

	vm->base.init = _init;
	vm->base.reset = _reset;
	vm->base.decode = _decode;
	vm->decoderHandle = NULL;
}

struct opus_manager *opus_manager_construct()
{
	struct opus_manager *vm = mblue_malloc(sizeof(struct opus_manager));
	if (NULL == vm)
		return NULL;
	__install_opus_manager_operation(vm);
	return vm;
}