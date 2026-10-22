#ifndef __MVAD_TYPE_H__
#define __MVAD_TYPE_H__

#include "ivDefine.h"

typedef enum
{
    vad_8k,
    vad_16k,
}MVAD_SAMPLE_TYPE;

typedef enum
{
    vad_first_seg,
    vad_seg,
    vad_last_seg,
}MVAD_SEG_STATUS;

typedef struct  
{
    unsigned int nBegin;
    unsigned int nEnd;
    MVAD_SEG_STATUS eState;
}MVAD_SEG;

typedef void* MVAD_INST;

#endif //__MVAD_TYPE_H__