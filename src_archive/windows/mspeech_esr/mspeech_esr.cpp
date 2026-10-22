#include "mspeech_esr.h"
#include <stdio.h>
#include <Windows.h>
#include <malloc.h>
// mspeech_esr.cpp : 定义静态库的函数。
//

static initCallBack mInitCallBack = 0;
static EsrSpeechListener *mEsrListener = 0;
static HINSTANCE smHandle;
static bool speechEnableFlag = false;
static int mAudioSize = 0;
static int mSmActive = -1;

typedef int(__cdecl* sm_authentication)(void *data, int len, int tag);

typedef int(__stdcall *sm_init)(sm_authentication pAuth);

typedef int(__stdcall *sm_active)(void *pData, int dateaLen, int tag);

typedef void(*sm_cb_fn)(void *pData, int len, int tag);

typedef void(*sm_adapter)(void *pData, int len, int tag, sm_cb_fn cb);

typedef int(_cdecl *SM_Translate_on_result)(INT32 session, int tag, const wchar_t* src, const wchar_t* result);
typedef int(__stdcall *sm_regist_ttCallback)(SM_Translate_on_result pCallback);

typedef struct _SM_CONFIG {
	int language = 0;                      //识别语言
	int translateLanguage = 0;    //翻译语言
	int domain = 0;
	int symbolMode = 0;
	int pgs = 1;	// pgs默认打开，值为0时关闭pgs
	int frontTime = 3000;                          //前端点时间，单位ms
	int tailTime = 2000;                           //后端点时间，单位ms
//	int timeout = 15000;                            //超时时间，单位ms

	// AIUI
	const char* lng = 0;							// 经度，保留字段，暂时无用
	const char* lat = 0;							// 纬度，保留字段，暂时无用

	// 离线命令配置
	const char* esrResPath = 0;	 				// 离线识别资源目录 (语法文件是grammar.txt,GBK格式,放在该目录下)。保留字段，暂时无用
	const char* esrPPROCPath = 0; 				// 离线识别工作目录 （用于保存相关日志）。保留字段，暂时无用

}SM_Config;


typedef struct _SM_Speech_Event_Info {
	INT32 sessionId;
	wchar_t *data; // 识别结果、错误码、音量
	//SM_Speech_Internal_Engine internalEngine;//目前暂不生效
}SM_Speech_Event_Info;

//typedef void(__cdecl *SM_Speech_on_event)(SM_Speech_EVENT event, SM_SpeechMode mode, SM_Speech_Event_Info* info);
typedef void(__cdecl *SM_Speech_on_event)(int event, int mode, SM_Speech_Event_Info* info);
// error_code为0表示成功
typedef void(__cdecl *SM_Speech_OnDataUploaded)(int error_code, const char* data_id);

typedef struct _SM_Speech_Listener
{
	SM_Speech_on_event spOnResult;
	SM_Speech_OnDataUploaded dataUpLoadResult;
}*LPSM_Speech_Listener, SM_Speech_Listener;
/*
设备引擎的基本参数，包括识别语言，翻译语言 等
*/
typedef void(__stdcall *sm_config)(SM_Config* pConfig);

/*
注册语音回调接口
*/
typedef void(__stdcall *sm_regist_splistener)(LPSM_Speech_Listener pListener);

// 开启或关闭录音
typedef void(__cdecl *SM_Speech_EnableRecord)(bool shouldEnable);

typedef INT32(__stdcall *sm_start_speech)(int mode, BOOL longVoice, SM_Speech_EnableRecord enableRecord);

typedef enum _SM_Speed_RecordCBKind
{
	eRecordcCBKindOpen = 0,
	eRecordcCBKindData,
	eRecordcCBKindClose,
}SM_Speed_RecordCBKind;

// 写入录音数据，size不超过1600
typedef INT32(__stdcall *sm_write_audio)(SM_Speed_RecordCBKind cbfKind, unsigned char* buffer, unsigned int size, void* userdata);

