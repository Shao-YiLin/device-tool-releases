#include <string.h>
#include <map>
#include <stdio.h>
#include <malloc.h>
#include <Windows.h>
#include <list>

#include "dllSpeechInterface.h"
#include "include/libspeech_device_manager.h"
#include "include/lib_device_define.h"
#include "include/libspeech_voice.h"

struct voice_param {

	unsigned char deviceAddress;
	unsigned char channel;
	unsigned char data[40];
	unsigned int len;
};

struct voice_handle {
	struct libspeech_voice *channel1;
	struct libspeech_voice *channel2;
};

typedef void(*deviceScanListener)(struct DeviceInfo *data, int size);

static deviceScanListener scanListener = NULL;
static DeviceInfo deviceArray[10] = { 0 };
static int deviceSize = 0;

static std::map<unsigned char, struct libspeech_device_manager *> deviceHandleMap;
static std::map<unsigned char, struct libmouse_device_info *> deviceMap;
static std::map<unsigned char, struct voice_handle *> voiceHandleMap;

static libspeechEventListener eventListener;
static libspeechActionListener actionListener;
static libspeechAudioListener audioListener;
static libspeechInfoListener infoListener;

static CRITICAL_SECTION lock;
static void * voice_pthread_handle;
static std::list<struct voice_param *> voiceDataList;

static void _scanCallback(struct libmouse_device_info *data, int size) {

	if (NULL == scanListener)
		return;

	if (NULL == data || size < 1)
		return;

	memset(deviceArray, 0x0, sizeof(deviceArray));

	int i;
	for ( i = 0; i < size; i++)
	{
		if (NULL != (data + i)) {
			struct libmouse_device_info ele = *(data + i);
			deviceArray[deviceSize].productId = ele.product_id;
			deviceArray[deviceSize].vendorId = ele.vendor_id;
			deviceArray[deviceSize].deviceAddress = ele.device_address;
			deviceArray[deviceSize].deviceType = ele.device_type;
			deviceSize++;
			/*struct libmouse_device_info *info = (struct libmouse_device_info *)malloc(sizeof(struct libmouse_device_info));*/
			struct libmouse_device_info *info = new libmouse_device_info();
			memcpy(info, &ele, sizeof(struct libmouse_device_info));
			deviceMap.insert(std::pair<unsigned char, struct libmouse_device_info *>(info->device_address, info));
		}
	}

	if (NULL != scanListener) {
		scanListener(deviceArray, deviceSize);
	}
}

void scanDevice(void(*scan_cb)(struct DeviceInfo *data, int size)) {
	scanListener = scan_cb;
	libdevice_scan_device(_scanCallback);
}

void setListener(libspeechEventListener event_cb,
	libspeechActionListener action_cb,
	libspeechAudioListener audio_cb,
	libspeechInfoListener info_cb) {

	eventListener = event_cb;
	actionListener = action_cb;
	audioListener = audio_cb;
	infoListener = info_cb;
}

static struct libmouse_device_info *getDevice(unsigned char deviceAddress) {

	if (deviceMap.empty())
		return NULL;

	std::map<unsigned char, struct libmouse_device_info *>::iterator ite = deviceMap.find(deviceAddress);
	if (ite != deviceMap.end()) {
		return ite->second;
	}
	return NULL;
}

static void removeDevice(unsigned char deviceAddress) {

	if (deviceMap.empty())
		return ;

	std::map<unsigned char, struct libmouse_device_info *>::iterator ite = deviceMap.find(deviceAddress);
	if (ite != deviceMap.end()) {
		struct libmouse_device_info * data = ite->second;
		deviceMap.erase(ite);
	}
}

static struct libspeech_device_manager *getDeviceHandle(unsigned char deviceAddress) {

	if (deviceHandleMap.empty())
		return NULL;

	std::map<unsigned char, struct libspeech_device_manager *>::iterator ite = deviceHandleMap.find(deviceAddress);
	if (ite != deviceHandleMap.end()) {
		return ite->second;
	}
	return NULL;
}

static void removeDeviceHandle(unsigned char deviceAddress) {

	if (deviceHandleMap.empty())
		return;

	std::map<unsigned char, struct libspeech_device_manager *>::iterator ite = deviceHandleMap.find(deviceAddress);
	if (ite != deviceHandleMap.end()) {
		deviceHandleMap.erase(ite);
	}
}

