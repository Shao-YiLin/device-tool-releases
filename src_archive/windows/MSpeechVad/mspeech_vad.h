#pragma once
#ifdef __cplusplus
// #include "mspeech_vad_type.h"

extern "C"
{
#endif // __cplusplus

	typedef enum
	{
		mspeech_vad_8k,
		mspeech_vad_16k,
	}MSPEECH_VAD_SAMPLE_TYPE;

	typedef enum
	{
		mspeech_vad_first_seg,
		mspeech_vad_seg,
		mspeech_vad_last_seg,
	}MSPEECH_VAD_SEG_STATUS;

	typedef struct
	{
		unsigned int begin;
		unsigned int end;
		MSPEECH_VAD_SEG_STATUS state;
	}MSPEECH_VAD_SEG;


	typedef void* MSPEECH_VAD_INST;


	// __declspec(dllexport) 
#define DllExport __declspec(dllexport)

	/****************************************************************
	// FunctionName : int mspeech_vad_create_session(MSPEECH_VAD_INST *pVadInst, MSPEECH_VAD_SAMPLE_TYPE sample_type, const char *szResBuf);
	// Function :   创建VAD实例
	// Parameter:
	//              MVAD_INST * pVadInst    VAD实例指针
	//              MSPEECH_VAD_SAMPLE_TYPE sample_type VAD实例支持的默认类型
					const char* szResBuf	资源文件的二进制地址
	// Return :     VAD定义错误码
	// Author: ruili6
	// Create: 2017-04-13 17:42:33
	****************************************************************/
	DllExport int mspeech_vad_create_session(MSPEECH_VAD_INST *pVadInst, MSPEECH_VAD_SAMPLE_TYPE sample_type, const char *szResBuf);
	/****************************************************************
// FunctionName : int mspeech_vad_destroy_session(MSPEECH_VAD_INST vadInst);
// Function :   销毁VAD实例
// Parameter:
//              MSPEECH_VAD_INST vadInst  VAD实例
// Return :     VAD定义错误码
// Author: ruili6
// Create: 2017-04-13 17:42:33
****************************************************************/
	DllExport int mspeech_vad_destroy_session(MSPEECH_VAD_INST vadInst);
	/****************************************************************
// FunctionName : int mspeech_vad_reset_session(MSPEECH_VAD_INST vadInst, MSPEECH_VAD_SAMPLE_TYPE sample_type);
// Function :   每次调用AppendPcm接口，如果返回值为4，重置VAD实例
// Parameter:
//              MSPEECH_VAD_INST vadInst  VAD实例
//              MSPEECH_VAD_SAMPLE_TYPE sample_type VAD实例支持的默认类型
// Return :     VAD定义错误码
// Author: ruili6
// Create: 2017-04-13 17:42:33
****************************************************************/
	DllExport int mspeech_vad_reset_session(MSPEECH_VAD_INST vadInst, MSPEECH_VAD_SAMPLE_TYPE sample_type);
	/****************************************************************
// FunctionName : int mspeech_vad_reset_sentence(MSPEECH_VAD_INST vadInst, MSPEECH_VAD_SAMPLE_TYPE sample_type);
// Function :   离线测试时，跑完一条样例，重置实例
// Parameter:
//              MSPEECH_VAD_INST vadInst  VAD实例
//              MSPEECH_VAD_SAMPLE_TYPE sample_type VAD实例支持的默认类型
// Return :     VAD定义错误码
// Author: ruili
// Create: 2017-03-23 17:48:30
****************************************************************/
	DllExport int mspeech_vad_reset_sentence(MSPEECH_VAD_INST vadInst, MSPEECH_VAD_SAMPLE_TYPE sample_type);


	/****************************************************************
// FunctionName : int mspeech_vad_append_pcm(MSPEECH_VAD_INST vadInst, const char* pcmBuf, unsigned int bufByteSize, int *pVolumLevel);
// Function :   向VAD实例中添加原始音频数据，内部同步处理数据，如果一次Append比较多，函数等待时间会比较长
// Parameter:
//              MSPEECH_VAD_INST vadInst  VAD实例
//              const char* pcmBuf 音频缓存
//              unsigned int bufByteSize 缓存大小，单位Byte
//              int bLast          是否是最后一块
//              int* pVolumLevel   该块的音量信息
// Return :     VAD定义错误码
// Author: ruili6
//Create: 2017-03-23 17:48:30
****************************************************************/

	DllExport int mspeech_vad_append_pcm(MSPEECH_VAD_INST vadInst, const char* pcmBuf, unsigned int bufByteSize, int *pVolumLevel);

	/****************************************************************
// FunctionName : int mspeech_vad_get_seg(MSPEECH_VAD_INST vadInst, MSPEECH_VAD_SEG *pSeg);
// Function :   获取一个VAD段
// Parameter:
//              MSPEECH_VAD_INST vadInst  VAD实例
//              MSPEECH_VAD_SEG * pSeg     VAD段信息结构体的指针，向该结构体填充指针，指向内存在外部管理
// Return :     VAD定义错误码
// Author: ruili6
//Create: 2017-03-23 17:48:30
****************************************************************/

	DllExport int mspeech_vad_get_seg(MSPEECH_VAD_INST vadInst, MSPEECH_VAD_SEG *pSeg);


	/****************************************************************
// FunctionName : int mspeech_vad_set_param(MSPEECH_VAD_INST vadInst, const char* szKey, const char* szValue);
// Function :   对VAD实例进行参数调节
// Parameter:
//              MSPEECH_VAD_INST vadInst  VAD实例
//              const char* szKey   类型
//              const char* szValue 传入新值
// Return :
// Author: ruili6
// Create: 2017-03-23 17:48:30
****************************************************************/

	DllExport int mspeech_vad_set_param(MSPEECH_VAD_INST vadInst, const char* szKey, const char* szValue);



	/****************************************************************
	// FunctionName : int mspeech_vad_get_sent_confidence(MSPEECH_VAD_INST vadInst,float* value);
	// Function :   获取一次session中有效seg的置信度
	// Parameter:
	//              MSPEECH_VAD_INST vadInst  VAD实例
	//              float* value     置信度
	// Return :     VAD定义错误码
	// Author: ruili
	// Create: 2016-12-20 17:05:07
	****************************************************************/

	DllExport int mspeech_vad_get_sent_confidence(MSPEECH_VAD_INST vadInst, float *value);

	/**
	* 仅需要在单元测试工具调用
	*/
	DllExport int mspeech_vad_get_last_seg(MSPEECH_VAD_INST vadInst, MSPEECH_VAD_SEG *seg);



#ifdef __cplusplus
}
#endif