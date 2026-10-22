/*
 * @Author: xuhan
 * @Date: 2020-02-08 17:59:40
 * @LastEditors  : xuhan
 * @LastEditTime : 2020-02-08 18:04:05
 * @Description: 
 */
#include "lib_speech_usb.h"
#include "usb_manager.h"
#include <string.h>
#include "mblue_heap.h"

static LIB_SPEECH_USB_SCAN_CALLBACK lib_scan_cb = NULL;

static struct usb_manager *__get_handle(void *handle)
{
    return (struct usb_manager *)handle;
}
static int _init(struct lib_speech_usb *um, struct libspeechusb_device_info *device, LIB_SPEECH_USB_RECEIVE_CALLBACK callback, LIB_SPEECH_USB_REMOVE_CALLBACK remove_cb, void *param)
{
    if (NULL == um->handle)
        um->handle = mblue_malloc(sizeof(struct usb_manager));
    usb_construct(um->handle);

    return __get_handle(um->handle)->init(um->handle, (struct device_info *)device, callback, remove_cb, param);
}

static int _start_read(struct lib_speech_usb *um)
{
    return __get_handle(um->handle)->start_read(um->handle);
}

static int _stop_read(struct lib_speech_usb *um) {
	return __get_handle(um->handle)->stop_read(um->handle);
}

static int _write(struct lib_speech_usb *um, uint8_t *data, uint32_t len)
{
    return __get_handle(um->handle)->write(um->handle, data, len);
}

static int _uninit(struct lib_speech_usb *um)
{
    int ret = __get_handle(um->handle)->uninit(um->handle);
    if (NULL != um->handle)
    {
        mblue_free(um->handle);
        um->handle = NULL;
    }

    return ret;
}

static void _scan_device_cb(struct device_info *data, int size)
{
    if (NULL == lib_scan_cb)
        return;

    if (NULL == data || size == 0)
        return;

    struct libspeechusb_device_info devices[10];
    int i;
    for (i = 0; i < size; i++)
    {
        memcpy(&devices[i], data + i, sizeof(struct device_info));
    }

    lib_scan_cb(devices, size);
}

void libspeechusb_scan_device(LIB_SPEECH_USB_SCAN_CALLBACK callback, unsigned short vendor_id, unsigned short product_id)
{
    lib_scan_cb = callback;
    scan_device(_scan_device_cb, vendor_id, product_id);
}

static inline void __install_lib_speech_usb_operation(struct lib_speech_usb *manager)
{
    manager->init = _init;
    manager->start_read = _start_read;
	manager->stop_read = _stop_read;
    manager->write = _write;
    manager->uninit = _uninit;
	manager->handle = NULL;
	manager->receive_callback = NULL;
}

void libspeechusb_construct_device(struct lib_speech_usb *usb)
{
    if (NULL == usb)
        return;

    __install_lib_speech_usb_operation(usb);
}
