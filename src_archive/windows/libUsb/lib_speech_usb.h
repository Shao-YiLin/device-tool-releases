/*
 * @Author: xuhan
 * @Date: 2020-02-08 17:59:10
 * @LastEditors  : xuhan
 * @LastEditTime : 2020-02-08 18:04:13
 * @Description: 
 */
#ifndef _LIB_SPEECH_USB_H
#define _LIB_SPEECH_USB_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct libspeechusb_device_info
{
    unsigned short vendor_id;
    unsigned short product_id;
    unsigned char device_address;
    unsigned char device_type;
};

typedef void (*LIB_SPEECH_USB_SCAN_CALLBACK)(struct libspeechusb_device_info *data, int size);

typedef void(*LIB_SPEECH_USB_RECEIVE_CALLBACK)(void *param, uint8_t* data, uint32_t len);

typedef void(*LIB_SPEECH_USB_REMOVE_CALLBACK)(void *param);

/**
 * @brief  usb hotplug state
 * @note   
 * @param  status: 0(°Î³ö), 1(²åÈë)
 * @retval 
 */
typedef void (*LIB_SPEECH_USB_HOTPLUG_CALLBACK)(unsigned char status, unsigned int device_address);

void libspeechusb_scan_device(LIB_SPEECH_USB_SCAN_CALLBACK callback, unsigned short vendor_id, unsigned short product_id);

//int libspeechusb_hotplug_init(LIB_SPEECH_USB_HOTPLUG_CALLBACK callback);
//
//int libspeechusb_hotplug_uninit();

struct lib_speech_usb {

    LIB_SPEECH_USB_RECEIVE_CALLBACK receive_callback;
    void *handle;

    int (*init)(struct lib_speech_usb *um, struct libspeechusb_device_info *device, LIB_SPEECH_USB_RECEIVE_CALLBACK callback, LIB_SPEECH_USB_REMOVE_CALLBACK remove_cb, void *param);
    int (*start_read)(struct lib_speech_usb *um);
	int (*stop_read)(struct lib_speech_usb *um);
    int (*write)(struct lib_speech_usb *um, uint8_t *data, uint32_t len);
    int (*uninit)(struct lib_speech_usb *um);
};

void libspeechusb_construct_device(struct lib_speech_usb *usb);

#ifdef __cplusplus
}
#endif

#endif