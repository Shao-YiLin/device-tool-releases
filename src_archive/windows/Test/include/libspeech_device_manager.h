/*
 * @Author: xuhan
 * @Date: 2020-02-08 19:36:20
 * @LastEditors  : xuhan
 * @LastEditTime : 2020-02-08 19:45:44
 * @Description: 
 */
#ifndef _LIBSPEECH_DEVICE_MANAGER_H_
#define _LIBSPEECH_DEVICE_MANAGER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "lib_device_define.h"

void libdevice_scan_device(void (*scan_cb)(struct libmouse_device_info *data, int size));

struct libspeech_device_manager
{
    void *handle;

    void (*init)(struct libspeech_device_manager *dm,
                     libmouse_event_cb_fn event_cb,
                     libmouse_action_cb_fn action_cb,
                     libmouse_audio_cb_fn audio_cb,
                     libmouse_info_cb_fn info_cb);

        void (*uninit)(struct libspeech_device_manager *dm);

        int (*open_device)(struct libspeech_device_manager *dm, struct libmouse_device_info *usb_device);

        int (*close_device)(struct libspeech_device_manager *dm);

        void (*set_dpi)(struct libspeech_device_manager *dm, unsigned int dpi_level);

        void (*get_dpi)(struct libspeech_device_manager *dm);

        void (*start_record)(struct libspeech_device_manager *dm);

        void (*stop_record)(struct libspeech_device_manager *dm);

        void (*get_battery)(struct libspeech_device_manager *dm);

        void (*get_device_version)(struct libspeech_device_manager *dm);

        void (*get_device_macaddr)(struct libspeech_device_manager *dm);

        void (*get_dongle_version)(struct libspeech_device_manager *dm);

        void (*get_dongle_macaddr)(struct libspeech_device_manager *dm);

        void (*handshake)(struct libspeech_device_manager *dm);

        void (*get_device_productNo)(struct libspeech_device_manager *dm);

        void (*get_protocol_version)(struct libspeech_device_manager *dm);

        void (*set_record_channel_size)(struct libspeech_device_manager *dm, int size);

        void (*start_authentication)(struct libspeech_device_manager *dm);
};

void libspeech_device_manager_construct(struct libspeech_device_manager *dm);

#ifdef __cplusplus
}
#endif

#endif