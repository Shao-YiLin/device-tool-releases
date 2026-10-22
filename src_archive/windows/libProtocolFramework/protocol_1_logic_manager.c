/*
 * @Author: xuhan
 * @Date: 2020-02-06 23:17:45
 * @LastEditors  : xuhan
 * @LastEditTime : 2020-02-10 20:16:07
 * @Description:
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <Windows.h>
#include "mblue_heap.h"

#include "protocol_1_logic_manager.h"
#include "libprotocol_1_define.h"
#include "protocol_define.h"

static void _register_action_callback(struct protocol_1_logic_manager *lm, LOGIC_ACTION_CALLBACK callback, DEVICE_WRITE_CALLBACK write_callback, void *param)
{
	if (!lm)
		return;

	lm->action_listener = callback;
	lm->write_listener = write_callback;
	lm->cb_param = param;
}

static void __event_action(struct protocol_1_logic_manager *lm, unsigned char *data, unsigned int len)
{
	if (!lm || NULL == data || len < 5)
		return;

	unsigned char type = *data;
	unsigned int value = byteToInt(data + 1, 4);

	if (!lm->action_listener)
		return;

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
}

static unsigned char __change_2_action_cmd_type(unsigned char type)
{
	unsigned char action_cmd = 0;
	if (type == PRE_DPI)
	{
		action_cmd = DPI_DATA;
	}
	else if (type == PRE_VERSION_MOURSE)
	{
		action_cmd = VERSION_MOURSE_DATA;
	}
	else if (type == PRE_MAC_MOURSE)
	{
		action_cmd = MAC_MOURSE_DATA;
	}
	else if (type == PRE_VERSION_DONGLE)
	{
		action_cmd = VERSION_DONGLE_DATA;
	}
	else if (type == PRE_MAC_DONGLE)
	{
		action_cmd = MAC_DONGLE_DATA;
	}
	return action_cmd;
}

static int __get_productNo(int type)
{
	int productNo = 0x21 + type;

	//if (type == 1) 
	//{ // remake
	//    productNo = 0x22;
	//}
	//else if (type == 2)
	//{ // pro
	//    productNo = 0x23;
	//}
	//else if (type == 3)
	//{ // lite
	//    productNo = 0x24;
	//}
	return productNo;
}

static int __get_dongle_productNo(int type)
{
	int productNo = 0x12;
	return productNo;
}

static void __prop_respond(struct protocol_1_logic_manager *lm, unsigned char *data, unsigned int len)
{

	if (!lm || NULL == data || len < 8)
		return;

	if (!lm->action_listener)
		return;

	unsigned int err_code = byteToInt(data, 4);
	unsigned char type = *(data + 4);
	char value[16] = { 0 };
	memcpy(value, data + 5, len - 5);

	unsigned char action_cmd_type = __change_2_action_cmd_type(type);
	if (err_code == ERR_SUCCESS)
	{
		switch (action_cmd_type)
		{
		case VERSION_MOURSE_DATA:
		{
			int no = (value[0] & 0xf0) >> 4;
			no = __get_productNo(no);
			lm->action_listener(lm->cb_param, PRODUCT_NO, 0, 0, (char *)&no, 4);
			//Sleep(200);
			value[0] = value[0] & 0x0f;
			lm->action_listener(lm->cb_param, action_cmd_type, 0, 0, value, len - 5);
		}
		break;
		case VERSION_DONGLE_DATA:
		{
			int no = (value[0] & 0xf0) >> 4;
			no = __get_dongle_productNo(no);
			// lm->action_listener(lm->cb_param, DONGLE_PRODUCT_NO, 0, 0, (char *)&no, 4);
		  //   value[0] = value[0] & 0x0f;
			lm->action_listener(lm->cb_param, action_cmd_type, 0, 0, value, len - 5);
		}
		break;
		default:
			lm->action_listener(lm->cb_param, action_cmd_type, 0, 0, value, len - 5);
			break;
		}
	}
}

static unsigned char __change_2_action_cmd(unsigned char protocol_cmd)
{
	unsigned char cmd = 0;
	switch (protocol_cmd)
	{
	case PRE_COMMAND_START_RECORD_RESP:
		cmd = START_RECORD;
		break;
	case PRE_COMMAND_STOP_RECORD_RESP:
		cmd = STOP_RECORD;
		break;
	case PRE_COMMAND_SET_PROP_RESP:
		cmd = SET_DPI;
		break;
	case PRE_COMMAND_INIT_RESP:
		cmd = HANDSHAKE;
		break;
	default:
		break;
	}
	return cmd;
}

static void __common_resp(struct protocol_1_logic_manager *lm, unsigned char cmd, unsigned char *data, unsigned int len)
{
	if (NULL == data || len < 4 || !lm)
		return;

	unsigned int err_code = byteToInt(data, 4);

	unsigned char action_cmd = __change_2_action_cmd(cmd);
	if (lm->action_listener)
		lm->action_listener(lm->cb_param, action_cmd, err_code, 0, NULL, 0);
}

static void __login_resp(struct protocol_1_logic_manager *lm, unsigned char cmd, unsigned char *data, unsigned int len)
{
	if (NULL == data || len < 4 || !lm)
		return;

	unsigned int err_code = byteToInt(data, 4);
	if (err_code == 99) {
		err_code = 0;
	}

	unsigned char action_cmd = HANDSHAKE;
	if (lm->action_listener)
		lm->action_listener(lm->cb_param, action_cmd, err_code, 0, NULL, 0);
}

static void _protocol_cb(void *param, unsigned char cmd, unsigned char sequence, unsigned char *data, unsigned int size)
{
	if (!param)
		return;

	struct protocol_1_logic_manager *lm = param;

	switch (cmd)
	{
		/* case PRE_COMMAND_EVENT:
			 __event_action(lm, data, size);
			 break;*/
	case PRE_COMMAND_GET_PROP_RESP:
		__prop_respond(lm, data, size);
		break;
	case PRE_COMMAND_LOGIN_RESP:
		__login_resp(lm, cmd, data, size);
		//onLoginAction(data, len);
		break;
	case PRE_COMMAND_START_RECORD_RESP:
	case PRE_COMMAND_STOP_RECORD_RESP:
	case PRE_COMMAND_SET_PROP_RESP:
	case PRE_COMMAND_INIT_RESP:
		__common_resp(lm, cmd, data, size);
		break;
	default:
		break;
		/*  case PRE_COMMAND_ASYNC_DATA:
			  lm->action_listener(lm->cb_param, RECORD_DATA, *data, *(data + 1), (char *)(data + 2), size - 2);
			  break;*/
	}
}


