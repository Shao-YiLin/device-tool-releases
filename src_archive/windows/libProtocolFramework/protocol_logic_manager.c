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
#include <time.h>

#include "mblue_heap.h"
#include "mblue_aes.h"
#include "mblue_hash.h"
#include "mblue_md5.h"

#include "libprotocol_2_define.h"
#include "protocol_2_logic_manager.h"
#include "protocol_define.h"

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

	if (!lm->action_listener)
		return;

	unsigned char type = *data;
	unsigned int value = byteToInt(data + 1, 4);

	if (type == EVENT_TYPE_KEY)
	{
		unsigned int device_index = (value & 0xffff0000) >> 16;
		unsigned int device_status = (value & 0x0000ffff);
		if (device_status == EVENT_KEY_UP)
		{
			lm->action_listener(lm->cb_param, KEY_UP, device_index, 0, NULL, 0);
		}
		else if (device_status == EVENT_KEY_DOWN)
		{
			lm->action_listener(lm->cb_param, KEY_DOWN, device_index, 0, NULL, 0);
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
	else if (type == EVENT_TYPE_SINCHECK) {
		printf("event sincheck\r\n");
		lm->action_listener(lm->cb_param, SIN_CHECK, 0, 0, (char *)&value, sizeof(value));
	}
}

static void __reset_all_cmd_fifo(struct protocol_logic_manager *lm)
{
	if (lm)
	{
		int i;
		for (i = 0; i < NOTE_CACHE_SIZE; i++)
		{
			lm->item_node_array[i].sequence = -1;
			lm->item_node_array[i].cmd = 0;
			lm->item_node_array[i].tick = 0;
		}
	}
}

static long get_time_tick() {
	clock_t start = clock() / CLOCKS_PER_SEC;
	return start;
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
	EnterCriticalSection(&lm->mLock);
	
	for (i = 0; i < NOTE_CACHE_SIZE; i++)
	{
		if (lm->item_node_array[i].sequence == (int)sequence)
		{
			action_cmd = lm->item_node_array[i].cmd;
			lm->item_node_array[i].sequence = -1;
			lm->item_node_array[i].cmd = 0;
			lm->item_node_array[i].tick = 0;
			break;
		}
	}
	LeaveCriticalSection(&lm->mLock);
	return action_cmd;
}

static void is_command_exist(struct protocol_logic_manager *lm, unsigned sequence, unsigned char action_cmd) {

	int i;
	


	for (i = 0; i < NOTE_CACHE_SIZE; i++)
	{
		if (lm->item_node_array[i].cmd == (int)action_cmd)
		{
			lm->item_node_array[i].sequence = -1;
			lm->item_node_array[i].cmd = 0;
			lm->item_node_array[i].tick = 0;
			continue;
		}
		if ((lm->item_node_array[i].sequence == sequence) && (labs(get_time_tick() - lm->item_node_array[i].tick) > 5))
		{
			lm->item_node_array[i].sequence = -1;
			lm->item_node_array[i].cmd = 0;
			lm->item_node_array[i].tick = 0;
		}
	}
}

static bool __push_command_map(struct protocol_logic_manager *lm, unsigned char sequence, unsigned char action_cmd)
{
	int i;
	long tick_max = 0;
	int index;
	long curreent_time = get_time_tick();
	EnterCriticalSection(&lm->mLock);
	is_command_exist(lm, sequence, action_cmd);
	for (i = 0; i < NOTE_CACHE_SIZE; i++)
	{
		if (lm->item_node_array[i].sequence == -1)
		{
			lm->item_node_array[i].sequence = sequence;
			lm->item_node_array[i].cmd = action_cmd;
			lm->item_node_array[i].tick = curreent_time;
			LeaveCriticalSection(&lm->mLock);
			return true;
		}
		else if (labs(curreent_time - tick_max) < labs(curreent_time - lm->item_node_array[i].tick))
		{
			tick_max = lm->item_node_array[i].tick;
			index = i;
		}
	}
	lm->item_node_array[index].sequence = sequence;
	lm->item_node_array[index].cmd = action_cmd;
	lm->item_node_array[index].tick = curreent_time;
	LeaveCriticalSection(&lm->mLock);
	return true;
}

static void __clear_command_map(struct protocol_logic_manager *lm)
{
	EnterCriticalSection(&lm->mLock);
	for (int i = 0; i < NOTE_CACHE_SIZE; i++)
	{
		if (lm->item_node_array[i].sequence != -1)
		{
			lm->item_node_array[i].sequence = -1;
			lm->item_node_array[i].cmd = 0;
			lm->item_node_array[i].tick = 0;
		}
	}
	LeaveCriticalSection(&lm->mLock);
}

static void __prop_respond(struct protocol_logic_manager *lm, u8 action_cmd, u8 *data, u32 len)
{
	if (NULL == data || len < 4)
		return;

	if (!lm->action_listener)
		return;

	u32 err_code = byteToInt(data, 4);
	if (err_code != ERR_SUCCESS)
	{
		//printf("action_cmd: %d,%d \n", action_cmd, err_code);
		lm->action_listener(lm->cb_param, action_cmd, err_code, 0, NULL, 0);
		return;
	}

	char value[64] = { 0 };
	memcpy(value, data + 4, len - 4);
	lm->action_listener(lm->cb_param, action_cmd, err_code, 0, value, len - 4);
}

static void __ack(struct protocol_logic_manager *lm, u8 sequence, u8 *data, u32 len)
{
	if (NULL == data || len < 4)
		return;
	u32 err_code = byteToInt(data, 4);
	u8 action_cmd = __get_action_cmd(lm, sequence);
	if (action_cmd == 0)
		return;
	//printf("cmd = %d, len = %d \n", action_cmd, len);
	/*if (action_cmd == START_RECORD) {
		printf("start record respond \n");
	}*/
	if ((GET_CUSTOM_BTN == action_cmd) && (len != 15))//解决返回自定义按键键值错误问题
	{
		if (lm->action_listener)
			lm->action_listener(lm->cb_param, action_cmd, ERR_UNKNOW, 0, NULL, 0);
		return;
	}
	switch (action_cmd)
	{
	case HANDSHAKE:
	case START_RECORD:
	case STOP_RECORD:
	case CLIENT_FINGER:
	case RECORD_CHANNEL:
	case SLEEP_TIME:
	case LED_RGB:
	case VIBRATION:
	case SET_CUSTOM_BTN:
	case MANUFACTURE_INSTRUCTION:
	case SET_POLLING_RATE:
	case SET_LASER:
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
		/*   case COMMAND_EVENT:
			   __event_action(lm, data, size);
			   break;*/
	case COMMAND_ACK:
		__ack(lm, sequence, data, size);
		break;
		/* case COMMAND_ASYNC_DATA:
			 if (lm->action_listener)
				 lm->action_listener(lm->cb_param, RECORD_DATA, *data, *(data + 1), (char *)(data + 2), size - 2);
			 break;*/
	case COMMAND_HEART_HEAT:
		if (lm->action_listener)
			lm->action_listener(lm->cb_param, START_HEART, *data, *(data + 1), 0, 0);
		break;
	case COMMAND_AUTHENTICATE:
		__authenticate(lm, cmd, data, size);
		break;
	case COMMAND_MASTER_WRITE_RESP:
		if ((*data == 3) && *(data + 1) == 3)
		{
			lm->action_listener(lm->cb_param, DONGLE_PRODUCT_NO, 0, 0, data + 2, size - 2);
		}
	default:
		break;
	}
}


//static DWORD WINAPI __protocol_thread_func(LPVOID param) {
//
//	struct protocol_logic_manager *mg = (struct protocol_logic_manager *)param;
//	if (NULL != mg)
//	{
//		bool bRunFlag = true;
//		while (bRunFlag && NULL != mg->pCmdFifo) {
//			
//			int len = mg->pCmdFifo->len(mg->pCmdFifo);
//			if (len > 0) {
//				CmdNode_T note;
//				int size = mg->pCmdFifo->get(mg->pCmdFifo, (unsigned char *)&note, sizeof(CmdNode_T));
//				if (size) {
//					_protocol_cb(mg, note.cmd, note.sequence, note.data, note.size);
//				}
//			}
//			else {
//				Sleep(10);
//			}
//		}
//	}
//	return 0;
//}


static int _init(struct protocol_logic_manager *lm)
{
	/*if(NULL == lm->pCmdFifo )
		lm->pCmdFifo = mblue_fifo_alloc(sizeof(CmdNode_T) * 16);*/
	lm->protocol_handle.init(&lm->protocol_handle, _protocol_cb, lm);
	/*lm->protocol_handle.set_sleep(&lm->protocol_handle, 10);*/
	/*LPDWORD dwThreadId = NULL;
	lm->pthread_handle = CreateThread(NULL, 0, __protocol_thread_func, lm, 0, dwThreadId);*/

	InitializeCriticalSection(&lm->mLock);
	return ERR_SUCCESS;
}

static void __stop_service(struct protocol_logic_manager *mg)
{

	//if (mg->service_state) {
	//	// log_d("stop protocol service \n");
	//	mg->protocol_handle.stop_service(&mg->protocol_handle);
	//	mg->service_state = false;
	//}

  /*  if (mg->pthread_handle)
	{
		TerminateThread(mg->pthread_handle, 0);
		mg->pthread_handle = 0;
	}*/
}

static int _uninit(struct protocol_logic_manager *lm)
{
	// __stop_service(lm);
	lm->protocol_handle.uninit(&lm->protocol_handle);
	DeleteCriticalSection(&lm->mLock);
	//if (lm->pCmdFifo) {
	//	mblue_fifo_free(lm->pCmdFifo);
	//	lm->pCmdFifo = NULL;
	//}
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
	char buffer[5] = { 0 };
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
	else if(cmd == SLAVE_DEVICE_VERSION)
		action = PROP_KEY_SLAVE_DEVICE_VERSION;
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

//新增参数表明是否需要鉴权：value = 1需要鉴权；value = 0不需要鉴权
static int _handshake(struct protocol_logic_manager *lm)
{
	unsigned int value = 1;
	return lm->send_cmd(lm, HANDSHAKE, (unsigned char *)&value, 4);
}

static int _set_record_channel(struct protocol_logic_manager *lm, int channel_size)
{
	char buffer[5] = { 0 };
	buffer[0] = PROP_KEY_RECORD_CHANNEL_SIZE;
	intToByte(channel_size, buffer + 1);
	return lm->send_cmd(lm, RECORD_CHANNEL, buffer, sizeof(buffer));
}

static int _set_os_type(struct protocol_logic_manager *lm, int os_type)
{
	unsigned char buffer[5] = { 0 };
	buffer[0] = PROP_KEY_OS_TYPE;
	intToByte(os_type, buffer + 1);
	return lm->send_cmd(lm, OS_TYPE, (unsigned char *)buffer, sizeof(buffer));
}

static int _set_sleep_time(struct protocol_logic_manager *lm, int sleep_time)
{
	unsigned char buffer[5] = { 0 };
	buffer[0] = PROP_KEY_SLEEP_TIME;
	intToByte(sleep_time, buffer + 1);
	return lm->send_cmd(lm, SLEEP_TIME, (unsigned char *)buffer, sizeof(buffer));
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
	unsigned char respond_buf[64] = { 0 };
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
		// case SET_DPI:
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
	case SLEEP_TIME:
	case LED_RGB:
	case VIBRATION:
	case SET_CUSTOM_BTN:
	case GET_CUSTOM_BTN:
	case MANUFACTURE_INSTRUCTION:
	case THEME:
	case GET_FLIP_PAGE:
	case GET_SUBTITLE_STATUS:
	case GET_MOUSE_WOEK_MODE:
	case GET_MOUSE_CONNECTION_TYPE:
	case START_OTA:
	case UPDATE_OTA:
	case END_OTA:
	case SET_POLLING_RATE:
	case GET_POLLING_RATE:
	case GET_LED_INTENSITY:
	case SET_LED_INTENSITY:
	case SET_BACKHLIGHT:
	case SET_LASER:
	case SLAVE_DEVICE_VERSION:
	case SET_WHEEL_MODE:
	case GET_WHEEL_MODE:
	case PlAYER_PLAY:
	case SET_PLAYER_VOLUME:
	case GET_PLAYER_VOLUME:
	case SET_TIME_HEART_BEAT:
	case GET_TIME_HEART_BEAT:
	case SET_KEYBOARD_CUSTOM_RGB:
	case SET_ADAPTIVE_MODULATION:
	case GET_ADAPTIVE_MODULATION:
	case GET_LS_DEVICE_ID:
	case GET_LS_CHECK_SUM:
	//case GET_BACHLIGHT:
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
	case OS_TYPE:
	case SLEEP_TIME:
	case LED_RGB:
	case VIBRATION:
	case SET_CUSTOM_BTN:
	case FORBID_AUTHENTICATION:
	case SET_FLIP_PAGE:
	case SET_POLLING_RATE:
	case SET_LED_INTENSITY:
	case SET_BACKHLIGHT:
	case SET_LASER:
	case SET_WHEEL_MODE:
	case PlAYER_PLAY:
	case SET_PLAYER_VOLUME:
	case SET_TIME_HEART_BEAT:
	case SET_KEYBOARD_CUSTOM_RGB:
	case SET_ADAPTIVE_MODULATION:
		protocol_cmd = COMMAND_SET_SLAVE_PROP;
		break;
	case GET_DPI:
	case VERSION_MOURSE_DATA:
	case MAC_MOURSE_DATA:
	case GET_BATTERY:
	case PRODUCT_NO:
	case DEVICE_FINGER:
	case PROTOCOL_VERSION:
	case THEME:
	case GET_FLIP_PAGE:
	case GET_CUSTOM_BTN:
	case GET_SUBTITLE_STATUS:
	case GET_MOUSE_WOEK_MODE:
	case GET_MOUSE_CONNECTION_TYPE:
	case GET_POLLING_RATE:
	case GET_LED_INTENSITY:
	case GET_BACKHLIGHT:
	case GET_LASER:
	case SLAVE_DEVICE_VERSION:
	case GET_WHEEL_MODE:
	case GET_PLAYER_VOLUME:
	case GET_TIME_HEART_BEAT:
	case GET_ADAPTIVE_MODULATION:
	case GET_LS_DEVICE_ID:
	case GET_LS_CHECK_SUM:
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
	case MANUFACTURE_INSTRUCTION:
		protocol_cmd = COMMAND_MANUFACTURE_INSTRUCTION;
		break;
	case START_OTA:
	case UPDATE_OTA:
	case END_OTA:
		protocol_cmd = COMMAND_WRITE_DATA;
		break;
	case DONGLE_PRODUCT_NO:
		protocol_cmd = COMMAND_MASTER_WRITE;
		break;
	case START_HEART:
		protocol_cmd = COMMAND_HEART_HEAT;
	default:
		break;
	}
	return protocol_cmd;
}

static void printDex(unsigned char *data, int len)
{
	if (len == 0)
		return;

	for (int i = 0; i < len; i++)
	{
		printf("%02x", *(data + i) & 0xff);
	}
	printf("\n");
}

static int _send_cmd(struct protocol_logic_manager *lm, unsigned char cmd, unsigned char *data, int size)
{
	if (!lm->write_listener)
		return ERR_FAIL;
	unsigned char protocol_cmd = __change_2_protocol_cmd(cmd);
	int need_cache = __check_need_push_cache(cmd);

	/*if (cmd == START_RECORD) {
		printf("start record send: \n");
	}*/

	//printf("protocol_cmd: %d \n", protocol_cmd);
	struct libspeech_protocol_command_data command = { 0 };
	lm->protocol_handle.get_command(&lm->protocol_handle, protocol_cmd, data, size, &command);
	if (command.len)
	{
		// printDex(command->data, command->len);
		if (need_cache)
		{
			//	printf("push cmd = %d, seq = %d \n", cmd, command->sequence);
			if (!__push_command_map(lm, command.sequence, cmd))
				return ERR_FAIL;

		}

		if (lm->write_listener)
			lm->write_listener(lm->cb_param, command.data, command.len);
	}

	return ERR_SUCCESS;
}

static int _add_data(struct protocol_logic_manager *lm, unsigned char cmd, unsigned char sequence, unsigned char *data, int size)
{
	//  lm->protocol_handle.add_data(&lm->protocol_handle, data, size);

	  /*printf("_add_data: %d \n", sequence);

	  CmdNode_T info;
	  info.cmd = cmd;
	  info.sequence = sequence;
	  info.size = (unsigned short)size;
	  if (size > 0) {
		  memcpy(info.data, data, size);
	  }

	  if (lm->pCmdFifo) {
		  lm->pCmdFifo->put(lm->pCmdFifo, (unsigned char*)&info, sizeof(CmdNode_T));
	  }*/

	_protocol_cb(lm, cmd, sequence, data, size);
	return ERR_SUCCESS;
}

static int _set_led_rgb(struct protocol_logic_manager *lm, unsigned char type, unsigned int duration, unsigned char *data, int size)
{
	int len = 1 + 1 + 4 + size;
	unsigned char buffer[64] = { 0 };
	buffer[0] = PROP_KEY_RGB;
	buffer[1] = type;
	memcpy(buffer + 2, &duration, 4);
	memcpy(buffer + 6, data, size);
	return lm->send_cmd(lm, LED_RGB, buffer, len);
}

static int _set_vibration(struct protocol_logic_manager *lm, unsigned char status, unsigned char type, unsigned char level, unsigned int duration)
{
	int len = 1 + 7;
	unsigned char buffer[16] = { 0 };
	buffer[0] = PROP_KEY_VIBRATION;
	buffer[1] = status;
	buffer[2] = type;
	buffer[3] = level;
	memcpy(buffer + 4, &duration, 4);
	return lm->send_cmd(lm, VIBRATION, buffer, len);
}

static int _set_custom_key(struct protocol_logic_manager *lm, unsigned char type, unsigned char key_index, unsigned char key_type, unsigned char *data, unsigned int size)
{
	int len = 1 + 3 + size;
	unsigned char buffer[64] = { 0 };
	buffer[0] = PROP_KEY_CUSTOM_KEY;
	buffer[1] = key_index;
	buffer[2] = type;
	buffer[3] = key_type;
	memcpy(buffer + 4, data, size);
	return lm->send_cmd(lm, SET_CUSTOM_BTN, buffer, len);
}

static int _start_sincheck(struct protocol_logic_manager *lm) {
	int len = 1;
	unsigned char buffer[64] = { 0 };
	buffer[0] = INSTRUCTION_KEY_SINCHECK;
	return lm->send_cmd(lm, MANUFACTURE_INSTRUCTION, buffer, len);
}

static int _get_theme(struct protocol_logic_manager *lm) {
	char action = PROP_KEY_GET_THEME;
	return lm->send_cmd(lm, THEME, &action, 1);
}

static int _set_handshake_info(struct protocol_logic_manager *lm, unsigned int flag) {
	unsigned char buffer[5] = { 0 };
	buffer[0] = PROP_KEY_FORBID_AHTUENTICATE;
	intToByte(flag, buffer + 1);
	return lm->send_cmd(lm, FORBID_AUTHENTICATION, (unsigned char *)buffer, sizeof(buffer));
}

static int _set_flip_page(struct protocol_logic_manager *lm, unsigned int value) {
	unsigned char buffer[5] = { 0 };
	buffer[0] = PROP_KEY_SET_FLIP_PAGE;
	intToByte(value, buffer + 1);
	return lm->send_cmd(lm, SET_FLIP_PAGE, (unsigned char *)buffer, sizeof(buffer));
}

static int _get_flip_page(struct protocol_logic_manager *lm) {
	unsigned char buffer;
	buffer = PROP_KEY_GET_FLIP_PAGE;
	return lm->send_cmd(lm, GET_FLIP_PAGE, &buffer, 1);
}

static int _get_subtitle_status(struct protocol_logic_manager *lm) {
	unsigned char buffer;
	buffer = PROP_KEY_SUBTITLE_STATUS;
	return lm->send_cmd(lm, GET_SUBTITLE_STATUS, &buffer, 1);
}

static int _get_mouse_work_mode(struct protocol_logic_manager *lm) {
	unsigned char buffer;
	buffer = PROP_KEY_M610_WORK_MODE;
	return lm->send_cmd(lm, GET_MOUSE_WOEK_MODE, &buffer, 1);
}

static int _get_mouse_connection_type(struct protocol_logic_manager *lm)
{
	unsigned char buffer;
	buffer = PROP_KEY_DEVICE_CONNECTION_TYPE;
	return lm->send_cmd(lm, GET_MOUSE_CONNECTION_TYPE, &buffer, 1);
}

static int _get_custom_key(struct protocol_logic_manager *lm, u8 index)
{
	unsigned char buffer[2];
	buffer[0] = PROP_KEY_CUSTOM_KEY;
	buffer[1] = index;
	return lm->send_cmd(lm, GET_CUSTOM_BTN, buffer, 2);
}

static int _set_polling_rate(struct protocol_logic_manager *lm, unsigned int frequency)
{
	unsigned char buffer[5];
	buffer[0] = PROP_KEY_POLLING_RATE;
	memcpy(buffer + 1, &frequency, 4);
	return lm->send_cmd(lm, SET_POLLING_RATE, buffer, 5);
}

static int _get_polling_rate(struct protocol_logic_manager *lm)
{
	unsigned char buffer[1];
	buffer[0] = PROP_KEY_POLLING_RATE;
	return lm->send_cmd(lm, GET_POLLING_RATE, buffer, 1);
}

static int _start_ota(struct protocol_logic_manager *lm, unsigned char *session_key) {
	unsigned char buffer[18];
	buffer[0] = 1;
	buffer[1] = OTA_KEY_START;
	memcpy(buffer + 2, session_key, 16);
	return lm->send_cmd(lm, START_OTA, buffer, 18);
}

static int _update_ota_data(struct protocol_logic_manager *lm, unsigned char *firmware_data, unsigned int size) {
	unsigned char buffer[34];
	buffer[0] = 1;
	buffer[1] = OTA_KEY_UPDATE;
	memcpy(buffer + 2, firmware_data, size);
	return lm->send_cmd(lm, UPDATE_OTA, buffer, 34);
}

static int _end_ota(struct protocol_logic_manager *lm, unsigned char value, unsigned int firmware_crc_sum) {
	unsigned char buffer[7];
	buffer[0] = 1;
	buffer[1] = OTA_KEY_END;
	buffer[2] = value;
	memcpy(buffer + 3, (unsigned char *)&firmware_crc_sum, 4);
	return lm->send_cmd(lm, END_OTA, buffer, 7);
}

static int _get_master_product_no(struct protocol_logic_manager *lm)
{
	unsigned char buffer[2] = { 0 };
	buffer[0] = 0x3;
	buffer[1] = 3;
	return lm->send_cmd(lm, DONGLE_PRODUCT_NO, buffer, 2);
}

static int _get_led_intensity(struct protocol_logic_manager *lm,unsigned char index)
{
	unsigned char buffer[2] = { 0 };
	buffer[0] = PROP_KEY_LED;
	buffer[1] = index;
	return lm->send_cmd(lm, GET_LED_INTENSITY, buffer, 2);
}

static int _set_led_intensity(struct protocol_logic_manager *lm, uint8_t index, uint8_t value)
{
	unsigned char buffer[3] = { 0 };
	buffer[0] = PROP_KEY_LED;
	buffer[1] = index;
	buffer[2] = value;
	return lm->send_cmd(lm, SET_LED_INTENSITY, buffer, 3);
}
static int _get_rgb_backlight_param(struct protocol_logic_manager *lm, unsigned char index)
{
	unsigned char buffer[2] = { 0 };
	buffer[0] = PROP_KEY_BACKLIGHT;
	buffer[1] = index;
	return lm->send_cmd(lm, GET_BACKHLIGHT, buffer, 2);
}
static int _set_rgb_backlight_param(struct protocol_logic_manager *lm, uint8_t index, uint8_t value)
{
	unsigned char buffer[3] = { 0 };
	buffer[0] = PROP_KEY_BACKLIGHT;
	buffer[1] = index;
	buffer[2] = value;
	return lm->send_cmd(lm, SET_BACKHLIGHT, buffer, 3);
}
static int _get_laser_mode(struct protocol_logic_manager *lm)
{

	char action = PROP_KEY_LASER;
	return lm->send_cmd(lm, GET_LASER, &action, 1);
}
static int _set_laser_mode(struct protocol_logic_manager *lm, uint8_t value)
{
	unsigned char buffer[2] = { 0 };
	buffer[0] = PROP_KEY_LASER;
	buffer[1] = value;
	return lm->send_cmd(lm, SET_LASER,buffer, 2);
}

static int _set_wheel_mode(struct protocol_logic_manager *lm, uint8_t value)
{
	unsigned char buffer[5] = { 0 };
	buffer[0] = PROP_KEY_WHEEL_MODE;
	//buffer[1] = value;
	intToByte(value, buffer + 1);
	return lm->send_cmd(lm, SET_WHEEL_MODE, (unsigned char *)buffer, sizeof(buffer));

}

static int _get_wheel_mode(struct protocol_logic_manager *lm)
{
	unsigned char buffer[1] = { 0 };
	buffer[0] = PROP_KEY_WHEEL_MODE;
	return lm->send_cmd(lm, GET_WHEEL_MODE, buffer, 1);

}

static int _heart_beat(struct protocol_logic_manager *lm, uint8_t connect_status)
{
	unsigned char buffer[2] = { 0 };
	buffer[0] = PROP_KEY_HEART_HEAT;
	buffer[1] = connect_status;
	//intToByte(value, buffer + 1);
	return lm->send_cmd(lm, START_HEART, (unsigned char *)buffer, sizeof(buffer));

}

static int _player_play(struct protocol_logic_manager *lm, uint32_t tone_index)
{
	unsigned char buffer[5] = { 0 };
	buffer[0] = PLAY_ASR_PLAYER;
	//buffer[1] = value;
	intToByte(tone_index, buffer + 1);
	return lm->send_cmd(lm, PlAYER_PLAY, (unsigned char *)buffer, sizeof(buffer));

}

static int _set_player_volume(struct protocol_logic_manager *lm, uint32_t volume)
{
	unsigned char buffer[5] = { 0 };
	buffer[0] = ASR_VOLUME;
	//buffer[1] = value;
	intToByte(volume, buffer + 1);
	return lm->send_cmd(lm, SET_PLAYER_VOLUME, (unsigned char *)buffer, sizeof(buffer));

}

static int _get_player_volume(struct protocol_logic_manager *lm)
{
	unsigned char buffer = ASR_VOLUME;
	//buffer[0] = PROP_KEY_SET_DEVICE_ACTION;
	//buffer[1] = value;
//	intToByte(value, buffer + 1);
	return lm->send_cmd(lm, GET_PLAYER_VOLUME, &buffer, 1);

}

static int _set_time_heart_beat(struct protocol_logic_manager *lm, uint32_t time)
{
	unsigned char buffer[5] = { 0 };
	buffer[0] = HEART_RATE_TIME;
	intToByte(time, buffer + 1);
	return lm->send_cmd(lm, SET_TIME_HEART_BEAT, (unsigned char *)buffer, sizeof(buffer));

}

static int _get_time_heart_beat(struct protocol_logic_manager *lm)
{
	unsigned char buffer = HEART_RATE_TIME;
	return lm->send_cmd(lm, GET_TIME_HEART_BEAT, &buffer, 1);

}

static int _set_keyboard_custom_rgb(struct protocol_logic_manager *lm, unsigned char *data, unsigned int size)
{
	int len = 1 + 1 + size;
	unsigned char buffer[64] = { 0 };
	buffer[0] = PROP_KEYBOARD_RGB;
	buffer[1] = size / 4;
	memcpy(buffer + 2, data, size);
	//memcpy(buffer + 6, data, size);
	return lm->send_cmd(lm, SET_KEYBOARD_CUSTOM_RGB, buffer, len);

}

static int _set_adaptive_modulation(struct protocol_logic_manager *lm, unsigned int value)
{
	char buffer[5] = { 0 };
	buffer[0] = PROP_ADAPTIVE_MODULATION;
	intToByte(value, buffer + 1);
	return lm->send_cmd(lm, SET_ADAPTIVE_MODULATION, buffer, sizeof(buffer));

}
static int _get_adaptive_modulation(struct protocol_logic_manager *lm)
{
	char action = PROP_ADAPTIVE_MODULATION;
	return lm->send_cmd(lm, GET_ADAPTIVE_MODULATION, &action, 1);

}

static int _get_ls_device_id(struct protocol_logic_manager *lm)
{
	char action = PROP_LS_DEVICE_ID;
	return lm->send_cmd(lm, GET_LS_DEVICE_ID, &action, 1);

}

static int _get_ls_check_sum(struct protocol_logic_manager *lm, uint32_t time)
{
	unsigned char buffer[5] = { 0 };
	buffer[0] = PROP_LS_CHECK_SUM;
	intToByte(time, buffer + 1);
	return lm->send_cmd(lm, GET_LS_CHECK_SUM, (unsigned char *)buffer, sizeof(buffer));

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
	plm->set_os_type = _set_os_type;
	plm->set_sleep_time = _set_sleep_time;
	plm->get_productNo = _get_productNo;
	plm->get_protocol_version = _get_protocol_version;
	plm->set_authenticate = _set_authenticate;
	plm->set_ifly_authenticate = _set_ifly_authenticate;
	plm->send_cmd = _send_cmd;
	plm->register_action_callback = _register_action_callback;
	plm->add_data = _add_data;
	plm->set_client_finger = _set_client_finger;
	plm->get_device_finger = _get_device_finger;
	plm->set_led_rgb = _set_led_rgb;
	plm->set_vibration = _set_vibration;
	plm->set_custom_key = _set_custom_key;
	plm->start_sincheck = _start_sincheck;
	plm->get_theme = _get_theme;
	plm->set_handshake_info = _set_handshake_info;
	plm->set_flip_page = _set_flip_page;
	plm->get_flip_page = _get_flip_page;
	plm->get_subtitle_status = _get_subtitle_status;
	plm->get_mouse_work_mode = _get_mouse_work_mode;
	plm->get_mouse_connection_type = _get_mouse_connection_type;
	plm->get_custom_key = _get_custom_key;
	plm->set_polling_rate = _set_polling_rate;
	plm->get_polling_rate = _get_polling_rate;
	plm->start_ota = _start_ota;
	plm->update_ota_data = _update_ota_data;
	plm->end_ota = _end_ota;
	plm->get_led_intensity = _get_led_intensity;
	plm->set_led_intensity = _set_led_intensity;
	plm->get_rgb_backlight_param = _get_rgb_backlight_param;
	plm->set_rgb_backlight_param = _set_rgb_backlight_param;
	plm->get_laser_mode = _get_laser_mode;
	plm->set_laser_mode = _set_laser_mode;
	plm->set_wheel_mode = _set_wheel_mode;
	plm->get_wheel_mode = _get_wheel_mode;

	plm->heart_beat = _heart_beat;
	plm->player_play = _player_play;
	plm->set_player_volume = _set_player_volume;
	plm->get_player_volume = _get_player_volume;
	plm->set_time_heart_beat = _set_time_heart_beat;
	plm->get_time_heart_beat = _get_time_heart_beat;
	plm->set_keyboard_custom_rgb = _set_keyboard_custom_rgb;
	
	plm->set_adaptive_modulation = _set_adaptive_modulation;
	plm->get_adaptive_modulation = _get_adaptive_modulation;

	plm->get_ls_device_id = _get_ls_device_id;
	plm->get_ls_check_sum = _get_ls_check_sum;

	plm->action_listener = 0;
	plm->write_listener = 0;
	plm->get_dongle_product_no = _get_master_product_no;
	plm->clear_command_map = __clear_command_map;
	//plm->pCmdFifo = NULL;

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