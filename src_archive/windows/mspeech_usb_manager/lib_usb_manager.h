#pragma once
#ifdef __cplusplus
extern "C"
{
#endif

	enum DEVICE_MANAGER_ACTION
	{
		CONNECT_DEVICE = 0x1,
		REMOVE_DEVICE = 0x2,
		GET_CONNECTED_DEVICE = 0x3,
		RESET_DEVICE = 0xf
	};

	enum USB_CMD
	{
		SCAN_DEVICE = 0x41,
		WRITE_DATA = 0x45,
		WRITE_DATA_RESP = 0x47,
		DEVICE_MANAGER = 0x48,
		DEVICE_EVENT = 0x49
	};

	enum USB_ATT
	{
		VERSION_MASTER = 1,
		MAC_MASTER = 2,
		PRODUCT_NO = 3
	};

	enum USB_EVENT_TYPE
	{
		EVENT_TYPE_CONNECTED_DEVICE = 0x1,
		EVENT_TYPE_DISCONNECT_DEVICE = 0x2,
		EVENT_TYPE_SCAN_DEVICE = 0x3,
	};

	enum USB_WRITE_TYPE
	{
		WRITE_TYPE_OTA = 0x1,
		WRITE_TYPE_TRANSPOND = 0x2,
		WRITE_TYPE_MASTER_ATTRIBUTE = 0x3
	};

	enum USB_OTA_STATE
	{
		START_OTA_CB = 0x1,
		UPDATA_OTA_CB = 0x2,
		END_OTA_CB = 0x3
	};

	typedef struct {
		unsigned char ota_step;
		unsigned char dongle_mac[6];
		unsigned char dongle_version[4];
		unsigned char session[16];
		unsigned char session_key[16];
		unsigned int firmware_hash_sum;
		unsigned int firmware_crc32_sum;
		unsigned int firmware_index;
		unsigned char product_NO[4];
		unsigned int firmware_len;
		unsigned char *firmware;
	}ota_item;

	typedef void(*lib_event_cb)(void *param, unsigned char type, unsigned char *data, unsigned int size);
	typedef void(*lib_write_cb)(void *param, unsigned char *data, unsigned int size);

	struct lib_usb_manager {
		void *handler;
		ota_item ota_param;
		void *base;
		unsigned char device_type;
		unsigned char product_ack_flag;
		lib_event_cb event_cb;
		lib_write_cb write_cb;
		void(*init)(struct lib_usb_manager *manager, unsigned char device_type, lib_event_cb cb, lib_write_cb w_cb, void *param);
		void(*start_service)(struct lib_usb_manager *manager);
		void(*stop_service)(struct lib_usb_manager *manager);
		int(*convert_to_usb_data)(struct lib_usb_manager *manager, unsigned char *mac, unsigned char *data, unsigned int size);
		void(*append)(struct lib_usb_manager *manager, unsigned char *data, unsigned int size);
		void(*scan_device)(struct lib_usb_manager *manager, unsigned int scan_timer, unsigned char scan_rssi);
		void(*get_connected_device)(struct lib_usb_manager *manager);
		void(*connect_device)(struct lib_usb_manager *manager, unsigned char *mac);
		void(*remove_device)(struct lib_usb_manager *manager, unsigned char *mac);
		void(*uninit)(struct lib_usb_manager *manager);//必须先调stop_service接口
		void(*reset_device)(struct lib_usb_manager *manager);
		unsigned char(*start_ota)(struct lib_usb_manager *manager, unsigned char *session, unsigned char *firmware, unsigned int size);
		void(*end_ota)(struct lib_usb_manager *manager, unsigned char value);
		void(*get_dongle_product_no)(struct lib_usb_manager *manager);
	};

	void lib_usb_manager_construct(struct lib_usb_manager *usb);
#ifdef __cplusplus
}
#endif