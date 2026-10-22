/*
 * @Author: xuhan
 * @Date: 2020-02-07 16:43:16
 * @LastEditors  : xuhan
 * @LastEditTime : 2020-02-08 16:31:59
 * @Description:
 */
#ifndef _PROTOCOL_2_LOGIC_MANAGER_H_
#define _PROTOCOL_2_LOGIC_MANAGER_H_

#include "libprotocol_logic_define.h"
#include "include/libspeech_protocol.h"
#include "mblue_fifo.h"

typedef struct item_node
{
	int sequence;
	int cmd;
	long tick;
} item_node_t;

typedef struct CmdNode
{
	unsigned char cmd;
	unsigned char sequence;
	unsigned short size;
	unsigned char data[48];
}CmdNode_T;

#define NOTE_CACHE_SIZE (5)

struct protocol_logic_manager
{
	struct libspeech_protocol protocol_handle;
	LOGIC_ACTION_CALLBACK action_listener;
	DEVICE_WRITE_CALLBACK write_listener;
	//void * pthread_handle;
   // int service_state;
	void *cb_param;
	struct item_node item_node_array[NOTE_CACHE_SIZE];
	CRITICAL_SECTION mLock;
	//struct mblue_fifo *pCmdFifo;

	void(*register_action_callback)(struct protocol_logic_manager *lm, LOGIC_ACTION_CALLBACK callback, DEVICE_WRITE_CALLBACK write_callback, void *param);
	int(*init)(struct protocol_logic_manager *lm);
	int(*uninit)(struct protocol_logic_manager *lm);
	int(*start_record)(struct protocol_logic_manager *lm);
	int(*stop_record)(struct protocol_logic_manager *lm);
	int(*set_dpi)(struct protocol_logic_manager *lm, int level);
	int(*get_dpi)(struct protocol_logic_manager *lm);
	int(*get_version)(struct protocol_logic_manager *lm, unsigned char cmd);
	int(*get_mac_address)(struct protocol_logic_manager *lm, unsigned char cmd);
	int(*get_battery)(struct protocol_logic_manager *lm);
	int(*handshake)(struct protocol_logic_manager *lm);
	int(*set_record_channel)(struct protocol_logic_manager *lm, int channel_size);
	int(*set_os_type)(struct protocol_logic_manager *lm, int os_type);
	int(*set_sleep_time)(struct protocol_logic_manager *lm, int sleep_time);
	int(*get_productNo)(struct protocol_logic_manager *lm);
	int(*get_protocol_version)(struct protocol_logic_manager *lm);
	int(*get_device_finger)(struct protocol_logic_manager *lm);
	int(*set_client_finger)(struct protocol_logic_manager *lm, unsigned char *data, int len);
	int(*set_authenticate)(struct protocol_logic_manager *lm, unsigned char *data, int size, unsigned char* session_key, unsigned char* mouse_finger, unsigned char* client_finger, unsigned char* random);
	int(*send_cmd)(struct protocol_logic_manager *lm, unsigned char cmd, unsigned char *data, int size);
	int(*add_data)(struct protocol_logic_manager *lm, unsigned char cmd, unsigned char sequence, unsigned char *data, int size);
	int(*set_ifly_authenticate)(struct protocol_logic_manager *lm, unsigned char *data, int size);
	int(*set_led_rgb)(struct protocol_logic_manager *lm, unsigned char type, unsigned int duration, unsigned char *data, int size);
	int(*set_vibration)(struct protocol_logic_manager *lm, unsigned char status, unsigned char type, unsigned char level, unsigned int duration);
	int(*set_custom_key)(struct protocol_logic_manager *lm, unsigned char type, unsigned char key_index, unsigned char key_type, unsigned char *data, unsigned int size);
	int(*start_sincheck)(struct protocol_logic_manager *lm);
	int(*get_theme)(struct protocol_logic_manager *lm);
	int(*set_handshake_info)(struct protocol_logic_manager *lm, unsigned int flag);
	int(*set_flip_page)(struct protocol_logic_manager *lm, unsigned int value);
	int(*get_flip_page)(struct protocol_logic_manager *lm);
	int(*get_subtitle_status)(struct protocol_logic_manager *lm);
	int(*get_mouse_work_mode)(struct protocol_logic_manager *lm);
	int(*get_mouse_connection_type)(struct protocol_logic_manager *lm);
	int(*get_custom_key)(struct protocol_logic_manager *lm, unsigned char key_index);
	void(*clear_command_map)(struct protocol_logic_manager *lm);
	int(*start_ota)(struct protocol_logic_manager *lm, unsigned char *session_key);
	int(*update_ota_data)(struct protocol_logic_manager *lm, unsigned char *firmware_data, unsigned int size);
	int(*end_ota)(struct protocol_logic_manager *lm, unsigned char value, unsigned int firmware_crc_sum);
	int(*set_polling_rate)(struct protocol_logic_manager *lm, unsigned int value);
	int(*get_polling_rate)(struct protocol_logic_manager *lm);
	int(*get_dongle_product_no)(struct protocol_logic_manager *lm);
	int(*get_led_intensity)(struct protocol_logic_manager *lm,unsigned char index);
	int(*set_led_intensity)(struct protocol_logic_manager *lm, unsigned char index,unsigned char value);
	int(*get_rgb_backlight_param)(struct protocol_logic_manager *lm, unsigned char index);
	int(*set_rgb_backlight_param)(struct protocol_logic_manager *lm, unsigned char index, unsigned char value);
	int(*get_laser_mode)(struct protocol_logic_manager *lm);
	int(*set_laser_mode)(struct protocol_logic_manager *lm, unsigned char value);
	int(*set_wheel_mode)(struct protocol_logic_manager *lm, unsigned char value);
	int(*get_wheel_mode)(struct protocol_logic_manager *lm);

	int(*heart_beat)(struct protocol_logic_manager *framework, unsigned char connect_status);
	int(*player_play)(struct protocol_logic_manager *framework, unsigned int tone_index);
	int(*set_player_volume)(struct protocol_logic_manager *framework, unsigned int volume);
	int(*get_player_volume)(struct protocol_logic_manager *framework);
	int(*set_time_heart_beat)(struct protocol_logic_manager *framework, unsigned int time);
	int(*get_time_heart_beat)(struct protocol_logic_manager *framework);
	int(*set_keyboard_custom_rgb)(struct protocol_logic_manager *framework, unsigned char *data, unsigned int size);
	int(*set_adaptive_modulation)(struct protocol_logic_manager *framework, unsigned int value);
	int(*get_adaptive_modulation)(struct protocol_logic_manager *framework);
	int(*get_ls_device_id)(struct protocol_logic_manager *framework);
	int(*get_ls_check_sum)(struct protocol_logic_manager *framework, unsigned int time);



	
};

void protocol_logic_manager_construct(struct protocol_logic_manager *plm);

#endif