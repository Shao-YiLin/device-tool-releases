#ifndef _VOICE_MANAGER_H_
#define _VOICE_MANAGER_H_

#include <stdint.h>
#include "include/ico/ivCodecOne.h"

#define ENC_BITRATE 16000 /*encode bps*/
#define ENC_BANDWIDTH 7000
#define BUFFER_SIZE 5000

struct voice_manager
{
    unsigned char ico_buffer[BUFFER_SIZE];
    ivHandle hICOHandleEnc;
    TICOInitParam ico_tParam;

    int (*init)(struct voice_manager *vm);
    void (*reset)(struct voice_manager *vm);
    int (*decode)(struct voice_manager *vm, uint16 *in, uint16 *out, uint16 *outLen);
};

struct voice_manager *voice_manager_construct();

#endif