static void addDeviceHandler(unsigned char deviceAddress, struct libspeech_device_manager *manager) {
	
	deviceHandleMap.insert(std::pair<unsigned char, struct libspeech_device_manager *>(deviceAddress, manager));
}

static struct voice_handle* getVoiceHandle(unsigned char deviceAddress) {

	if (voiceHandleMap.empty())
		return NULL;
	std::map<unsigned char, struct voice_handle*>::iterator ite = voiceHandleMap.find(deviceAddress);
	if (ite != voiceHandleMap.end())
		return ite->second;
	return NULL;
}

static void removeVoiceHandle(unsigned char deviceAddress) {
	if (voiceHandleMap.empty())
		return;

	std::map<unsigned char, struct voice_handle *>::iterator ite = voiceHandleMap.find(deviceAddress);
	if (ite != voiceHandleMap.end()) {
		voiceHandleMap.erase(ite);
	}
}

static void addVoiceHandle(unsigned char deviceAddress, struct voice_handle *voice) {
	voiceHandleMap.insert(std::pair<unsigned char, struct voice_handle *>(deviceAddress, voice));
}

static void putVoiceData(unsigned char deviceAddress, unsigned char channel, unsigned char *data, unsigned int len) {
	
	EnterCriticalSection(&lock);
	struct voice_param *param = new voice_param();
	memset(param, 0, sizeof(struct voice_param));
	param->channel = channel;
	param->deviceAddress = deviceAddress;
	memcpy(param->data, data, len);
	param->len = len;
	
	voiceDataList.push_back(param);
	LeaveCriticalSection(&lock);
}

static struct voice_param *popVoiceData() {
	struct voice_param *param = NULL;
	/*EnterCriticalSection(&lock);*/
	if (voiceDataList.size() > 0) {
		//printf("remove voice \n");
		std::list<struct voice_param*>::iterator ite = voiceDataList.begin();
		param = *ite;
		voiceDataList.erase(ite++);
		
	}
	//LeaveCriticalSection(&lock);
	return param;
}

/**
 * @brief  音频数据回调函数
 * @buffer: pcm音频数据
 * @len: 音频数据长度
 */
void _libmouse_audio_cb(struct libmouse_device_info *device, unsigned char channel, unsigned short *buffer, unsigned int len) {

	if (NULL == audioListener)
		return;

	putVoiceData(device->device_address, channel, (unsigned char*)buffer, len);

//	audioListener(device->device_address, channel, buffer, len);
}

/**
 * @brief  鼠标状态回调
 * @state:  1:鼠标dongle拔出
 *          2:鼠标dongle插入　
 *          3:鼠标和dongle连接　
 *          4:鼠标和dongle断开　
 *          5:语音按键按下
 *          6:语音按键释放
 * @retval
 */
static void _libmouse_event_cb(struct libmouse_device_info *device, libmouse_event_type state) {

	if (NULL == eventListener)
		return;

	eventListener(device->device_address, state);
}

/**
 * @brief  鼠标的命令反馈回调
 * @action: 0x11:鼠标电量，value：鼠标电量的值
 *          0x12:鼠标dpi, value: 鼠标dpi等级（０～４）
 *          0x13:握手反馈，value: 1 成功，０ 失败
 *          0x14:开始录音，value: 1 成功，０ 失败
 *          0x15:停止录音，value: 1 成功，０ 失败
 *          0x16:鉴权，value: 1 成功，０ 失败
 */
static void _libmouse_action_cb(struct libmouse_device_info *device, libmouse_action_type action, unsigned int value) {

	if (NULL == actionListener)
		return;

	actionListener(device->device_address, action, value);
}

/**
 * @brief  鼠标扩展命令反馈回调
 * @info: 0x21:鼠标版本号 （4字节的版本号）
 *          0x22:鼠标mac地址（4字节的版本号）
 *          0x23:dongle版本号（4字节的版本号）
 *          0x24:dongle的mac地址（4字节的版本号）
 *          0X25:设备产品系列（4字节的版本号）
 *              1) 0x22:复刻版
*				2) 0x23:Pro版
*				3) 0x24:Lite版
*           0x26:设备协议版本号 （4字节的版本号）
*           0x27:dongle产品系列
*               1) 0x10:通用
*				2) 0x11:键鼠
*				3) 0x12:鼠标
*				4) 0x13:键盘
*				5) 0x14:无线麦克风
*				6) 0x15:录音笔
 * @retval
 */
