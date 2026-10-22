#include "voice_manager.h"
#include <string.h>
#include <stdio.h>
#include "mblue_heap.h"
//#include "include/commondefine.h"

static int _init(struct voice_manager *vm)
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
    }else{
        printf("icocreate success \n");
    }

    return 1;
}

static void _reset(struct voice_manager *vm)
{
    if (NULL == vm)
        return;
    ICOReset(vm->hICOHandleEnc);
}

static int _decode(struct voice_manager *vm, uint16 *in, uint16 *out, uint16 *outLen)
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
}

static void __install_voice_manager_operation(struct voice_manager *vm)
{
    if (NULL == vm)
        return;

    vm->init = _init;
    vm->reset = _reset;
    vm->decode = _decode;
    vm->hICOHandleEnc = NULL;
}

struct voice_manager *voice_manager_construct()
{
    struct voice_manager *vm = mblue_malloc(sizeof(struct voice_manager));
    if (NULL == vm)
        return NULL;
    __install_voice_manager_operation(vm);
    return vm;
}
