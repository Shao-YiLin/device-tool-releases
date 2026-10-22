#pragma once

typedef enum {
	eOnlineRecognize = 0,   //在线语音识别
	eOfflineRecognize,		//离线语音识别,暂不支持
	eOfflineCommandRecognize,//离线命令词识别，暂不支持
	eAIUIRecognize,			//在线命令词和语义aiui识别，暂不支持
	eTranslate,			//语音翻译
	eMixRecognize,			//混合模式，无网络时,弱网络时，切换到离线， 暂不支持
}EsrSpeechMode;

typedef enum
{
	eSmartAdd = 0, // 智能添加
	eIgnoreEnd,	// 省略句末
	eIgnoreAll,	// 省略所有
	eReplaceWithSpace,	// 空格替代标点
}EsrSymbolMode;

typedef struct {
	int symbolMode = eSmartAdd;
	int frontTime = 3000;                          //前端点时间，单位ms
	int tailTime = 2000;                           //后端点时间，单位ms
	// 离线命令配置
	const char* esrResPath = 0;	 				// 离线识别资源目录 (语法文件是grammar.txt,GBK格式,放在该目录下)。保留字段，暂时无用
	const char* esrPPROCPath = 0; 				// 离线识别工作目录 （用于保存相关日志）。保留字段，暂时无用
}EsrConfig;

typedef enum {
	eSpeechResult, // 语音结果
	eSpeechError, // 错误
	eSpeechEnd,	// 结束（AIUI无此状态）
	eSpeechVolume,	// 音量更新
}EsrSpeechEVENT;

/*
	语音识别错误码
	805001 网络错误
	805002 长语音，超出内存限制
	805003 无结果
	805004 无网络
	805005 初始化错误
	805013 超时

	听写识别格式见：https://doc.xfyun.cn/msc_windows/识别结果.html
	AIUI识别结果格式见https://doc.iflyos.cn/aiui/sdk/more_doc/semantic_agreement/summary.html#消息格式概览
*/
typedef struct {
	signed int sessionId;
	wchar_t *data; // 识别结果、错误码、音量
}EsrSpeechEventInfo;

typedef void(*EsrSpeechOnEvent)(int event, int mode, EsrSpeechEventInfo* info);
// error_code为0表示成功
typedef void(*EsrSpeechOnDataUploaded)(int error_code, const char* data_id);

typedef struct
{
	EsrSpeechOnEvent onEvent;
	EsrSpeechOnDataUploaded dataUpLoadResult;
}EsrSpeechListener;

/**
* state : 1 success 0: fail
*/
typedef void(*initCallBack)(int state);

class MSpeechEsr {

public:
	 MSpeechEsr();
	 ~MSpeechEsr();

public:
	void init(initCallBack cb);
	void setConfig(EsrConfig *config, EsrSpeechListener *cb);
	int startSpeech(int mode);
	void stopSpeech(int session);
	int writeAudio(unsigned char *data, unsigned int size);
	void uninit();
};