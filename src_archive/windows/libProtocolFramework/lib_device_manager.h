
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
#include "mblue_crc.h"
#include "CommonProtocolLogicManager.h"
#include "MadaoMessage.h"

	struct voice_cache {
		char data[40];
		char len;
		char channel;
	};
	typedef struct {
		unsigned char session[16];
		unsigned char session_key[16];
		unsigned int firmware_hash_sum;
		unsigned int firmware_crc32_sum;
		unsigned int firmware_index;
		unsigned char product_NO[4];
		unsigned int firmware_len;
		unsigned char *firmware;


	}ota_item;

	struct lib_device_manager
	{
		void *base;
		void *protocol_handle1;
		void *protocol_handle2;
		char protocol_type;
		int is_new_protocol;
		int is_timeout;
		void *pthread_handle;

		char known_protocol_type;

		struct mblue_aes aes;
		struct mblue_hash hash;
		struct mblue_md5 md5;
		struct CommonProtocolManager commonProtocol;
		struct MadaoMessage msgManager;
		ota_item ota_param;

		int protocol_handler_valid_flag;

		libmouse_event_cb_fn libmouse_event_cb;
		libmouse_action_cb_fn libmouse_action_cb;
		libmouse_audio_cb_fn libmouse_audio_cb;
		libmouse_info_cb_fn libmouse_info_cb;
		libmouse_write_cb_fn libmouse_write_cb;

		unsigned char authentication_step;
		char device_mac[6];
		char device_version[4];
		char device_finger[16];
		unsigned char client_random[16];

		unsigned char client_finger[16];

		void(*init)(struct lib_device_manager *dm,
			libmouse_write_cb_fn write_cb,
			libmouse_event_cb_fn event_cb,
			libmouse_action_cb_fn action_cb,
			libmouse_audio_cb_fn audio_cb,
			libmouse_info_cb_fn info_cb, void *param);

		void(*uninit)(struct lib_device_manager *dm);

		void(*set_dpi)(struct lib_device_manager *dm, unsigned int dpi_level);

		void(*get_dpi)(struct lib_device_manager *dm);

		void(*start_record)(struct lib_device_manager *dm);

		void(*stop_record)(struct lib_device_manager *dm);

		void(*get_battery)(struct lib_device_manager *dm);

		void(*get_device_version)(struct lib_device_manager *dm);

		void(*get_device_macaddr)(struct lib_device_manager *dm);

		void(*get_dongle_version)(struct lib_device_manager *dm);

		void(*get_dongle_macaddr)(struct lib_device_manager *dm);

		void(*handshake)(struct lib_device_manager *dm);

		void(*get_device_productNo)(struct lib_device_manager *dm);

		void(*get_protocol_version)(struct lib_device_manager *dm);

		void(*set_record_channel_size)(struct lib_device_manager *dm, int size);

		void(*set_sleep_time)(struct lib_device_manager *dm, int time);

		void(*append_data)(struct lib_device_manager *dm, unsigned char *data, unsigned int size);

		void(*set_led_rgb)(struct lib_device_manager *dm, unsigned char type, unsigned int duration, unsigned char *data, int size);

		void(*set_vibration)(struct lib_device_manager *dm, unsigned char status, unsigned char type, unsigned char level, unsigned int duration);

		void(*set_custom_key)(struct lib_device_manager *dm, unsigned char type, unsigned char key_index, unsigned char key_type, unsigned char *data, unsigned int size);

		void(*start_sincheck)(struct lib_device_manager *dm);

		void(*get_theme)(struct lib_device_manager *lm);

		void(*set_handshake_info)(struct lib_device_manager *dm, unsigned int flag);

		void(*set_flip_page)(struct lib_device_manager *dm, unsigned int value);

		void(*get_flip_page)(struct lib_device_manager *dm);

		void(*get_subtitle_satus)(struct lib_device_manager *dm);

		void(*get_custom_key)(struct lib_device_manager *dm, unsigned char key_index);

		void(*get_work_mode)(struct lib_device_manager *dm);

		void(*get_connection_type)(struct lib_device_manager *dm);

		void(*start_ota)(struct lib_device_manager *dm, unsigned char *session, unsigned char *firmware, unsigned int size);

		void(*update_ota_data)(struct lib_device_manager *dm, unsigned char *data, unsigned int size);

		void(*end_ota)(struct lib_device_manager *dm, unsigned int value);
		void(*set_polling_rate)(struct lib_device_manager *dm, unsigned int value);
		void(*get_polling_rate)(struct lib_device_manager *dm);
		void(*get_dongle_product_no)(struct lib_device_manager *dm);
		void(*get_led_intensity)(struct lib_device_manager *dm, unsigned char index);
		void(*set_led_intensity)(struct lib_device_manager *dm, unsigned char index, unsigned char);

		void(*get_rgb_backlight_param)(struct lib_device_manager *dm, unsigned char index);
		void(*set_rgb_backlight_param)(struct lib_device_manager *dm, unsigned char index, unsigned char);

		void(*get_laser_mode)(struct lib_device_manager *dm);
		void(*set_laser_mode)(struct lib_device_manager *dm, unsigned char value);

		void(*get_slave_device_version)(struct lib_device_manager *dm);

		void(*set_wheel_mode)(struct lib_device_manager *dm, unsigned char value);
		void(*get_wheel_mode)(struct lib_device_manager *dm);
		void(*set_os_type)(struct lib_device_manager* dm, unsigned int os_type);

		void(*heart_beat)(struct lib_device_manager *framework, unsigned char connect_status);

		void(*player_play)(struct lib_device_manager *framework, unsigned int tone_index);

		void(*set_player_volume)(struct lib_device_manager *framework, unsigned int volume);

		void(*get_player_volume)(struct lib_device_manager *framework);

		void(*set_time_heart_beat)(struct lib_device_manager *framework, unsigned int time);

		void(*get_time_heart_beat)(struct lib_device_manager *framework);

		void(*set_keyboard_custom_rgb)(struct lib_device_manager *framework, unsigned char *data, unsigned int size);

		void(*set_adaptive_modulation)(struct lib_device_manager *framework,  unsigned int value);

		void(*get_adaptive_modulation)(struct lib_device_manager *framework);

		void(*get_ls_device_id)(struct lib_device_manager *framework);

		void(*get_ls_check_sum)(struct lib_device_manager *framework, unsigned int time);
		
		
	};

	void lib_get_device_manager(struct lib_device_manager *dm);

#ifdef __cplusplus
}
#endif

#endif