/*
停止语音识别/翻译
*/
typedef void(__stdcall *sm_stop_speech)(INT32 session);

typedef void(__stdcall *sm_uninit)();

#define FUNC_SM_INIT "sm_init"
#define FUNC_SM_UNINIT "sm_uninit"
#define	FUNC_SM_ADAPTER "sm_adapter"
#define FUNC_SM_ACTIVE "sm_active"
#define FUNC_SM_CONFIG "sm_config"
#define FUNC_SM_SPEECH_LISTENER "sm_regist_splistener"
#define FUNC_SM_SPEECH_START "sm_start_speech"
#define FUNC_SM_SPEECH_STOP "sm_stop_speech"
#define FUNC_SM_SPEECH_WRITE_AUDIO "sm_write_audio"


static void smAdapterCallBackFunc(void *pData, int len, int tag) {

	sm_active smActiveFunc = (sm_active)GetProcAddress(smHandle, FUNC_SM_ACTIVE);
	if (!smActiveFunc)
		return;

	mSmActive = smActiveFunc(pData, len, tag);
	
	if (!mSmActive) {

		if (mInitCallBack)
			mInitCallBack(1);
	}
	else {
		if (mInitCallBack)
			mInitCallBack(0);
	}
}

static int __cdecl onSmAuthentication(void *pData, int len, int tag) {

	if (!pData)
		return 0;

	HINSTANCE adapterHandle = LoadLibrary(L"MAdpter.dll");
	if (!adapterHandle)
		return 0;

	sm_adapter smAdapterFunc = (sm_adapter)GetProcAddress(adapterHandle, FUNC_SM_ADAPTER);
	if (smAdapterFunc) {
		smAdapterFunc(pData, len, tag, smAdapterCallBackFunc);
	}

	return 1;
}

static void __cdecl onSmSpeechOnEvent(int event, int mode, SM_Speech_Event_Info* info) {

	if (mEsrListener && mEsrListener->onEvent) {
		mEsrListener->onEvent(event, mode, (EsrSpeechEventInfo *)info);
	}

	//if (eSpeechResult == event) {
	//	if (eOfflineRecognize == mode)
	//	{
	//		// 离线识别
	//		if (info) {
	//			if (info->data) {
	//				//	char * resultStr = wChar2Char(info->data);
	//				//	logger("result: %d %s \n", info->sessionId, resultStr);
	//				//	free(resultStr);
	//				OutputDebugString(info->data);
	//			}
	//		}
	//	}
	//	else if (eOfflineCommandRecognize == mode) {
	//		// 离线识别命令
	//	}
	//}
	//else if (eSpeechError == event) {

	//}
	//else if (eSpeechEnd == event) {

	//}
	//else if (eSpeechVolume == event) {
	//	if (eOfflineRecognize == mode)
	//	{
	//		// 离线识别
	//		if (info) {
	//			if (info->data) {
	//			//	logger("volume: %d %s \n", info->sessionId, info->data);
	//			}
	//		}
	//	}
	//}
}

static void __cdecl onSmSpeechOnDataUpload(int error_code, const char* data_id) {
	//if (!error_code) {
	//	logger("speechDataUpload: %s\n", data_id);
	//}
	//logger("speechDataUpload: %d %s\n", error_code, data_id);

	if (mEsrListener && mEsrListener->dataUpLoadResult) {
		mEsrListener->dataUpLoadResult(error_code, data_id);
	}
}

static INT32 onWriteAudio(SM_Speed_RecordCBKind cbfKind, unsigned char* buffer, unsigned int size) {

	if (!buffer || size <= 0)
		return 0;

	sm_write_audio smWriteAudioFunc = (sm_write_audio)GetProcAddress(smHandle, FUNC_SM_SPEECH_WRITE_AUDIO);
	if (!smWriteAudioFunc)
		return 0;

	return smWriteAudioFunc(cbfKind, buffer, size, NULL);
}

