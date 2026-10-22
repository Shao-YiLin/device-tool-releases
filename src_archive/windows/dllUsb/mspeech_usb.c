
#include <malloc.h>
#include<Windows.h>

#include "mspeech_usb.h"
#include "include/lib_speech_usb.h"

//static void *pthread = NULL;

void mspeech_usb_scan(MSPEECH_USB_SCAN_CALLBACK callback, unsigned short vendor_id, unsigned short product_id) {
	libspeechusb_scan_device((LIB_SPEECH_USB_SCAN_CALLBACK)callback, vendor_id, product_id);
}

static struct lib_speech_usb *_get_handle(struct mspeech_usb *um) {
	return um->handle;
}

static int _init(struct mspeech_usb *um, struct mspeech_device_info *device, MSPEECH_RECEIVE_CALLBACK callback, MSPEECH_USB_REMOVE_CALLBACK remove_cb) {

	if(NULL == um->handle)
		um->handle = malloc(sizeof(struct lib_speech_usb));
	libspeechusb_construct_device(um->handle);

	return _get_handle(um)->init(_get_handle(um), (struct libspeechusb_device_info *)device, (LIB_SPEECH_USB_RECEIVE_CALLBACK)callback, (LIB_SPEECH_USB_REMOVE_CALLBACK)remove_cb, device);
}

//static DWORD WINAPI __usb_thread_func(LPVOID param) {
//
//	struct mspeech_usb *lm = param;
//	if (NULL != lm && lm->handle)
//	{
//		_get_handle(lm)->start_read(_get_handle(lm));
//	}
//	return 0;
//}

static int _start_read(struct mspeech_usb *um) {

	
	/*LPDWORD dwThreadId = NULL;
	pthread = CreateThread(NULL, 0, __usb_thread_func, um, 0, dwThreadId);*/
	return _get_handle(um)->start_read(_get_handle(um));
}

static int _stop(struct mspeech_usb *um) {
	return _get_handle(um)->stop_read(_get_handle(um));
}

static int _write(struct mspeech_usb *um, unsigned char* data, unsigned int len) {

	return _get_handle(um)->write(_get_handle(um), data, len);
}

static int _uninit(struct mspeech_usb *um) {
	
	int ret = _get_handle(um)->uninit(_get_handle(um));
	/*if (um->pthread) {
		CloseHandle(um->pthread);
	}*/
	
	if (um->handle) {
		free(um->handle);
		um->handle = NULL;
	}

	return ret;
}

void mspeech_usb_construct(struct mspeech_usb *usb) {

	if (usb) {
		usb->init = _init;
		usb->read = _start_read;
		usb->stop = _stop;
		usb->write = _write;
		usb->uninit = _uninit;
		usb->handle = NULL;
	//	usb->pthread = NULL;
	}
}


