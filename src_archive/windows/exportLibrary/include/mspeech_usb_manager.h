#pragma once
#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#define DllImport   __declspec( dllimport )
#define DllExport   __declspec( dllexport )

	enum USB_CMD
	{
		SCAN_DEVICE = 0x41,
		WRITE_DATA = 0x45,
		WRITE_DATA_RESP = 0x47,
		DEVICE_MANAGER = 0x48,
		DEVICE_EVENT = 0x49
	};

	enum USB_EVENT_TYPE
	{
		EVENT_TYPE_CONNECTED_DEVICE = 0x1,
		EVENT_TYPE_DISCONNECT_DEVICE = 0x2,
		EVENT_TYPE_SCAN_DEVICE = 0x3
	};

	enum USB_WRITE_TYPE
	{
		WRITE_TYPE_OTA = 0x1,
		WRITE_TYPE_TRANSPOND = 0x2
	};

	typedef void(*protocol_callback)(void *param, unsigned char cmd, unsigned char sequence, unsigned char *data, unsigned int size);
	typedef void(*write_callback)(void *param, unsigned char *data, unsigned int size);

	struct mspeech_usb_event_data
	{
		unsigned char type;
		unsigned char version;
		unsigned char mac[6];
		unsigned char data[128];
		unsigned int len;
	};

	struct mspeech_usb_scan_data
	{
		unsigned char type;
		unsigned char version;
		unsigned char rssi;
		unsigned char mac[6];
		unsigned char data[128];
		unsigned int len;
	};

	struct mspeech_usb_write_data {
		unsigned char type;
		unsigned char mac[6];
		unsigned char data[128];
		unsigned int len;
	};

	struct mspeech_usb_manager{

		void *handler;
		void *pthread_handle;
		write_callback write_cb;
		
		void(*init)(struct mspeech_usb_manager *manager, protocol_callback cb, write_callback w_cb);
		int(*convert_to_usb_data)(struct mspeech_usb_manager *manager, unsigned char *mac, unsigned char *data, unsigned int size, unsigned char *dest);
		void(*append)(struct mspeech_usb_manager *manager, unsigned char *data, unsigned int size);
		void(*scan_device)(struct mspeech_usb_manager *manager, unsigned int scan_timer);
		void(*get_connected_device)(struct mspeech_usb_manager *manager);
		void(*connect_device)(struct mspeech_usb_manager *manager, unsigned char *mac);
		void(*remove_device)(struct mspeech_usb_manager *manager, unsigned char *mac);
		void(*uninit)(struct mspeech_usb_manager *manager);
		void(*get_usb_event_data)(struct mspeech_usb_manager *manager, unsigned char *data, unsigned int size, struct mspeech_usb_event_data *event);
		void(*get_usb_scan_data)(struct mspeech_usb_manager *manager, unsigned char *data, unsigned int size, struct mspeech_usb_scan_data *scan);
		void(*get_usb_write_data)(struct mspeech_usb_manager *manager, unsigned char *data, unsigned int size, struct mspeech_usb_write_data *write);
	};

	DllExport void mspeech_usb_manager_construct(struct mspeech_usb_manager *usb);

#ifdef __cplusplus
}
#endif