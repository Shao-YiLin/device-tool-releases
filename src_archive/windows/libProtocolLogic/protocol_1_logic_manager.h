/*
 * @Author: xuhan
 * @Date: 2020-02-06 16:06:46
 * @LastEditTime : 2020-02-08 16:31:36
 * @LastEditors  : xuhan
 * @Description: 
 */

#ifndef _PROTOCOL_1_LOGIC_MANAGER_H_
#define _PROTOCOL_1_LOGIC_MANAGER_H_

#include "libprotocol_logic_define.h"
// #include "include/lib_speech_usb.h"
#include "include/libspeech_protocol.h"

struct protocol_1_logic_manager
{

    // struct lib_speech_usb *usb_handle;
    struct libspeech_protocol protocol_handle;
    LOGIC_ACTION_CALLBACK action_listener;
    DEVICE_WRITE_CALLBACK write_listener;
	void * pthread_handle;
    int service_state;
    void *cb_param;

    void (*register_action_callback)(struct protocol_1_logic_manager *lm, LOGIC_ACTION_CALLBACK callback, DEVICE_WRITE_CALLBACK write_callback, void *param);
    int (*init)(struct protocol_1_logic_manager *lm);
    int (*uninit)(struct protocol_1_logic_manager *lm);
    int (*start_record)(struct protocol_1_logic_manager *lm);
    int (*stop_record)(struct protocol_1_logic_manager *lm);
    int (*set_dpi)(struct protocol_1_logic_manager *lm, int level);
    int (*get_dpi)(struct protocol_1_logic_manager *lm);
    int (*get_version)(struct protocol_1_logic_manager *lm, unsigned char cmd);
    int (*get_mac_address)(struct protocol_1_logic_manager *lm, unsigned char cmd);
    int (*get_battery)(struct protocol_1_logic_manager *lm);
    int (*handshake)(struct protocol_1_logic_manager *lm);
    int (*set_record_channel)(struct protocol_1_logic_manager *lm, int channel_size);
    int (*get_productNo)(struct protocol_1_logic_manager *lm);
    int (*get_protocol_version)(struct protocol_1_logic_manager *lm);
	int (*get_device_finger)(struct protocol_logic_manager *lm);
	int (*set_client_finger)(struct protocol_logic_manager *lm, unsigned char *data, int len);
    int (*set_authenticate)(struct protocol_1_logic_manager *lm, unsigned char *data, int size, unsigned char* session_key, unsigned char* mouse_finger, unsigned char* client_finger, unsigned char* random);
    int (*send_cmd)(struct protocol_1_logic_manager *lm, unsigned char cmd, unsigned char *data, int size);
    int (*add_data)(struct protocol_1_logic_manager *lm, unsigned char *data, int size);

};

void protocol_1_logic_manager_construct(struct protocol_1_logic_manager *plm);

#endif