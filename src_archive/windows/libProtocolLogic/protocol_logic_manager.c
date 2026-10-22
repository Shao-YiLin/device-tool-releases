/*
 * @Author: xuhan
 * @Date: 2020-02-06 23:17:45
 * @LastEditors  : xuhan
 * @LastEditTime : 2020-02-08 18:18:49
 * @Description: 
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <Windows.h>

#include "mblue_heap.h"
#include "mblue_aes.h"
#include "mblue_hash.h"
#include "mblue_md5.h"

#include "libprotocol_2_define.h"
#include "protocol_2_logic_manager.h"
#include "protocol_define.h"

// #define NOTE_CACHE_SIZE (5)

// typedef struct item_node
// {
//     int sequence;
//     int cmd;
// } item_node_t;

// static struct item_node item_node_array[NOTE_CACHE_SIZE] = {0};

static void _register_action_callback(struct protocol_logic_manager *lm, LOGIC_ACTION_CALLBACK callback, DEVICE_WRITE_CALLBACK write_callback, void *param)
{
    lm->action_listener = callback;
    lm->write_listener = write_callback;
    lm->cb_param = param;
}

static void __event_action(struct protocol_logic_manager *lm, unsigned char *data, unsigned int len)
{
    if (NULL == data || len < 5)
        return;

    if(!lm->action_listener)
        return;

    unsigned char type = *data;
    unsigned int value = byteToInt(data + 1, 4);

    if (type == EVENT_TYPE_KEY)
    {
        if (value == EVENT_KEY_UP)
        {
            lm->action_listener(lm->cb_param, KEY_UP, 0, 0, NULL, 0);
        }
        else if (value == EVENT_KEY_DOWN)
        {
            lm->action_listener(lm->cb_param, KEY_DOWN, 0, 0, NULL, 0);
        }
    }
    else if (type == EVENT_TYPE_BATTERY)
    {
        lm->action_listener(lm->cb_param, BATTER, 0, 0, (char *)&value, sizeof(value));
    }
    else if (type == EVENT_TYPE_BLE)
    {
        lm->action_listener(lm->cb_param, BLE_STATUS, 0, 0, (char *)&value, sizeof(value));
    }
}

/**
 * @description: 需要从map中获取对应的action指令
 * @param {type} 
 * @return: 
 */
static unsigned char __get_action_cmd(struct protocol_logic_manager *lm, unsigned char sequence)
{

    unsigned char action_cmd = 0;
    int i;
    for (i = 0; i < NOTE_CACHE_SIZE; i++)
    {
        item_node_t ele = lm->item_node_array[i];
        if (ele.sequence == sequence)
        {
            action_cmd = ele.cmd;
            lm->item_node_array[i].sequence = -1;
            lm->item_node_array[i].cmd = 0;
        }
    }

    return action_cmd;
}

static void __push_command_map(struct protocol_logic_manager *lm, unsigned char sequence, unsigned char action_cmd)
{
    int i;
    for (i = 0; i < NOTE_CACHE_SIZE; i++)
    {
        if (lm->item_node_array[i].sequence == -1)
        {
            lm->item_node_array[i].sequence = sequence;
            lm->item_node_array[i].cmd = action_cmd;
        }
    }
}

static void __prop_respond(struct protocol_logic_manager *lm, u8 action_cmd, u8 *data, u32 len)
{

    if (NULL == data || len < 4)
        return;
    
    if(!lm->action_listener)
        return;

    u32 err_code = byteToInt(data, 4);
    if (err_code != ERR_SUCCESS)
    {
        lm->action_listener(lm->cb_param, action_cmd, 0, 0, NULL, 0);
        return;
    }

    char value[64] = {0};
    memcpy(value, data + 4, len - 4);
    lm->action_listener(lm->cb_param, action_cmd, 0, 0, value, len - 4);
}

static void __ack(struct protocol_logic_manager *lm, u8 sequence, u8 *data, u32 len)
{
    if (NULL == data || len < 4)
        return;

    u32 err_code = byteToInt(data, 4);
    u8 action_cmd = __get_action_cmd(lm, sequence);
    //printf("cmd: %u, respond: %u \n", logicCmd, errCode);

    switch (action_cmd)
    {
    case HANDSHAKE:
    case START_RECORD:
    case STOP_RECORD:
    case SET_DPI:
	case CLIENT_FINGER:
    case RECORD_CHANNEL:
        if (lm->action_listener)
            lm->action_listener(lm->cb_param, action_cmd, err_code, 0, NULL, 0);
        break;
    default:
        __prop_respond(lm, action_cmd, data, len);
        break;
    }
}