static void _libmouse_info_cb(struct libmouse_device_info *device, libmouse_info_type info, unsigned char *buffer, unsigned int len) {

	if (NULL == infoListener)
		return;

	infoListener(device->device_address, info, buffer, len);
}

static void _createVoiceHandle(unsigned char deviceAddress) {
	struct voice_handle *handle = getVoiceHandle(deviceAddress);
	if (NULL != handle)
		return;

	struct voice_handle *voiceHandle = new voice_handle();
	voiceHandle->channel1 = new libspeech_voice();
	voiceHandle->channel2 = new libspeech_voice();
	libspeech_voice_construct(voiceHandle->channel1);
	voiceHandle->channel1->init(voiceHandle->channel1);

	libspeech_voice_construct(voiceHandle->channel2);
	voiceHandle->channel2->init(voiceHandle->channel2);
	addVoiceHandle(deviceAddress, voiceHandle);
}

static void _destroyVoiceHandle(unsigned char deviceAddress) {
	struct voice_handle *handle = getVoiceHandle(deviceAddress);
	if (NULL == handle)
		return;

	if (handle->channel1) {
		handle->channel1->uninit(handle->channel1);
		delete handle->channel1;
		handle->channel1 = NULL;
	}

	if (handle->channel2) {
		handle->channel2->uninit(handle->channel2);
		delete handle->channel2;
		handle->channel2 = NULL;
	}

	delete handle;
	handle = NULL;
	removeVoiceHandle(deviceAddress);
}

 int createDevice(unsigned char deviceAddress) {

	 struct libmouse_device_info *device = getDevice(deviceAddress);
	 if (NULL == device)
		 return 0;

	 struct libspeech_device_manager *lastManager = getDeviceHandle(deviceAddress);
	 if (NULL != lastManager)
		 return 1;

	 _createVoiceHandle(deviceAddress);

	 struct libspeech_device_manager *manager = new libspeech_device_manager();

	// manager = (struct libspeech_device_manager *)malloc(sizeof(struct libspeech_device_manager));
	 libspeech_device_manager_construct(manager);
	 addDeviceHandler(deviceAddress, manager);

	 manager->init(manager, _libmouse_event_cb, _libmouse_action_cb, _libmouse_audio_cb, _libmouse_info_cb);

	 return manager->open_device(manager, device);
}

 int destroyDevice(unsigned char deviceAddress) {

	 struct libmouse_device_info *device = getDevice(deviceAddress);
	 if (NULL == device)
		 return 0;

	 _destroyVoiceHandle(deviceAddress);

	 struct libspeech_device_manager *manager = getDeviceHandle(deviceAddress);
	 if (NULL != manager) {
		 manager->close_device(manager);
		 manager->uninit(manager);
		 delete manager;
		 manager = NULL;
		 removeDeviceHandle(deviceAddress);
		 return 1;
	 }
	 return 0;
}

 static DWORD WINAPI __voice_thread_func(LPVOID param) {


	 while (true) {
		 EnterCriticalSection(&lock);
		 if (voiceDataList.size() == 0) {
			 LeaveCriticalSection(&lock);
			 Sleep(1);
			 continue;
		 }

		 unsigned short pcm_buffer[320];
		 unsigned short pcm_size;
		struct voice_param *data = popVoiceData();
		if (NULL != data) {
			struct voice_handle *voice = getVoiceHandle(data->deviceAddress);
			if (NULL != voice) {
				if (data->channel == 1) {
					if (data->len == 40 && NULL != voice->channel2) {
						if (voice->channel2->decode(voice->channel2, (unsigned short*)data->data, pcm_buffer, &pcm_size)) {
							if (audioListener)
								audioListener(data->deviceAddress, data->channel, pcm_buffer, pcm_size);
						}
					}
				}
				else {
					if (data->len == 40 && NULL != voice->channel1) {
						if (voice->channel1->decode(voice->channel1, (unsigned short*)data->data, pcm_buffer, &pcm_size)) {
							if (audioListener)
								audioListener(data->deviceAddress, data->channel, pcm_buffer, pcm_size);
						}
					}
				}
			}
			else {
				printf("voice handle is null \n");
			}
		}
		else {
			printf("data is NULL \n");
		}
		 LeaveCriticalSection(&lock);
	 }

	return 0;
}

 void init() {

	InitializeCriticalSection(&lock);
	voiceDataList.clear();
	voiceHandleMap.clear();
	deviceMap.clear();
	deviceHandleMap.clear();
	LPDWORD voiceThreadId = NULL;
	voice_pthread_handle = CreateThread(NULL, 0, __voice_thread_func, NULL, 0, voiceThreadId);
 }

 void uninit() {

	 voiceDataList.clear();
	 voiceHandleMap.clear();
	 deviceMap.clear();
	 deviceHandleMap.clear();

	 if (voice_pthread_handle) {
 		CloseHandle(voice_pthread_handle);
 		voice_pthread_handle = 0;
	 }

	 DeleteCriticalSection(&lock);
 }

 void setDpi(unsigned char deviceAddress, unsigned int dpiLevel) {

	 struct libspeech_device_manager *manager = getDeviceHandle(deviceAddress);
	 if (NULL == manager)
		 return;

	 manager->set_dpi(manager, dpiLevel);
}

 void getDpi(unsigned char deviceAddress) {

	 struct libspeech_device_manager *manager = getDeviceHandle(deviceAddress);
	 if (NULL == manager)
		 return;

	 manager->get_dpi(manager);
}

 void startRecord(unsigned char deviceAddress) {

	 struct libspeech_device_manager *manager = getDeviceHandle(deviceAddress);
	 if (NULL == manager)
		 return;

	 manager->start_record(manager);
}

 void stopRecord(unsigned char deviceAddress) {

	 struct libspeech_device_manager *manager = getDeviceHandle(deviceAddress);
	 if (NULL == manager)
		 return;

	 manager->stop_record(manager);
}

 void getBattery(unsigned char deviceAddress) {

	 struct libspeech_device_manager *manager = getDeviceHandle(deviceAddress);
	 if (NULL == manager)
		 return;

	 manager->get_battery(manager);
}

 void getDeviceVersion(unsigned char deviceAddress) {

	 struct libspeech_device_manager *manager = getDeviceHandle(deviceAddress);
	 if (NULL == manager)
		 return;

	 manager->get_device_version(manager);
}

 void getDeviceMacAddress(unsigned char deviceAddress) {

	 struct libspeech_device_manager *manager = getDeviceHandle(deviceAddress);
	 if (NULL == manager)
		 return;

	 manager->get_device_macaddr(manager);
}

 void getDongVersion(unsigned char deviceAddress) {

	 struct libspeech_device_manager *manager = getDeviceHandle(deviceAddress);
	 if (NULL == manager)
		 return;

	 manager->get_dongle_version(manager);
}

 void getDongleMacAddress(unsigned char deviceAddress) {

	 struct libspeech_device_manager *manager = getDeviceHandle(deviceAddress);
	 if (NULL == manager)
		 return;

	 manager->get_dongle_macaddr(manager);
 }

 void handShake(unsigned char deviceAddress) {

	 struct libspeech_device_manager *manager = getDeviceHandle(deviceAddress);
	 if (NULL == manager)
		 return;

	 manager->handshake(manager);
 }

 void getDeviceProductNo(unsigned char deviceAddress) {

	 struct libspeech_device_manager *manager = getDeviceHandle(deviceAddress);
	 if (NULL == manager)
		 return;
	 manager->get_device_productNo(manager);
}

 void getProtocolVersion(unsigned char deviceAddress) {

	 struct libspeech_device_manager *manager = getDeviceHandle(deviceAddress);
	 if (NULL == manager)
		 return;

	 manager->get_protocol_version(manager);
}

 void setRecordChannelSize(unsigned char deviceAddress, int size) {

//	 printf("setRecordChannelSize");

	 struct libspeech_device_manager *manager = getDeviceHandle(deviceAddress);
	 if (NULL == manager)
		 return;

	 manager->set_record_channel_size(manager, size);
}

 void startAuthentication(unsigned char deviceAddress) {

	 struct libspeech_device_manager *manager = getDeviceHandle(deviceAddress);
	 if (NULL == manager)
		 return;

	 manager->start_authentication(manager);
}