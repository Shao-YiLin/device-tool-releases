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
		MSPEECH_FRAMEWORK_EVENT_SINCHECK,
		MSPEECH_FRAMEWORK_EVENT_KEY_DOWN,
		MSPEECH_FRAMEWORK_EVENT_KEY_UP,
		MSPEECH_FRAMEWORK_EVENT_LASER,
		MSPEECH_FRAMEWORK_EVENT_EVENT_HEART = 0x09
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
		MSPEECH_FRAMEWORK_ACTION_CUSTOM_KEY,
		MSPEECH_FRAMEWORK_ACTION_SINCHECK,
		MSPEECH_FRAMEWORK_ACTION_START_OTA,
		MSPEECH_FRAMEWORK_ACTION_UPDATE_OTA,
		MSPEECH_FRAMEWORK_ACTION_END_OTA,
		MSPEECH_FRAMEWORK_ACTION_SET_POLLING_DATE,
		MSPEECH_FRAMEWORK_ACTION_GET_POLLING_DATE = 0x40,
		MSPEECH_FRAMEWORK_ACTION_SET_LED_INTENSITY,
		MSPEECH_FRAMEWORK_ACTION_SET_RGB_BACKLIHGHT,
		MSPEECH_FRAMEWORK_ACTION_SET_LASER,
		MSPEECH_FRAMEWORK_ACTION_SET_WHEEL = 0x44,
		MSPEECH_FRAMEWORK_ACTION_GET_WHEEL = 0x45,

		MSPEECH_FRAMEWORK_ACTION_SET_PLAYER_PLAY,
		MSPEECH_FRAMEWORK_ACTION_SET_PLAYER_VOLUME,
		MSPEECH_FRAMEWORK_ACTION_SET_HEART_RATE_TIME,
		MSPEECH_FRAMEWORK_ACTION_SET_KEYBOARD_CUSTOM_RGB = 0x49,
		MSPEECH_FRAMEWORK_ACTION_SET_ADAPTIVE_MODULATION = 0x50,
		MSPEECH_FRAMEWORK_ACTION_GET_ADAPTIVE_MODULATION = 0x51,

	} mspeech_framework_action_type;

	typedef enum
	{
		MSPEECH_FRAMEWORK_INFO_MOUSE_VERSION = 0x21,
		MSPEECH_FRAMEWORK_INFO_MOUSE_MACADDR,
		MSPEECH_FRAMEWORK_INFO_DONGLE_VERSION,
		MSPEECH_FRAMEWORK_INFO_DONGLE_MACADDR,
		MSPEECH_FRAMEWORK_INFO_MOUSE_PRODUCTNO,
		MSPEECH_FRAMEWORK_INFO_PROTOCOL_VERSION,
		MSPEECH_FRAMEWORK_INFO_DONGLE_PRODUCTNO,
		MSPEECH_FRAMEWORK_INFO_MOUSE_THEME,
		MSPEECH_FRAMEWORK_INFO_MOUSE_FLIP_PAGE,
		MSPEECH_FRAMEWORK_INFO_MOUSE_SUBTITLE_STATUS,
		MSPEECH_FRAMEWORK_INFO_MOUSE_WORK_MODE,
		MSPEECH_FRAMEWORK_INFO_MOUSE_CUSTOM_KEY,
		MSPEECH_FRAMEWORK_INFO_MOUSE_CONNECTION_TYPE,
		MSPEECH_FRAMEWORK_INFO_GET_LED_INTENSITY,
		MSPEECH_FRAMEWORK_INFO_RGB_BACKLIGHT,
		MSPEECH_FRAMEWORK_INFO_SLAVE_DEVICE_VERSION,
		MSPEECH_FRAMEWORK_INFO_PLAYER_VOLUME = 0x31,
		MSPEECH_FRAMEWORK_INFO_TIME_HEART_BEAT = 0x32,
		MSPEECH_FRAMEWORK_INFO_LS_DEVICE_ID = 0x33,
		MSPEECH_FRAMEWORK_INFO_LS_CHECK_SUM = 0x34,
	} mspeech_framework_info_type;

	typedef enum
	{
		MSPEECH_FRAMEWORK_DEVICE_TYPE_MOUSE = 0x31,
		MSPEECH_FRAMEWORK_DEVICE_TYPE_KEYBOARD = 0x32
	} mspeech_framework_device_type;



	typedef void(*mspeech_framework_write_cb_fn)(void * param, unsigned char *data, int size);

	/**
	 * @brief  音频数据回调函数
	 * @type 1: pcm 2: ico 3: opus
	 * @buffer: pcm音频数据
	 * @len: 音频数据长度
	 */
	typedef void(*mspeech_framework_audio_cb_fn)(void * param, unsigned char type, unsigned char channel, unsigned short *buffer, unsigned int len);

	/**
	 * @brief  鼠标状态回调
	 * @state:  1:鼠标dongle拔出
	 *          2:鼠标dongle插入　
	 *          3:鼠标和dongle连接　
	 *          4:鼠标和dongle断开　
	 *          5:语音按键按下 ： value 按键的index
	 *          6:语音按键释放 ： value 按键的index
	 * @retval
	 */
	typedef void(*mspeech_framework_event_cb_fn)(void * param, mspeech_framework_event_type state, unsigned int value);

	/**
	 * @brief  鼠标的命令反馈回调
	 * @action: 0x11:鼠标电量，value：鼠标电量的值
	 *          0x12:鼠标dpi, value: 鼠标dpi等级（０～４）
	 *          0x13:握手反馈，value: 1 成功，０ 失败
	 *          0x14:开始录音，value: 1 成功，０ 失败
	 *          0x15:停止录音，value: 1 成功，０ 失败
	 *          0x16:鉴权，value: 1 成功，０ 失败
	 */
	typedef void(*mspeech_framework_action_cb_fn)(void * param, mspeech_framework_action_type action, unsigned int value);

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
	typedef void(*mspeech_framework_info_cb_fn)(void * param, mspeech_framework_info_type info, unsigned char *buffer, unsigned int len);

	typedef struct
	{
		unsigned char r;
		unsigned char g;
		unsigned char b;
	} rgb_param;

	typedef struct
	{
		unsigned char index;
		unsigned char r;
		unsigned char g;
		unsigned char b;
	} rgb_param_keyboard;

	struct mspeech_framework
	{
		void *handle;
		unsigned char *mac;

		void(*init)(struct mspeech_framework *framework,
			mspeech_framework_write_cb_fn write_cb,
			mspeech_framework_event_cb_fn event_cb,
			mspeech_framework_action_cb_fn action_cb,
			mspeech_framework_audio_cb_fn audio_cb,
			mspeech_framework_info_cb_fn info_cb, void *param);

		void(*bind_mac)(struct mspeech_framework *framework, unsigned char *mac);

		void(*set_protocol_version)(struct mspeech_framework *framework, char version);

		void(*uninit)(struct mspeech_framework *framework);

		void(*set_dpi)(struct mspeech_framework *framework, unsigned int dpi_level);

		void(*get_dpi)(struct mspeech_framework *framework);

		void(*start_record)(struct mspeech_framework *framework);

		void(*stop_record)(struct mspeech_framework *framework);

		void(*get_battery)(struct mspeech_framework *framework);

		void(*get_device_version)(struct mspeech_framework *framework);

		void(*get_device_macaddr)(struct mspeech_framework *framework);

		void(*get_dongle_version)(struct mspeech_framework *framework);

		void(*get_dongle_macaddr)(struct mspeech_framework *framework);

		void(*handshake)(struct mspeech_framework *framework);

		void(*get_device_productNo)(struct mspeech_framework *framework);

		void(*get_protocol_version)(struct mspeech_framework *framework);

		void(*set_record_channel_size)(struct mspeech_framework *framework, int size);

		void(*set_sleep_time)(struct mspeech_framework *framework, int time);

		void(*append_data)(struct mspeech_framework *framework, unsigned char *data, unsigned int size);

		/**
		 * @brief  震动关闭
		 * @note
		 * @retval
		 */
		void(*vibration_close)(struct mspeech_framework *framework);

		/**
		 * @brief  震动报警设置
		 * @type   0: 所有  1：连接 2：语音
		 * @duration 震动时长（us）
		 * @retval
		 */
		void(*vibration_open)(struct mspeech_framework *framework, unsigned char type, unsigned int duration);


		/**
		 * @brief  立即震动
		 * @note
		 * @retval
		 */
		void(*start_vibration)(struct mspeech_framework *framework, unsigned int duration);

		/**
		 * @brief  设置LED灯的RGB值
		 * @type   1:常亮 2:语音 3:呼吸渐变 4:触发模式 5:充电
		 * @duration 每个颜色的间隔周期
		 * @data rbg颜色值数组
		 * @size rgb数组长度
		 * @retval
		 */
		void(*set_led_rgb)(struct mspeech_framework *framework, unsigned char type, unsigned int duration, rgb_param *data, int size);

		/**
		 * @brief  设置自定义鼠标按键
		 * @type    0：特殊功能按键 1：键盘按键 2：自定义按键 3：鼠标按键
		 * @key_index  1：key1索引 2：key2索引 ...
		 * @key_type 仅在type = 0/1时有效
		 * @data  normal key模式下：8字节的标准键盘协议数据格式;  consume key模式下：2字节的标准键盘
		 * @size
		 * @retval
		 */
		void(*set_custom_key)(struct mspeech_framework *framework, unsigned char type, unsigned char key_index, unsigned char key_type, unsigned char *data, unsigned int size);

		void(*start_sincheck)(struct mspeech_framework *framework);

		/**
	* 获取当前主题颜色值
	*/
		void(*get_theme)(struct mspeech_framework *framework);

		unsigned int(*get_version)(struct mspeech_framework *framework);


		/**
		 设置握手标记
		*/
		void(*set_handshake_info)(struct mspeech_framework *framework, unsigned int flag);

		/**
		 设置/获取M610lite左右键上下翻页功能
		*/
		void(*set_flip_page)(struct mspeech_framework *framework, unsigned int value);
		void(*get_flip_page)(struct mspeech_framework *framework);

		/*
		获取字幕上屏（侧键3开启状态）
		*/
		void(*get_subtitle_status)(struct mspeech_framework *framework);

		/*
		获取工作模式(mouse or laser pen)
		*/
		void(*get_work_mode)(struct mspeech_framework *framework);

		/*
		获取自定义按键参数(index为按键索引)
		*/
		void(*get_custom_key)(struct mspeech_framework *framework, unsigned char index);

		/*
		开启OTA(session长度为16字节的加密密钥, firmware为固件包：8字节包头加有效固件，size为固件包的长度)
		*/
		void(*start_ota)(struct mspeech_framework *framework, unsigned char *session, unsigned char *firmware, unsigned int size);

		/*
		结束OTA(value为1表示OTA以成功结束，否则OTA以失败结束{0：客户端强制终止OTA异常；2：超时异常；3：加密验证异常；4：硬件异常})
		*/
		void(*end_ota)(struct mspeech_framework *framework, unsigned int value);

		/*
		获取连接类型(ble/USB/2.4G)
		*/
		void(*get_connection_type)(struct mspeech_framework *framework);

		/**
		 * @brief  设置设备回报率
		 * @value  回报率(HZ)
		 * @retval
		 */
		void(*set_polling_rate)(struct mspeech_framework *framework, unsigned int value);

		/**
		 * @brief  获取设备回报率
		 * @retval
		 */
		void(*get_polling_rate)(struct mspeech_framework *framework);

		void(*get_dongle_product_no)(struct mspeech_framework *framework);

		void(*get_led_intensity)(struct mspeech_framework *framework, unsigned char index);

		void(*set_led_intensity)(struct mspeech_framework *framework, unsigned char index, unsigned char value);

		/**
		 * @brief  获取键盘灯光属性
		 * @index    0：灯光类型 1：灯光亮度 2：灯光速度
		 * @retval
		 */

		void(*get_rgb_backlight_param)(struct mspeech_framework *framework, unsigned char index);

		/**
		 * @brief  设置键盘灯光属性
		 * @index    0：灯光类型 1：灯光亮度 2：灯光速度
		 * @value  灯光类型0-17  灯光亮度0 85 170 255   灯光速度0-3
		 * @retval
		 */

		void(*set_rgb_backlight_param)(struct mspeech_framework *framework, unsigned char index, unsigned char value);

		/**
		* @brief  获取激光模式
		* @retval
		 */

		void(*get_laser_mode)(struct mspeech_framework *framework);

		/**
		 * @brief  设置激光模式
		 * @value  0:物理激光  1：数字激光
		 * @retval
		 */

		void(*set_laser_mode)(struct mspeech_framework *framework, unsigned char value);

		void(*get_slave_device_version)(struct mspeech_framework *framework);


		/**
		 * @brief  设置滚轮模式
		 * @value  0:音量  1：灯光
		 * @retval
		 */

		void(*set_wheel_mode)(struct mspeech_framework *framework, unsigned char value);

		void(*get_wheel_mode)(struct mspeech_framework *framework);


		/**
 * @brief  设置滚轮模式
 * @TYPE	OS_TYPE_WINDOWS = 0x1,
			OS_TYPE_ANDROID = 0x2,
			OS_TYPE_MAC = 0x3,
			OS_TYPE_IOS = 0x4
 * @retval
 */


		void(*set_os_type)(struct mspeech_framework* framework, unsigned int type);

		/**
		* @brief  开始心跳
		* @connect_status 默认为0
		* @retval
		*/
		void(*heart_beat)(struct mspeech_framework *framework, unsigned char connect_status);

		/**
		* @brief  播放音频
		* @tone_index  音频索引 范围为0-23
		* @retval
		*/

		void(*player_play)(struct mspeech_framework *framework, unsigned int tone_index);

		/**
		* @brief  设置音频播放音量大小
		* @volume  音量大小 范围为0-100
		* @retval
		*/
		void(*set_player_volume)(struct mspeech_framework *framework, unsigned int volume);

		/**
		* @brief  获取音频播放音量大小
		* @retval
		*/
		void(*get_player_volume)(struct mspeech_framework *framework);

		/**
		* @brief  设置心跳时间
		* @time  时间
		* @retval
		*/
		void(*set_time_heart_beat)(struct mspeech_framework *framework, unsigned int time);

		/**
		* @brief  获取心跳时间
		* @retval
		*/
		void(*get_time_heart_beat)(struct mspeech_framework *framework);

		/**
		* @brief  设置键盘自定义灯光
		* @number  数量
		* @data    灯光属性
		*/
		void(*set_keyboard_custom_rgb)(struct mspeech_framework *framework, unsigned int number, rgb_param_keyboard *data);
		/**
		* @brief  设置自适应调频
		* @value  1 开启  0 关闭
		*/
		void(*set_adaptive_modulation)(struct mspeech_framework *framework, unsigned int value);
		/**
		* @brief  获取自适应调频
		*/
		void(*get_adaptive_modulation)(struct mspeech_framework *framework);

		/**
		* @brief 获取LS芯片device_id
		*/
		void(*get_ls_device_id)(struct mspeech_framework *framework);


		/**
		* @brief  下发UTC时间戳,获取ls check sum
		* @time  时间戳
		* @retval
		*/
		void(*get_ls_check_sum)(struct mspeech_framework *framework, unsigned int time);



	};

	DllExport void mspeech_framework_construct(struct mspeech_framework *framework);

#ifdef __cplusplus
}
#endif