static void __authenticate(struct protocol_logic_manager *lm, unsigned char cmd, unsigned char *data, unsigned int len)
{
    if (NULL == data || len < 1)
        return;
    unsigned char phase = *data;
    if (lm->action_listener)
        lm->action_listener(lm->cb_param, AUTHENTICATE, phase, 0, (char *)(data + 1), len - 1);
}

static void _protocol_cb(void *param, unsigned char cmd, unsigned char sequence, unsigned char *data, unsigned int size)
{
    struct protocol_logic_manager *lm = param;
    switch (cmd)
    {
    case COMMAND_EVENT:
        __event_action(lm, data, size);
        break;
    case COMMAND_ACK:
        __ack(lm, sequence, data, size);
        break;
    case COMMAND_ASYNC_DATA:
        if (lm->action_listener)
            lm->action_listener(lm->cb_param, RECORD_DATA, *data, *(data + 1), (char *)(data + 2), size - 2);
        break;
    case COMMAND_AUTHENTICATE:
        __authenticate(lm, cmd, data, size);
        break;
    default:
        break;
    }
}

//static void *__protocol_thread_func(void *param)
//{
//    struct protocol_logic_manager *mg = (struct protocol_logic_manager *)param;
//    if (NULL != mg)
//    {
//        mg->protocol_handle.start_service(&mg->protocol_handle);
//    }
//	return NULL;
//}

static DWORD WINAPI __protocol_thread_func(LPVOID param) {

	struct protocol_logic_manager *mg = (struct protocol_logic_manager *)param;
	if (NULL != mg)
	{
		mg->protocol_handle.start_service(&mg->protocol_handle);
	}
	return 0;
}


static int _init(struct protocol_logic_manager *lm)
{
    lm->protocol_handle.init(&lm->protocol_handle, _protocol_cb, lm);
	LPDWORD dwThreadId = NULL;
	lm->pthread_handle = CreateThread(NULL, 0, __protocol_thread_func, lm, 0, dwThreadId);
    if (NULL == lm->pthread_handle)
    {
        log_d("create pthread error \n");
        lm->service_state = 0;
        return ERR_FAIL;
    }
    else
    {
        log_d("create protocol service success \n");
        lm->service_state = 1;
    }

    return ERR_SUCCESS;
}

static void __stop_service(struct protocol_logic_manager *mg)
{
    if (mg->pthread_handle)
    {
        // log_d("stop protocol service thread \n");
		CloseHandle(mg->pthread_handle);
        mg->pthread_handle = 0;
    }
    // log_d("stop protocol service: %d \n", mg->service_state);
    if (mg->service_state)
    {
        // log_d("stop protocol service \n");
        mg->protocol_handle.stop_service(&mg->protocol_handle);
        mg->service_state = false;
    }
}

static int _uninit(struct protocol_logic_manager *lm)
{
    __stop_service(lm);
    lm->protocol_handle.uninit(&lm->protocol_handle);
    return ERR_SUCCESS;
}

static int _start_record(struct protocol_logic_manager *lm)
{
    char mode = 0;
    return lm->send_cmd(lm, START_RECORD, &mode, 1);
}

static int _stop_record(struct protocol_logic_manager *lm)
{
    return lm->send_cmd(lm, STOP_RECORD, NULL, 0);
}

static int _set_dpi(struct protocol_logic_manager *lm, int level)
{

    char buffer[5] = {0};
    buffer[0] = PROP_KEY_DPI;
    intToByte(level, buffer + 1);
    return lm->send_cmd(lm, SET_DPI, buffer, sizeof(buffer));
}

static int _get_dpi(struct protocol_logic_manager *lm)
{
    char action = PROP_KEY_DPI;
    return lm->send_cmd(lm, GET_DPI, &action, 1);
}

static int _get_version(struct protocol_logic_manager *lm, unsigned char cmd)
{
    char action = 0;
    if (cmd == VERSION_MOURSE_DATA)
        action = PROP_KEY_VERSION_MOURSE;
    else if (cmd == VERSION_DONGLE_DATA)
        action = PROP_KEY_VERSION_DONGLE;
    return lm->send_cmd(lm, cmd, &action, 1);
}

static int _get_mac_address(struct protocol_logic_manager *lm, unsigned char cmd)
{
    char action = 0;
    if (cmd == MAC_MOURSE_DATA)
    {
        action = PROP_KEY_MAC_MOURSE;
    }
    else if (cmd == MAC_DONGLE_DATA)
    {
        action = PROP_KEY_MAC_DONGLE;
    }

    return lm->send_cmd(lm, cmd, &action, 1);
}