//static DWORD WINAPI __protocol_thread_func(LPVOID param) {
//
//	struct protocol_1_logic_manager *mg = (struct protocol_1_logic_manager *)param;
//	if (NULL != mg)
//	{
//		mg->protocol_handle.start_service(&mg->protocol_handle);
//	}
//	return 0;
//}


static int _init(struct protocol_1_logic_manager *lm)
{
	lm->protocol_handle.init(&lm->protocol_handle, _protocol_cb, lm);
	//lm->protocol_handle.set_sleep(&lm->protocol_handle, 10);
	//LPDWORD dwThreadId = NULL;
	//lm->pthread_handle = CreateThread(NULL, 0, __protocol_thread_func, lm, 0, dwThreadId);
	//if (NULL == lm->pthread_handle)
 //   {
 //       log_d("create pthread error \n");
 //       lm->service_state = 0;
 //       return ERR_FAIL;
 //   }
 //   else
 //   {
 //       log_d("create protocol service success \n");
 //       lm->service_state = 1;
 //   }

	return ERR_SUCCESS;
}

//static void __stop_service(struct protocol_1_logic_manager *mg)
//{
//    // log_d("stop protocol service: %d \n", mg->service_state);
//    if (mg->service_state)
//    {
//        // log_d("stop protocol service \n");
//        mg->protocol_handle.stop_service(&mg->protocol_handle);
//        mg->service_state = false;
//    }
//
//	if (mg->pthread_handle)
//	{
//		// log_d("stop protocol service thread \n");
//	//	CloseHandle(mg->pthread_handle);
//		TerminateThread(mg->pthread_handle, 0);
//		mg->pthread_handle = 0;
//	}
//}

static int _uninit(struct protocol_1_logic_manager *lm)
{
	//   __stop_service(lm);
	lm->protocol_handle.uninit(&lm->protocol_handle);
	return ERR_SUCCESS;
}

static int _start_record(struct protocol_1_logic_manager *lm)
{
	char mode = 1;
	return lm->send_cmd(lm, START_RECORD, &mode, 1);
}

static int _stop_record(struct protocol_1_logic_manager *lm)
{
	return lm->send_cmd(lm, STOP_RECORD, NULL, 0);
}

static int _set_dpi(struct protocol_1_logic_manager *lm, int level)
{

	char buffer[5] = { 0 };
	buffer[0] = PRE_DPI;
	intToByte(level, buffer + 1);
	return lm->send_cmd(lm, SET_DPI, buffer, sizeof(buffer));
}

static int _get_dpi(struct protocol_1_logic_manager *lm)
{
	char action = PRE_DPI;
	return lm->send_cmd(lm, GET_DPI, &action, 1);
}

