#pragma once
#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

#define DllImport __declspec(dllimport)
#define DllExport __declspec(dllexport)

	typedef enum
	{
		MSPEECH_FRAMEWORK_EVENT_LEFT = 0x1,
		MSPEECH_FRAMEWORK_EVENT_ARRIVED,
		MSPEECH_FRAMEWORK_EVENT_BLE_CONNECTED,
		MSPEECH_FRAMEWORK_EVENT_BLE_DISCONNECTED,
		MSPEECH_FRAMEWORK_EVENT_KEY_DOWN,
		MSPEECH_FRAMEWORK_EVENT_KEY_UP
	} mspeech_framework_event_type;

	typedef enum
	{
		MSPEECH_FRAMEWORK_ACTION_BATTERY = 0x11,
		MSPEECH_FRAMEWORK_ACTION_DPI,
		MSPEECH_FRAMEWORK_ACTION_SET_DPI,
		MSPEECH_FRAMEWORK_ACTION_HANDSHAKE,
		MSPEECH_FRAMEWORK_ACTION_START_RECORD,
		MSPEECH_FRAMEWORK_ACTION_STOP_RECORD,
		MSPEECH_FRAMEWORK_ACTION_AUTHENTICATION,
		MSPEECH_FRAMEWORK_ACTION_LED_RGB,
		MSPEECH_FRAMEWORK_ACTION_VIBRATION,
		MSPEECH_FRAMEWORK_ACTION_CUSTOM_KEY
	} mspeech_framework_action_type;

	typedef enum
	{
		MSPEECH_FRAMEWORK_INFO_MOUSE_VERSION = 0x21,
		MSPEECH_FRAMEWORK_INFO_MOUSE_MACADDR,
		MSPEECH_FRAMEWORK_INFO_DONGLE_VERSION,
		MSPEECH_FRAMEWORK_INFO_DONGLE_MACADDR,
		MSPEECH_FRAMEWORK_INFO_MOUSE_PRODUCTNO,
		MSPEECH_FRAMEWORK_INFO_PROTOCOL_VERSION,
		MSPEECH_FRAMEWORK_INFO_DONGLE_PRODUCTNO
	} mspeech_framework_info_type;

	typedef enum
	{
		MSPEECH_FRAMEWORK_DEVICE_TYPE_MOUSE = 0x31,
		MSPEECH_FRAMEWORK_DEVICE_TYPE_KEYBOARD = 0x32
	} mspeech_framework_device_type;

	typedef void (*mspeech_framework_write_cb_fn)(void * param, unsigned char *data, int size);

	/**
	 * @brief  音频数据回调函数
	 * @buffer: pcm音频数据
	 * @len: 音频数据长度
	 */
	typedef void (*mspeech_framework_audio_cb_fn)(void * param, unsigned char channel, unsigned short *buffer, unsigned int len);

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
	typedef void (*mspeech_framework_event_cb_fn)(void * param, mspeech_framework_event_type state, unsigned int value);

	/**
	 * @brief  鼠标的命令反馈回调
	 * @action: 0x11:鼠标电量，value：鼠标电量的值
	 *          0x12:鼠标dpi, value: 鼠标dpi等级（０～４）
	 *          0x13:握手反馈，value: 1 成功，０ 失败
	 *          0x14:开始录音，value: 1 成功，０ 失败
	 *          0x15:停止录音，value: 1 成功，０ 失败
	 *          0x16:鉴权，value: 1 成功，０ 失败
	 */
	typedef void (*mspeech_framework_action_cb_fn)(void * param, mspeech_framework_action_type action, unsigned int value);

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
	typedef void (*mspeech_framework_info_cb_fn)(void * param, mspeech_framework_info_type info, unsigned char *buffer, unsigned int len);

	typedef struct
	{
		unsigned char r;
		unsigned char g;
		unsigned char b;
	} rgb_param;

	struct mspeech_framework
	{
		void *handle;
		unsigned char *mac;

		void (*init)(struct mspeech_framework *framework,
					 mspeech_framework_write_cb_fn write_cb,
					 mspeech_framework_event_cb_fn event_cb,
					 mspeech_framework_action_cb_fn action_cb,
					 mspeech_framework_audio_cb_fn audio_cb,
					 mspeech_framework_info_cb_fn info_cb, void *param);

		void (*bind_mac)(struct mspeech_framework *framework, unsigned char *mac);

		void(*set_protocol_version)(struct mspeech_framework *framework, char version);

		void (*uninit)(struct mspeech_framework *framework);

		void (*set_dpi)(struct mspeech_framework *framework, unsigned int dpi_level);

		void (*get_dpi)(struct mspeech_framework *framework);

		void (*start_record)(struct mspeech_framework *framework);

		void (*stop_record)(struct mspeech_framework *framework);

		void (*get_battery)(struct mspeech_framework *framework);

		void (*get_device_version)(struct mspeech_framework *framework);

		void (*get_device_macaddr)(struct mspeech_framework *framework);

		void (*get_dongle_version)(struct mspeech_framework *framework);

		void (*get_dongle_macaddr)(struct mspeech_framework *framework);

		void (*handshake)(struct mspeech_framework *framework);

		void (*get_device_productNo)(struct mspeech_framework *framework);

		void (*get_protocol_version)(struct mspeech_framework *framework);

		void (*set_record_channel_size)(struct mspeech_framework *framework, int size);

		void (*set_sleep_time)(struct mspeech_framework *framework, int time);

		void (*append_data)(struct mspeech_framework *framework, unsigned char *data, unsigned int size);

/**
 * @brief  震动关闭
 * @note   
 * @retval 
 */
		void (*vibration_close)(struct mspeech_framework *framework);

/**
 * @brief  震动报警设置
 * @type   0: 所有  1：连接 2：语音
 * @duration 震动时长（us）
 * @retval 
 */
		void (*vibration_open)(struct mspeech_framework *framework, unsigned char type, unsigned int duration);


/**
 * @brief  立即震动
 * @note   
 * @retval 
 */
		void (*start_vibration)(struct mspeech_framework *framework, unsigned int duration);

/**
 * @brief  设置LED灯的RGB值
 * @type   1:充电 2:语音 3:呼吸渐变
 * @duration 每个颜色的间隔周期
 * @data rbg颜色值数组
 * @size rgb数组长度
 * @retval 
 */
		void (*set_led_rgb)(struct mspeech_framework *framework, unsigned char type, unsigned int duration, rgb_param *data, int size);

/**
 * @brief  设置自定义鼠标按键
 * @type    1：标准按键 2：自定义按键
 * @key_index  1：key1索引 2：key2索引 ...
 * @key_type 0: normal key 1: consume key
 * @data  normal key模式下：8字节的标准键盘协议数据格式;  consume key模式下：2字节的标准键盘
 * @size 
 * @retval 
 */
		void (*set_custom_key)(struct mspeech_framework *framework, unsigned char type, unsigned char key_index, unsigned char key_type, unsigned char *data, unsigned int size);
	};

	DllExport void mspeech_framework_construct(struct mspeech_framework *framework);

#ifdef __cplusplus
}
#endif