static int _get_battery(struct protocol_logic_manager *lm)
{
    char action = PROP_KEY_BATTERY;
    return lm->send_cmd(lm, GET_BATTERY, &action, 1);
}

static int _handshake(struct protocol_logic_manager *lm)
{
    return lm->send_cmd(lm, HANDSHAKE, NULL, 0);
}

static int _set_record_channel(struct protocol_logic_manager *lm, int channel_size)
{
    char buffer[5] = {0};
    buffer[0] = PROP_KEY_RECORD_CHANNEL_SIZE;
    intToByte(channel_size, buffer + 1);
    return lm->send_cmd(lm, RECORD_CHANNEL, buffer, sizeof(buffer));
}

static int _get_productNo(struct protocol_logic_manager *lm)
{
    char action = PROP_KEY_PRODUCT_NO;
    return lm->send_cmd(lm, PRODUCT_NO, &action, 1);
}

static int _get_protocol_version(struct protocol_logic_manager *lm)
{
    char action = PROP_KEY_PROTOCOL_VERSION;
    return lm->send_cmd(lm, PROTOCOL_VERSION, &action, 1);
}

static int _get_device_finger(struct protocol_logic_manager *lm) {
	char action = PROP_KEY_FINGER;
	return lm->send_cmd(lm, DEVICE_FINGER, &action, 1);
}

static int _set_client_finger(struct protocol_logic_manager *lm, unsigned char *data, int len) {
	char buffer[17] = { 0 };
	buffer[0] = PROP_KEY_CLIENT_FINGER;
	memcpy(buffer + 1, data, len);
	return lm->send_cmd(lm, CLIENT_FINGER, buffer, sizeof(buffer));
}

static int _set_authenticate(struct protocol_logic_manager *lm, unsigned char *data, int size, unsigned char* session_key, unsigned char* mouse_finger, unsigned char* client_finger, unsigned char* random)
{
	if (!data || size == 0)
		return 0;

	unsigned char decrypt_buf[16] = { 0 };
	memcpy(decrypt_buf, data, 16);
	unsigned char device_random_buf[16] = { 0 };

	struct aes_ctx aesctx;
	struct mblue_aes aes;
	mblue_aes_construct(&aes);
	aes.init_ctx(&aes, &aesctx, session_key);
	aes.ecb_decrypt(&aes, &aesctx, decrypt_buf);
	for (int i = 0; i < 16; i++)
	{
		device_random_buf[i] = decrypt_buf[i] ^ mouse_finger[i];
	}
	
	unsigned char respond_buf[33] = { 0 };
	respond_buf[0] = PHASE2;
	unsigned char respond1[16] = { 0 };
	struct mblue_hash hash;
	mblue_hash_construct(&hash);

	unsigned int hash_finger = hash.sdbm_mem(&hash, mouse_finger, 16);
	unsigned char temp_buf[4 + 16] = { 0 };
	memcpy(temp_buf, &hash_finger, 4);
	memcpy(temp_buf + 4, device_random_buf, 16);

	md5_ctx md5ctx;
	struct mblue_md5 md5;
	mblue_md5_construct(&md5);
	md5.init(&md5, &md5ctx);
	md5.update(&md5, &md5ctx, temp_buf, sizeof(temp_buf));
	md5.final(&md5, &md5ctx, respond1);
	memcpy(respond_buf + 1, respond1, 16);

	unsigned char xor_value[16] = { 0 };
	for (int i = 0; i < 16; i++)
	{
		xor_value[i] = client_finger[i] ^ random[i];
	}
	aes.ecb_encrypt(&aes, &aesctx, xor_value);
	memcpy(respond_buf + 17, xor_value, 16);

   /* unsigned char respondBuf[64] = {0};
    respondBuf[0] = 0x2;
    memcpy(respondBuf + 1, data, size);*/

    return lm->send_cmd(lm, AUTHENTICATE, (char *)respond_buf, sizeof(respond_buf));
}

static int _set_ifly_authenticate(struct protocol_logic_manager *lm, unsigned char *data, int size) {

	unsigned char respond_buf[64] = {0};
	respond_buf[0] = PROP_KEY_IFLY_AUTHENTICATE;
	memcpy(respond_buf + 1, data, size);

	return lm->send_cmd(lm, IFLY_AUTHENTICATE, (char *)respond_buf, 1 + size);
}

