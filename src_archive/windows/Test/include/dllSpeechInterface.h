#pragma once


#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#define DllImport   __declspec( dllimport )
#define DllExport   __declspec( dllexport )

	//typedef enum
	//{
	//	LIBMOUSE_EVENT_LEFT = 0x1,
	//	LIBMOUSE_EVENT_ARRIVED,
	//	LIBMOUSE_EVENT_BLE_CONNECTED,
	//	LIBMOUSE_EVENT_BLE_DISCONNECTED,
	//	LIBMOUSE_EVENT_KEY_DOWN,
	//	LIBMOUSE_EVENT_KEY_UP
	//} libspeech_event_type;
	//
	//typedef enum
	//{
	//	LIBMOUSE_ACTION_BATTERY = 0x11,
	//	LIBMOUSE_ACTION_DPI,
	//	LIBMOUSE_ACTION_SET_DPI,
	//	LIBMOUSE_ACTION_HANDSHAKE,
	//	LIBMOUSE_ACTION_START_RECORD,
	//	LIBMOUSE_ACTION_STOP_RECORD,
	//	LIBMOUSE_ACTION_AUTHENTICATION
	//} libspeech_action_type;
	//
	//typedef enum
	//{
	//	LIBMOUSE_INFO_MOUSE_VERSION = 0x21,
	//	LIBMOUSE_INFO_MOUSE_MACADDR,
	//	LIBMOUSE_INFO_DONGLE_VERSION,
	//	LIBMOUSE_INFO_DONGLE_MACADDR,
	//	LIBMOUSE_INFO_MOUSE_PRODUCTNO,
	//	LIBMOUSE_INFO_PROTOCOL_VERSION,
	//	LIBMOUSE_INFO_DONGLE_PRODUCTNO
	//} libspeech_info_type;
	//
	//typedef enum
	//{
	//	LIBMOUSE_DEVICE_TYPE_MOUSE = 0x31,
	//	LIBMOUSE_DEVICE_TYPE_KEYBOARD = 0x32
	//}libspeech_device_type;

	struct DeviceInfo
	{
		unsigned short vendorId;
		unsigned short productId;
		unsigned char deviceAddress;
		unsigned char deviceType;
	};

	/**
	 * @brief  音频数据回调函数
	 * @buffer: pcm音频数据
	 * @len: 音频数据长度
	 */
	typedef void(*libspeechAudioListener)(unsigned char deviceAddress, unsigned char channel, unsigned short *buffer, unsigned int len);

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
	typedef void(*libspeechEventListener)(unsigned char deviceAddress, unsigned char state);

	/**
	 * @brief  鼠标的命令反馈回调
	 * @action: 0x11:鼠标电量，value：鼠标电量的值
	 *          0x12:鼠标dpi, value: 鼠标dpi等级（０～４）
	 *          0x13:握手反馈，value: 1 成功，０ 失败
	 *          0x14:开始录音，value: 1 成功，０ 失败
	 *          0x15:停止录音，value: 1 成功，０ 失败
	 *          0x16:鉴权，value: 1 成功，０ 失败
	 */
	typedef void(*libspeechActionListener)(unsigned char deviceAddress, unsigned char action, unsigned int value);

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
	typedef void(*libspeechInfoListener)(unsigned char deviceAddress, unsigned char info, unsigned char *buffer, unsigned int len);

	DllExport void scanDevice(void(*scan_cb)(struct DeviceInfo *data, int size));

	DllExport void init();

	DllExport void uninit();

	DllExport void setListener(libspeechEventListener event_cb,
		libspeechActionListener action_cb,
		libspeechAudioListener audio_cb,
		libspeechInfoListener info_cb);

	DllExport int createDevice(unsigned char deviceAddress);

	DllExport int destroyDevice(unsigned char deviceAddress);

	DllExport void setDpi(unsigned char deviceAddress, unsigned int dpiLevel);

	DllExport void getDpi(unsigned char deviceAddress);

	DllExport void startRecord(unsigned char deviceAddress);

	DllExport void stopRecord(unsigned char deviceAddress);

	DllExport void getBattery(unsigned char deviceAddress);

	DllExport void getDeviceVersion(unsigned char deviceAddress);

	DllExport void getDeviceMacAddress(unsigned char deviceAddress);

	DllExport void getDongVersion(unsigned char deviceAddress);

	DllExport void getDongleMacAddress(unsigned char deviceAddress);

	DllExport void handShake(unsigned char deviceAddress);

	DllExport void getDeviceProductNo(unsigned char deviceAddress);

	DllExport void getProtocolVersion(unsigned char deviceAddress);

	DllExport void setRecordChannelSize(unsigned char deviceAddress,int size);

	DllExport void startAuthentication(unsigned char deviceAddress);

#ifdef __cplusplus
}
#endif
