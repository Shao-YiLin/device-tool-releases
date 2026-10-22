/*
 * @Author: xuhan
 * @Date: 2020-02-08 18:01:37
 * @LastEditors: xuhan
 * @LastEditTime: 2020-02-08 18:01:38
 * @Description: 
 */
#include "include/libusb.h"
#include <stdint.h>

struct device_info
{
    unsigned short vendor_id;
    unsigned short product_id;
    unsigned char device_address;
    unsigned char device_type;
};

typedef void (*USB_SCAN_CALLBACK)(struct device_info *data, int size);

void scan_device(USB_SCAN_CALLBACK scan_callback, unsigned short vendor_id, unsigned short product_id);

typedef void(*USB_RECEIVE_CALLBACK)( void *param, uint8_t* data, uint32_t len);

typedef void(*USB_REMOVE_CALLBACK)(void *param);

struct usb_manager
{

    USB_RECEIVE_CALLBACK data_callback;
	USB_REMOVE_CALLBACK usb_remove_callback;
    libusb_device_handle *dev_handler;
    
    uint8_t lock_flag;

    uint8_t b_find_device_flag;
    uint8_t b_run;
    uint8_t out_endpoit;
    uint8_t in_endpoit;
    uint8_t read_buffer[256];
    libusb_device *dev;
    libusb_device **devs;
    void *cb_param;

    int (*init)(struct usb_manager *um, struct device_info *device, USB_RECEIVE_CALLBACK callback, USB_REMOVE_CALLBACK remove_cb, void *param);
    int (*start_read)(struct usb_manager *um);
	int (*stop_read)(struct usb_manager *um);
    int (*write)(struct usb_manager *um, uint8_t *data, uint32_t len);
    int (*uninit)(struct usb_manager *um);
};

void usb_construct(struct usb_manager *manager);

// <==
