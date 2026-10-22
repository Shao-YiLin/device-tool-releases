//#include <stdio.h>
//#include <stdlib.h>

#include "voice_process.h"
#include <string.h>

#define MAX_SAMPLE_RATE (32000)
#define MAX_FRAMESIZE   (MAX_SAMPLE_RATE/50)
#define FRAME_SIZE_7k   (MAX_FRAMESIZE>>1)

#define ENC_BITRATE     (16000) /*encode bps*/
#define ENC_BANDWIDTH   (7000)
// #define BUFFER_SIZE     5000
#define BUFFER_SIZE     2600

#define INPUT_SIZE_ENC  (FRAME_SIZE_7k)      /*samples of in*/   //320
#define OUT_SIZE_ENC    (ENC_BITRATE/50/8/2) /*samples of out*/   //20

 unsigned char temp_buffer[BUFFER_SIZE];
 unsigned char temp_buffer1[BUFFER_SIZE];
 static ivHandle hICOHandelEnc;
 static ivHandle hICOHandelEnc1;

 static TICOInitParam ico_tParam;
 static TICOInitParam ico_tParam1;

 int codec_init(void)
{
    ivStatus sta = 0;
    ico_tParam.nBufferSize = BUFFER_SIZE;
    ico_tParam.nBandWidth = ENC_BANDWIDTH;
    ico_tParam.nBitRate = ENC_BITRATE;
    ico_tParam.pBuffer = temp_buffer;

    //outSize = 0;
    sta = ICOCreate(&hICOHandelEnc, &ico_tParam);
    if(sta != ivErr_OK)
    {
       return 0;
    }
    ivStatus sta1 = 0;
    ico_tParam1.nBufferSize = BUFFER_SIZE;
    ico_tParam1.nBandWidth = ENC_BANDWIDTH;
    ico_tParam1.nBitRate = ENC_BITRATE;
    ico_tParam1.pBuffer = temp_buffer1;
    sta = ICOCreate(&hICOHandelEnc1, &ico_tParam1);
    if(sta1 != ivErr_OK)
    {
       return 0;
    }

    return 1;
}

void codec_reset(void)
{
	ICOReset(hICOHandelEnc);
	ICOReset(hICOHandelEnc1);
}

int codec_encode(char* in, char *out, short *outLen)
{
    ivStatus sta = 0;


    sta = ICOEncoder(hICOHandelEnc,(void *)in,INPUT_SIZE_ENC,(void *)out,outLen);
    if(sta != ivErr_OK)
    {
    	return 0;
    }
    return 1;
}

int codec_encode1(char* in, char *out, short *outLen)
{
    ivStatus sta1 = 0;

    sta1 = ICOEncoder(hICOHandelEnc1,(void *)in,INPUT_SIZE_ENC,(void *)out,outLen);
    if(sta1 != ivErr_OK)
    {
    	return 0;
    }
    return 1;
}