static int __check_need_push_cache(unsigned char action_cmd)
{
    int push_flag = 0;

    switch (action_cmd)
    {
    case START_RECORD:
    case STOP_RECORD:
    case SET_DPI:
    case GET_DPI:
    case VERSION_MOURSE_DATA:
    case MAC_MOURSE_DATA:
    case HANDSHAKE:
    case RECORD_CHANNEL:
    case PRODUCT_NO:
	case DEVICE_FINGER:
	case CLIENT_FINGER:
    case PROTOCOL_VERSION:
    case VERSION_DONGLE_DATA:
    case MAC_DONGLE_DATA:
	case IFLY_AUTHENTICATE:
        push_flag = 1;
        break;
    default:
        break;
    }
    return push_flag;
}

static unsigned char __change_2_protocol_cmd(unsigned char action_cmd)
{
    unsigned char protocol_cmd = 0;

    switch (action_cmd)
    {
    case START_RECORD:
        protocol_cmd = COMMAND_START_RECORD;
        break;
    case STOP_RECORD:
        protocol_cmd = COMMAND_STOP_RECORD;
        break;
    case SET_DPI:
    case RECORD_CHANNEL:
	case CLIENT_FINGER:
	case IFLY_AUTHENTICATE:
        protocol_cmd = COMMAND_SET_SLAVE_PROP;
        break;
    case GET_DPI:
    case VERSION_MOURSE_DATA:
    case MAC_MOURSE_DATA:
    case GET_BATTERY:
    case PRODUCT_NO:
	case DEVICE_FINGER:
    case PROTOCOL_VERSION:
        protocol_cmd = COMMAND_GET_SLAVE_PROP;
        break;
    case VERSION_DONGLE_DATA:
    case MAC_DONGLE_DATA:
        protocol_cmd = COMMAND_GET_MASTER_PROP;
        break;
    case HANDSHAKE:
        protocol_cmd = COMMAND_HANDSHAKE;
        break;
    case AUTHENTICATE:
        protocol_cmd = COMMAND_AUTHENTICATE;
        break;
    default:
        break;
    }
    return protocol_cmd;
}

static int _send_cmd(struct protocol_logic_manager *lm, unsigned char cmd, unsigned char *data, int size)
{
    if (!lm->write_listener) 
        return ERR_FAIL;

    unsigned char protocol_cmd = __change_2_protocol_cmd(cmd);
    int need_cache = __check_need_push_cache(cmd);

    struct libspeech_protocol_command_data *command = lm->protocol_handle.get_command(&lm->protocol_handle, protocol_cmd, data, size);
    if (command)
    {
        if (need_cache)
        {
            //
            __push_command_map(lm, command->sequence, cmd);
        }

        if (lm->write_listener)
            lm->write_listener(lm->cb_param, command->data, command->len);
    }

    return ERR_SUCCESS;
}

static int _add_data(struct protocol_logic_manager *lm, unsigned char *data, int size)
{
    lm->protocol_handle.add_data(&lm->protocol_handle, data, size);
    return ERR_SUCCESS;
}

static void __install_logic_operation(struct protocol_logic_manager *plm)
{
    if (!plm)
        return;

    plm->init = _init;
    plm->uninit = _uninit;
    plm->start_record = _start_record;
    plm->stop_record = _stop_record;
    plm->set_dpi = _set_dpi;
    plm->get_dpi = _get_dpi;
    plm->get_version = _get_version;
    plm->get_mac_address = _get_mac_address;
    plm->get_battery = _get_battery;
    plm->handshake = _handshake;
    plm->set_record_channel = _set_record_channel;
    plm->get_productNo = _get_productNo;
    plm->get_protocol_version = _get_protocol_version;
    plm->set_authenticate = _set_authenticate;
    plm->send_cmd = _send_cmd;
    plm->register_action_callback = _register_action_callback;
    plm->add_data = _add_data;
	plm->set_client_finger = _set_client_finger;
	plm->get_device_finger = _get_device_finger;

    plm->action_listener = 0;
    plm->write_listener = 0;

    // memset(&item_node_array, 0, sizeof(item_node_array));
    int i;
    for (i = 0; i < NOTE_CACHE_SIZE; i++)
    {
         plm->item_node_array[i].sequence = -1;
         plm->item_node_array[i].cmd = 0;
    }

    libspeech_protocol_construct(&plm->protocol_handle);
}

void protocol_logic_manager_construct(struct protocol_logic_manager *plm)
{
    if (!plm)
        return;
    __install_logic_operation(plm);
}