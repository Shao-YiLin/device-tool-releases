
#include "ico_manager.h"
#include "mblue_heap.h"

#define ENC_BITRATE 16000 /*encode bps*/
#define ENC_BANDWIDTH 7000

static int _init(struct ico_manager *vm, int channels)
{
	if (NULL == vm)
		return 0;
	ivStatus sta = 0;
	vm->ico_tParam.nBufferSize = BUFFER_SIZE;
	vm->ico_tParam.nBandWidth = ENC_BANDWIDTH;
	vm->ico_tParam.nBitRate = ENC_BITRATE;
	vm->ico_tParam.pBuffer = vm->ico_buffer;

	sta = ICOCreate(&vm->hICOHandleEnc, &vm->ico_tParam);
	if (sta != ivErr_OK)
	{
		printf("icocreate error: %d \n", sta);
		return 0;
	}
	else {
		printf("icocreate success \n");
	}

	return 1;
}

static void _reset(struct ico_manager *vm)
{
	if (NULL == vm || NULL == vm->hICOHandleEnc)
		return;
	ICOReset(vm->hICOHandleEnc);
}

static int _decode(struct ico_manager *vm, unsigned short *in, unsigned short *out, unsigned short *outLen)
{
	if (NULL == vm)
		return 0;

	if (NULL == out)
		return 0;

	ivStatus sta = 0;
	signed short outSize = 0;
	if (NULL == vm->hICOHandleEnc)
	{
		printf("hICOHandelEnc is NULL \n");
		return 0;
	}
	sta = ICODecoder(vm->hICOHandleEnc, (void *)in, 20, (void *)out, &outSize);
	if (sta != ivErr_OK)
	{
		printf("ICODecoder error %d \n", sta);
		return 0;
	}
	// *outLen = (short)outSize * 2;
	*outLen = outSize;
	return 1;

	return 1;
}


static void __install_ico_manager_operation(struct ico_manager *vm)
{
	if (NULL == vm)
		return;

	vm->base.init = _init;
	vm->base.reset = _reset;
	vm->base.decode = _decode;
	vm->hICOHandleEnc = NULL;
}

struct ico_manager *ico_manager_construct()
{
	struct ico_manager *vm = mblue_malloc(sizeof(struct ico_manager));
	if (NULL == vm)
		return NULL;
	__install_ico_manager_operation(vm);
	return vm;
}