#ifndef __MVAD_API_H__
#define __MVAD_API_H__
#include "stdlib.h"
#include "mvad_type.h"
#include "mvad_error.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

    /****************************************************************   
    // FunctionName : int VADCreateSession(MVAD_INST * pVadInst, MVAD_SAMPLE_TYPE eSample,const char* szResBuf);
    // Function :   创建VAD实例
    // Parameter: 
    //              MVAD_INST * pVadInst    VAD实例指针
    //              MVAD_SAMPLE_TYPE eSample VAD实例支持的默认类型
					const char* szResBuf	资源文件的二进制地址
    // Return :     VAD定义错误码
    // Author: ruili6 
    // Create: 2017-04-13 17:42:33 
    ****************************************************************/
    int VADCreateSession(MVAD_INST * pVadInst, MVAD_SAMPLE_TYPE eSample,const char* szResBuf);

    /****************************************************************   
    // FunctionName : int VADDestroySession(MVAD_INST oVadInst);
    // Function :   销毁VAD实例
    // Parameter: 
    //              MVAD_INST oVadInst  VAD实例
    // Return :     VAD定义错误码
    // Author: ruili6 
    // Create: 2017-04-13 17:42:33 
    ****************************************************************/
    int VADDestroySession(MVAD_INST oVadInst);

    /****************************************************************   
    // FunctionName : int VADResetSession(MVAD_INST oVadInst, MVAD_SAMPLE_TYPE eSample);
    // Function :   每次调用AppendPcm接口，如果返回值为4，重置VAD实例
    // Parameter: 
    //              MVAD_INST oVadInst  VAD实例
    //              MVAD_SAMPLE_TYPE eSample VAD实例支持的默认类型
    // Return :     VAD定义错误码
    // Author: ruili6 
    // Create: 2017-04-13 17:42:33 
    ****************************************************************/
    int VADResetSession(MVAD_INST oVadInst, MVAD_SAMPLE_TYPE eSample);
	/****************************************************************   
    // FunctionName : int VADResetSentence(MVAD_INST oVadInst, MVAD_SAMPLE_TYPE eSample);
    // Function :   离线测试时，跑完一条样例，重置实例
    // Parameter: 
    //              MVAD_INST oVadInst  VAD实例
    //              MVAD_SAMPLE_TYPE eSample VAD实例支持的默认类型
    // Return :     VAD定义错误码
    // Author: ruili 
    // Create: 2017-03-23 17:48:30 
    ****************************************************************/
	int VADResetSentence(MVAD_INST oVadInst, MVAD_SAMPLE_TYPE eSample);
    /****************************************************************   
    // FunctionName : int VADAppendPCM(MVAD_INST oVadInst, const char* pPcmBuf, unsigned int nBufByteSize, int bLast, int* pnVolumLevel);
    // Function :   向VAD实例中添加原始音频数据，内部同步处理数据，如果一次Append比较多，函数等待时间会比较长
    // Parameter: 
    //              MVAD_INST oVadInst  VAD实例
    //              const char* pPcmBuf 音频缓存
    //              unsigned int nBufByteSize 缓存大小，单位Byte
    //              int bLast          是否是最后一块
    //              int* pnVolumLevel   该块的音量信息
    // Return :     VAD定义错误码
    // Author: ruili6 
    //Create: 2017-03-23 17:48:30 
    ****************************************************************/
    int VADAppendPCM(MVAD_INST oVadInst, const char* pPcmBuf, unsigned int nBufByteSize, int* pnVolumLevel);

    /****************************************************************   
    // FunctionName : int VADGetSeg(MVAD_INST oVadInst, MVAD_SEG * pSeg);
    // Function :   获取一个VAD段
    // Parameter: 
    //              MVAD_INST oVadInst  VAD实例
    //              MVAD_SEG * pSeg     VAD段信息结构体的指针，向该结构体填充指针，指向内存在外部管理
    // Return :     VAD定义错误码
	// Author: ruili6 
	//Create: 2017-03-23 17:48:30 
    ****************************************************************/
    int VADGetSeg(MVAD_INST oVadInst, MVAD_SEG * pSeg);

    /****************************************************************   
    // FunctionName : int VADSetParam(MVAD_INST oVadInst, const char* szKey, const char* szValue);
    // Function :   对VAD实例进行参数调节
    // Parameter: 
    //              MVAD_INST oVadInst  VAD实例
    //              const char* szKey   类型
    //              const char* szValue 传入新值
    // Return : 
	// Author: ruili6 
	// Create: 2017-03-23 17:48:30 
    ****************************************************************/
    int VADSetParam(MVAD_INST oVadInst, const char* szKey, const char* szValue);

	/****************************************************************   
    // FunctionName : int VADGetSentConfidence(MVAD_INST oVadInst,float* fValue);
    // Function :   获取一次session中有效seg的置信度
    // Parameter: 
    //              MVAD_INST oVadInst  VAD实例
    //              float* fValue     置信度
    // Return :     VAD定义错误码
    // Author: ruili 
    // Create: 2016-12-20 17:05:07 
    ****************************************************************/
	int VADGetSentConfidence(MVAD_INST oVadInst,float* fValue);
	int VADGetLastSeg(MVAD_INST oVadInst, MVAD_SEG * pSeg);//仅在单元测试工具调用;
#ifdef __cplusplus
};


#endif //__cplusplus

#endif // __MVAD_API_H__
