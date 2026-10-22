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

	struct libspeech_device_manager
	{
		void* handle;

		void(*init)(struct libspeech_device_manager* dm,
			libmouse_write_cb_fn write_cb,
			libmouse_event_cb_fn event_cb,
			libmouse_action_cb_fn action_cb,
			libmouse_audio_cb_fn audio_cb,
			libmouse_info_cb_fn info_cb, void* param);

		void(*uninit)(struct libspeech_device_manager* dm);

		void(*set_protocol_version)(struct libspeech_device_manager* dm, char version);

		void(*set_dpi)(struct libspeech_device_manager* dm, unsigned int dpi_level);

		void(*get_dpi)(struct libspeech_device_manager* dm);

		void(*start_record)(struct libspeech_device_manager* dm);

		void(*stop_record)(struct libspeech_device_manager* dm);

		void(*get_battery)(struct libspeech_device_manager* dm);

		void(*get_device_version)(struct libspeech_device_manager* dm);

		void(*get_device_macaddr)(struct libspeech_device_manager* dm);

		void(*get_dongle_version)(struct libspeech_device_manager* dm);

		void(*get_dongle_macaddr)(struct libspeech_device_manager* dm);

		void(*handshake)(struct libspeech_device_manager* dm);

		void(*get_device_productNo)(struct libspeech_device_manager* dm);

		void(*get_protocol_version)(struct libspeech_device_manager* dm);

		void(*set_record_channel_size)(struct libspeech_device_manager* dm, int size);

		void(*set_sleep_time)(struct libspeech_device_manager* dm, int time);

		void(*append_data)(struct libspeech_device_manager* dm, unsigned char* data, unsigned int size);

		void(*set_led_rgb)(struct libspeech_device_manager* dm, unsigned char type, unsigned int duration, unsigned char* data, int size);

		void(*set_vibration)(struct libspeech_device_manager* dm, unsigned char status, unsigned char type, unsigned char level, unsigned int duration);

		void(*set_custom_key)(struct libspeech_device_manager* dm, unsigned char type, unsigned char key_index, unsigned char key_type, unsigned char* data, unsigned int size);

		void(*start_sincheck)(struct libspeech_device_manager* dm);

		void(*get_theme)(struct libspeech_device_manager* dm);

		void(*set_handshake_info)(struct libspeech_device_manager* dm, unsigned int flag);

		void(*set_flip_page)(struct libspeech_device_manager* framework, unsigned int value);

		void(*get_flip_page)(struct libspeech_device_manager* framework);

		void(*get_subtitle_status)(struct libspeech_device_manager* framework);

		void(*get_work_mode)(struct libspeech_device_manager* framework);

		void(*get_connection_type)(struct libspeech_device_manager* framework);

		void(*get_custom_key)(struct libspeech_device_manager* framework, unsigned char index);

		void(*start_ota)(struct libspeech_device_manager* framework, unsigned char* session, unsigned char* firmware, unsigned int size);

		void(*end_ota)(struct libspeech_device_manager* framework, unsigned int value);

		void(*set_polling_rate)(struct libspeech_device_manager* framework, unsigned int value);

		void(*get_polling_rate)(struct libspeech_device_manager* framework);

		void(*get_dongle_product_no)(struct libspeech_device_manager* framework);

		void(*get_led_intensity)(struct libspeech_device_manager* framework, unsigned char index);

		void(*set_led_intensity)(struct libspeech_device_manager* framework, unsigned char index, unsigned char value);

		void(*get_rgb_backlight_param)(struct libspeech_device_manager* framework, unsigned char index);

		void(*set_rgb_backlight_param)(struct libspeech_device_manager* framework, unsigned char index, unsigned char value);

		void(*get_laser_mode)(struct libspeech_device_manager* framework);

		void(*set_laser_mode)(struct libspeech_device_manager* framework, unsigned char value);

		void(*get_slave_device_version)(struct libspeech_device_manager* framework);

		void(*set_wheel_mode)(struct libspeech_device_manager* framework, unsigned char value);

		void(*get_wheel_mode)(struct libspeech_device_manager* framework);

		void (*set_os_type)(struct libspeech_device_manager* dm, unsigned int type);

		void(*heart_beat)(struct libspeech_device_manager *framework, unsigned char connect_status);

		void(*player_play)(struct libspeech_device_manager *framework, unsigned int tone_index);

		void(*set_player_volume)(struct libspeech_device_manager *framework, unsigned int volume);

		void(*get_player_volume)(struct libspeech_device_manager *framework);

		void(*set_time_heart_beat)(struct libspeech_device_manager *framework, unsigned int time);

		void(*get_time_heart_beat)(struct libspeech_device_manager *framework);

		void(*set_keyboard_custom_rgb)(struct libspeech_device_manager *framework, unsigned char *data, unsigned int size);

		void(*set_adaptive_modulation)(struct libspeech_device_manager *framework,  unsigned int value);

		void(*get_adaptive_modulation)(struct libspeech_device_manager *framework);

		void(*get_ls_device_id)(struct libspeech_device_manager *framework);

		void(*get_ls_check_sum)(struct libspeech_device_manager *framework, unsigned int time);
		
	};

	void libspeech_device_manager_construct(struct libspeech_device_manager* dm);

#ifdef __cplusplus
}
#endif

#endif