static INT32 onWriteAudioStart(unsigned char* buffer, unsigned int size) {
	return onWriteAudio(eRecordcCBKindOpen, buffer, size);
}

static INT32 onWriteAudioData(unsigned char* buffer, unsigned int size) {

	return onWriteAudio(eRecordcCBKindData, buffer, size);
}

static INT32 onWriteAudioStop(unsigned char* buffer, unsigned int size) {
	return onWriteAudio(eRecordcCBKindClose, buffer, size);
}

static void onSmUninit() {
	sm_uninit func = (sm_uninit)GetProcAddress(smHandle, FUNC_SM_UNINIT);
	if (func) {
		func();
	}
}

MSpeechEsr::MSpeechEsr()
{
}

MSpeechEsr::~MSpeechEsr()
{
}

void MSpeechEsr::init(initCallBack cb)
{
	smHandle = LoadLibrary(L"iflysmlogic.dll");
	if (!smHandle) {
		return;
	}

	mInitCallBack = cb;

	sm_init smInitFunc = (sm_init)GetProcAddress(smHandle, FUNC_SM_INIT);
	if (smInitFunc) {
		smInitFunc(onSmAuthentication);
	}
}

void MSpeechEsr::setConfig(EsrConfig * config, EsrSpeechListener * cb)
{
	if (mSmActive)
		return;

	SM_Config smConfig;
	if (config) {
		smConfig.esrPPROCPath = config->esrPPROCPath;
		smConfig.esrResPath = config->esrResPath;

		smConfig.frontTime = config->frontTime;
		smConfig.tailTime = config->tailTime;
	//	smConfig.language = config->language;
		smConfig.symbolMode = config->symbolMode;
	}

	mEsrListener = cb;

	sm_config smConfigFunc = (sm_config)GetProcAddress(smHandle, FUNC_SM_CONFIG);
	if (!smConfigFunc)
		return;

	smConfigFunc(&smConfig);

	SM_Speech_Listener listener;
	listener.dataUpLoadResult = onSmSpeechOnDataUpload;
	listener.spOnResult = onSmSpeechOnEvent;

	sm_regist_splistener smRegisterListener = (sm_regist_splistener)GetProcAddress(smHandle, FUNC_SM_SPEECH_LISTENER);
	if (smRegisterListener) {
		smRegisterListener(&listener);
	}

}

static void __cdecl onSmSpeechEnableRecord(bool shouldEnable) {
	if (shouldEnable) {
		//		logger("speech enable record \n");
		speechEnableFlag = true;
	}
}

int MSpeechEsr::startSpeech(int mode)
{
	if(mSmActive)
		return 0;

	sm_start_speech startSpeechFunc = (sm_start_speech)GetProcAddress(smHandle, FUNC_SM_SPEECH_START);
	if (!startSpeechFunc)
		return 0;

	mAudioSize = 0;
	return startSpeechFunc(mode, 1, onSmSpeechEnableRecord);
}

void MSpeechEsr::stopSpeech(int session)
{
	if (mSmActive)
		return;

	sm_stop_speech stopSpeechFunc = (sm_stop_speech)GetProcAddress(smHandle, FUNC_SM_SPEECH_STOP);
	if (!stopSpeechFunc)
		return;

	mAudioSize = 0;
	speechEnableFlag = false;
	stopSpeechFunc(session);
}

int MSpeechEsr::writeAudio(unsigned char * data, unsigned int size)
{
	if (mSmActive || !speechEnableFlag)
		return -1;

	int ret = 0;
	if (speechEnableFlag) {
		mAudioSize++;

		if (1 == mAudioSize) {
			ret = onWriteAudioStart(data, size);
		}
		else {
			ret = onWriteAudioData(data, size);
		}
	}
	return ret;
}

void MSpeechEsr::uninit()
{
	if (mSmActive)
		return;

	if (speechEnableFlag) {
		// 
	}

	onSmUninit();
}
