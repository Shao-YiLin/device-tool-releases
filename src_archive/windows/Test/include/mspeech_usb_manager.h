#pragma once
#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#define DllImport   __declspec( dllimport )
#define DllExport   __declspec( dllexport )
#define PART_FIRMWARE_SIZE 32

	enum MSPEECH_USB_MANAGERE_EVENT_TYPE
	{
		MSPEECH_USB_MANAGERE_EVENT_TRANSPOND = 0x40,
		MSPEECH_USB_MANAGERE_EVENT_SCAN,
		MSPEECH_USB_MANAGERE_EVENT_CONNECTED,
		MSPEECH_USB_MANAGERE_EVENT_DISCONNECT,
		MSPEECH_USB_MANAGERE_EVENT_START_OTA,
		MSPEECH_USB_MANAGERE_EVENT_UPDATE_OTA,
		MSPEECH_USB_MANAGERE_EVENT_END_OTA,
		MSPEECH_USB_MANAGERE_EVENT_PRODUCT_NO,
		MSPEECH_USB_MANAGERE_EVENT_APPEND,
	};

	enum USB_DEVICE_TYPE
	{
		SINGLE_DONGLE = 1,
		MULTI_DONGLE = 2,
	};

	typedef void(*event_callback)(void *param, unsigned char type, unsigned char *data, unsigned int size);
	typedef void(*write_callback)(void *param, unsigned char *data, unsigned int size);

	struct mspeech_usb_event_data
	{
		unsigned char version;
		unsigned char state;
		unsigned char mac[6];
		unsigned char data[128];
		unsigned int len;
	};

	struct mspeech_usb_scan_data
	{
		unsigned char version;
		unsigned char rssi;
		unsigned char mac[6];
		unsigned char data[128];
		unsigned int len;
	};

	struct mspeech_usb_write_data {
		unsigned char mac[6];
		unsigned char data[128];
		unsigned int len;
	};

	struct mspeech_usb_manager {
		void *handler;
		void *ota_param;
		void *base;
		event_callback event_cb;
		write_callback write_cb;
		void(*init)(struct mspeech_usb_manager *manager, unsigned char device_type, event_callback cb, write_callback w_cb, void *param);
		void(*start_service)(struct mspeech_usb_manager *manager);//需要单独开启线程调用此接口，此接口会一直占用线程不会退出，直到调用stop_service
		void(*stop_service)(struct mspeech_usb_manager *manager);
		/**
		* @brief 转换成usb可转发的数据格式
		* @mac  转发目的设备的mac地址
		* @data 转发的数据
		* @size 转发数据的长度
		* @return 转换后数据的长度
		*/
		int(*convert_to_usb_data)(struct mspeech_usb_manager *manager, unsigned char *mac, unsigned char *data, unsigned int size);
		/**
		 * @brief 追加usb接收到的数据
		 * @data 数据存储地址
		 * @size 数据长度
		 */
		void(*append)(struct mspeech_usb_manager *manager, unsigned char *data, unsigned int size);
		/**
		* @brief 下发扫描指令，扫描到的设备通过init中的protocol_callback回调函数输出
		* @scan_timer 扫描时间（ms）
		* @scan_rssi 扫描距离（db）
		*/
		void(*scan_device)(struct mspeech_usb_manager *manager, unsigned int scan_timer, unsigned char scan_rssi);
		/**
		* @brief 下发获取当前连接的设备， 扫描到的设备通过init中的protocol_callback回调函数输出
		*/
		void(*get_connected_device)(struct mspeech_usb_manager *manager);
		/**
		* @brief 连接当前设备
		* @mac 需要连接的设备的mac地址
		*/
		void(*connect_device)(struct mspeech_usb_manager *manager, unsigned char *mac);
		/**
		* @brief 移除当前设备
		* @mac 需要移除的设备的mac地址
		*/
		void(*remove_device)(struct mspeech_usb_manager *manager, unsigned char *mac);
		void(*uninit)(struct mspeech_usb_manager *manager);//必须先调stop_service接口
		/**
		* @brief 解析usb接收到的event事件数据
		*
		*/
		void(*get_usb_event_data)(struct mspeech_usb_manager *manager, unsigned char *data, unsigned int size, struct mspeech_usb_event_data *_event);
		/**
		* @brief 解析usb扫描到的设备设备
		*
		*/
		void(*get_usb_scan_data)(struct mspeech_usb_manager *manager, unsigned char *data, unsigned int size, struct mspeech_usb_scan_data *scan);
		/**
		* @brief 解析usb接受到的data数据
		*
		*/
		void(*get_usb_write_data)(struct mspeech_usb_manager *manager, unsigned char *data, unsigned int size, struct mspeech_usb_write_data *write);
		/**
		* @brief 删除dongle所有配对信息
		*
		*/
		void(*reset_device)(struct mspeech_usb_manager *manager);
		/**
		* @brief 开启OTA
		* session_key OTA密钥，固定长度为16字节
		* firmware: 加密后的固件
		* size：加密后固件总长度
		* return:0：开启OTA失败，请检查输入参数内存空间是否有效或调用end_ota后重试；1：开启OTA成功
		*/
		unsigned char(*start_ota)(struct mspeech_usb_manager *manager, unsigned char *session, unsigned char *firmware, unsigned int size);
		/**
		* @brief 结束OTA
		* value OTA结果，1：成功；0：失败
		*
		*/
		void(*end_ota)(struct mspeech_usb_manager *manager, unsigned char value);

		void(*get_dongle_product_no)(struct mspeech_usb_manager *manager);
	};

	DllExport void mspeech_usb_manager_construct(struct mspeech_usb_manager *usb);

#ifdef __cplusplus
}
#endif