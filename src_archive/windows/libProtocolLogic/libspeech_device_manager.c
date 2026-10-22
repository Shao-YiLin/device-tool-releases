/*
 * @Author: xuhan
 * @Date: 2020-02-08 19:46:05
 * @LastEditors  : xuhan
 * @LastEditTime : 2020-02-09 22:09:02
 * @Description: 
 */

#include <stdio.h>
#include <string.h>
#include "mblue_heap.h"

#include "libspeech_device_manager.h"
#include "include/lib_speech_usb.h"
#include "lib_device_manager.h"

static struct lib_device_manager *__get_handle(void *param)
{
    return (struct lib_device_manager *)param;
}

static void _init(struct libspeech_device_manager *dm,
                  libmouse_event_cb_fn event_cb,
                  libmouse_action_cb_fn action_cb,
                  libmouse_audio_cb_fn audio_cb,
                  libmouse_info_cb_fn info_cb)
{

    if (!dm->handle)
        dm->handle = mblue_malloc(sizeof(struct lib_device_manager));
    lib_get_device_manager(dm->handle);
    __get_handle(dm->handle)->init(dm->handle, event_cb, action_cb, audio_cb, info_cb);
}

static void _uninit(struct libspeech_device_manager *dm)
{
    if (dm->handle)
    {
        __get_handle(dm->handle)->uninit(dm->handle);
        mblue_free(dm->handle);
        dm->handle = 0;
    }
}

static int _open_device(struct libspeech_device_manager *dm, struct libmouse_device_info *usb_device)
{
    return __get_handle(dm->handle)->open_device(dm->handle, usb_device);
}

static int _close_device(struct libspeech_device_manager *dm)
{
    return __get_handle(dm->handle)->close_device(dm->handle);
}

static void _set_dpi(struct libspeech_device_manager *dm, unsigned int dpi_level)
{
    __get_handle(dm->handle)->set_dpi(dm->handle, dpi_level);
}

static void _get_dpi(struct libspeech_device_manager *dm)
{
    __get_handle(dm->handle)->get_dpi(dm->handle);
}

static void _start_record(struct libspeech_device_manager *dm)
{
    __get_handle(dm->handle)->start_record(dm->handle);
}

static void _stop_record(struct libspeech_device_manager *dm)
{
    __get_handle(dm->handle)->stop_record(dm->handle);
}

static void _get_battery(struct libspeech_device_manager *dm)
{
    __get_handle(dm->handle)->get_battery(dm->handle);
}

static void _get_device_version(struct libspeech_device_manager *dm)
{
    __get_handle(dm->handle)->get_device_version(dm->handle);
}

static void _get_device_macaddr(struct libspeech_device_manager *dm)
{
    __get_handle(dm->handle)->get_device_macaddr(dm->handle);
}

static void _get_dongle_version(struct libspeech_device_manager *dm)
{
    __get_handle(dm->handle)->get_dongle_version(dm->handle);
}

static void _get_dongle_macaddr(struct libspeech_device_manager *dm)
{
    __get_handle(dm->handle)->get_dongle_macaddr(dm->handle);
}

static void _handshake(struct libspeech_device_manager *dm)
{
    __get_handle(dm->handle)->handshake(dm->handle);
}

static void _get_device_productNo(struct libspeech_device_manager *dm)
{
    __get_handle(dm->handle)->get_device_productNo(dm->handle);
}

static void _get_protocol_version(struct libspeech_device_manager *dm)
{
    __get_handle(dm->handle)->get_protocol_version(dm->handle);
}

static void _set_record_channel_size(struct libspeech_device_manager *dm, int size)
{
    __get_handle(dm->handle)->set_record_channel_size(dm->handle, size);
}

static void _start_authentication(struct libspeech_device_manager *dm)
{
    __get_handle(dm->handle)->start_authentication(dm->handle);
}

static void __install_device_manager_construct(struct libspeech_device_manager *dm)
{
    if (!dm)
        return;

    dm->init = _init;
    dm->uninit = _uninit;
    dm->open_device = _open_device;
    dm->close_device = _close_device;
    dm->set_dpi = _set_dpi;
    dm->get_dpi = _get_dpi;
    dm->start_record = _start_record;
    dm->stop_record = _stop_record;
    dm->get_battery = _get_battery;
    dm->get_device_version = _get_device_version;
    dm->get_device_macaddr = _get_device_macaddr;
    dm->get_dongle_version = _get_dongle_version;
    dm->get_dongle_macaddr = _get_dongle_macaddr;
    dm->handshake = _handshake;
    dm->get_device_productNo = _get_device_productNo;
    dm->get_protocol_version = _get_protocol_version;
    dm->set_record_channel_size = _set_record_channel_size;
    dm->start_authentication = _start_authentication;
}

void libspeech_device_manager_construct(struct libspeech_device_manager *dm)
{
    if (!dm)
        return;

    __install_device_manager_construct(dm);
}

void libdevice_scan_device(void (*scan_cb)(struct libmouse_device_info *data, int size))
{
    libspeechusb_scan_device(scan_cb, USB_VID, USB_PRODUCT_ID);
}