static int _get_version(struct protocol_1_logic_manager *lm, unsigned char cmd)
{
	char action = 0;
	if (cmd == VERSION_MOURSE_DATA)
		action = PRE_VERSION_MOURSE;
	else if (cmd == VERSION_DONGLE_DATA)
		action = PRE_VERSION_DONGLE;
	return lm->send_cmd(lm, cmd, &action, 1);
}

static int _get_mac_address(struct protocol_1_logic_manager *lm, unsigned char cmd)
{
	char action = 0;
	if (cmd == MAC_MOURSE_DATA)
	{
		action = PRE_MAC_MOURSE;
	}
	else if (cmd == MAC_DONGLE_DATA)
	{
		action = PRE_MAC_DONGLE;
	}

	return lm->send_cmd(lm, cmd, &action, 1);
}

static int _get_battery(struct protocol_1_logic_manager *lm)
{
	char action = PRE_BATTERY;
	return lm->send_cmd(lm, GET_BATTERY, &action, 1);
}

static int _handshake(struct protocol_1_logic_manager *lm)
{
	return lm->send_cmd(lm, HANDSHAKE, NULL, 0);
}

static int _set_record_channel(struct protocol_1_logic_manager *lm, int channel_size)
{
	char buffer[5] = { 0 };
	buffer[0] = PRE_RECORD_CHANNEL_SIZE;
	intToByte(channel_size, buffer + 1);
	return lm->send_cmd(lm, RECORD_CHANNEL, buffer, sizeof(buffer));
}

static int _get_productNo(struct protocol_1_logic_manager *lm)
{
	return ERR_SUCCESS;
}

/**
* current version not support this function, but need to respond
**/
static int _get_protocol_version(struct protocol_1_logic_manager *lm)
{

	if (lm->action_listener) {
		int product_version = 0;
		lm->action_listener(lm->cb_param, PROTOCOL_VERSION, ERR_SUCCESS, 0, (char *)&product_version, 4);
	}
	return ERR_SUCCESS;
}

static int _get_device_finger(struct protocol_1_logic_manager *lm) {
	return ERR_SUCCESS;
}

static int _set_client_finger(struct protocol_1_logic_manager *lm, unsigned char *data, int len) {
	return ERR_SUCCESS;
}

static int _set_authenticate(struct protocol_1_logic_manager *lm, unsigned char *data, int size, unsigned char* session_key, unsigned char* mouse_finger, unsigned char* client_finger, unsigned char* random)
{
	return ERR_SUCCESS;
}

static unsigned char __change_2_protocol_cmd(unsigned char action_cmd)
{
	unsigned char protocol_cmd = 0;

	switch (action_cmd)
	{
	case START_RECORD:
		protocol_cmd = PRE_COMMAND_START_RECORD;
		break;
	case STOP_RECORD:
		protocol_cmd = PRE_COMMAND_STOP_RECORD;
		break;
	case SET_DPI:
	case RECORD_CHANNEL:
		protocol_cmd = PRE_COMMAND_SET_PROP;
		break;
	case GET_DPI:
	case VERSION_MOURSE_DATA:
	case VERSION_DONGLE_DATA:
	case MAC_DONGLE_DATA:
	case MAC_MOURSE_DATA:
	case GET_BATTERY:
		protocol_cmd = PRE_COMMAND_GET_PROP;
		break;
	case HANDSHAKE:
		protocol_cmd = PRE_COMMAND_INIT;
		break;
	}

	return protocol_cmd;
}

static int _send_cmd(struct protocol_1_logic_manager *lm, unsigned char cmd, unsigned char *data, int size)
{
	unsigned char protocol_cmd = __change_2_protocol_cmd(cmd);

	struct libspeech_protocol_command_data command = { 0 };
	lm->protocol_handle.get_command(&lm->protocol_handle, protocol_cmd, data, size, &command);
	if (command.len)
	{
		if (lm->write_listener)
			lm->write_listener(lm->cb_param, command.data, command.len);
	}

	return ERR_SUCCESS;
}

static int _add_data(struct protocol_1_logic_manager *lm, unsigned char cmd, unsigned char sequence, unsigned char *data, int size)
{
	//  lm->protocol_handle.add_data(&lm->protocol_handle, data, size);
	_protocol_cb(lm, cmd, sequence, data, size);
	return ERR_SUCCESS;
}

static void __install_logic_operation(struct protocol_1_logic_manager *plm)
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

	libspeech_protocol_construct(&plm->protocol_handle);
}

void protocol_1_logic_manager_construct(struct protocol_1_logic_manager *plm)
{
	if (!plm)
		return;
	__install_logic_operation(plm);
}