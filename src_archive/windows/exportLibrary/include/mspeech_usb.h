#pragma once
#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#define DllImport   __declspec( dllimport )
#define DllExport   __declspec( dllexport )

	struct mspeech_device_info
	{
		unsigned short vendor_id;
		unsigned short product_id;
		unsigned char device_address;
		unsigned char device_type;
	};

	typedef void(*MSPEECH_USB_SCAN_CALLBACK)(struct mspeech_device_info *data, int size);

	typedef void(*MSPEECH_RECEIVE_CALLBACK)(void *param, unsigned char* data, unsigned int len);

	typedef void(*MSPEECH_USB_REMOVE_CALLBACK)(void *param);

	DllExport void mspeech_usb_scan(MSPEECH_USB_SCAN_CALLBACK callback, unsigned short vendor_id, unsigned short product_id);

	struct mspeech_usb {

		MSPEECH_RECEIVE_CALLBACK receive_callback;
		void *handle;
	//	void *pthread;

		int(*init)(struct mspeech_usb *um, struct mspeech_device_info *device, MSPEECH_RECEIVE_CALLBACK callback, MSPEECH_USB_REMOVE_CALLBACK remove_cb);
		int(*read)(struct mspeech_usb *um);
		int(*stop)(struct mspeech_usb *um);
		int(*write)(struct mspeech_usb *um, unsigned char* data, unsigned int len);
		int(*uninit)(struct mspeech_usb *um);
	};

	DllExport void mspeech_usb_construct(struct mspeech_usb *usb);

#ifdef __cplusplus
}
#endif