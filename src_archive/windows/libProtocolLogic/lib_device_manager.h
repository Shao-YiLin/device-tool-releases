
/*
 * @Author: xuhan
 * @Date: 2020-02-08 11:01:23
 * @LastEditors  : xuhan
 * @LastEditTime : 2020-02-09 12:24:40
 * @Description: 
 */
#ifndef _LIB_DEVICE_MANAGER_H_
#define _LIB_DEVICE_MANAGER_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "lib_device_define.h"
#include "mblue_fifo.h"
#include <Windows.h>
#include "mblue_aes.h"
#include "mblue_hash.h"
#include "mblue_md5.h"

	struct voice_cache {
		char data[40];
		char len;
		char channel;
	};

    struct lib_device_manager
    {

        void *protocol_handle1;
		void *protocol_handle2;
        struct lib_speech_usb *usb_handle;
        char protocol_type;
        int is_new_protocol;
        int is_timeout;
		void * pthread_handle;
       /* void *voice_handle1;
        void *voice_handle2;*/
		/*struct mblue_fifo *fifo;
		CRITICAL_SECTION lock; 
		void * voice_pthread_handle;*/

		struct mblue_aes aes;
		struct mblue_hash hash;
		struct mblue_md5 md5;

        struct libmouse_device_info *device;
        libmouse_event_cb_fn libmouse_event_cb;
        libmouse_action_cb_fn libmouse_action_cb;
        libmouse_audio_cb_fn libmouse_audio_cb;
        libmouse_info_cb_fn libmouse_info_cb;

		unsigned char authentication_step;
		char device_mac[6];
		char device_version[4];
		char device_finger[16];
		unsigned char client_random[16];

        void (*init)(struct lib_device_manager *dm,
                     libmouse_event_cb_fn event_cb,
                     libmouse_action_cb_fn action_cb,
                     libmouse_audio_cb_fn audio_cb,
                     libmouse_info_cb_fn info_cb);

        void (*uninit)(struct lib_device_manager *dm);

        int (*open_device)(struct lib_device_manager *dm, struct libmouse_device_info *usb_device);

        int (*close_device)(struct lib_device_manager *dm);

        void (*set_dpi)(struct lib_device_manager *dm, unsigned int dpi_level);

        void (*get_dpi)(struct lib_device_manager *dm);

        void (*start_record)(struct lib_device_manager *dm);

        void (*stop_record)(struct lib_device_manager *dm);

        void (*get_battery)(struct lib_device_manager *dm);

        void (*get_device_version)(struct lib_device_manager *dm);

        void (*get_device_macaddr)(struct lib_device_manager *dm);

        void (*get_dongle_version)(struct lib_device_manager *dm);

        void (*get_dongle_macaddr)(struct lib_device_manager *dm);

        void (*handshake)(struct lib_device_manager *dm);

        void (*get_device_productNo)(struct lib_device_manager *dm);

        void (*get_protocol_version)(struct lib_device_manager *dm);

        void (*set_record_channel_size)(struct lib_device_manager *dm, int size);

        void (*start_authentication)(struct lib_device_manager *dm);
    };

    void lib_get_device_manager(struct lib_device_manager *dm);

#ifdef __cplusplus
}
#endif

#endif