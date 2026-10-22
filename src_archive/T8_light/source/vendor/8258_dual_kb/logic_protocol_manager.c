/*
 * @Descripttion: 
 * @version: 
 * @Author: hanxu
 * @Date: 2021-05-20 13:31:09
 * @LastEditors: xdhuang
 * @LastEditTime: 2021-05-28 09:53:45
 */
#include "logic_protocol_manager.h"
#include "AAA_public_Config.h"
#include "../../proj_lib/authentic.h"
#include "OTA_manager.h"
#include "custom_button_logic_manager.h"


u32 device_protocol_version = 1;
//static u32 device_product_no = 0x31;
u8 device_product_no[] = DEVICE_PRODUCT_NO;
static u8 protocol_start_record_sequence;
static u8 protocol_stop_record_sequence;
u32 forbid_authentication = 0;

extern u32 byteToInt(u8 *data, int len);
//extern u8 push_data_out_fifo(u8*, u8, u8);

ble_sts_t _protocol_notify_data(struct command_data *command)
{
	if (NULL == command)
		return HCI_ERR_UNKNOWN_CONN_ID;

	if (blc_ll_getCurrentState() == BLS_LINK_STATE_CONN || fun_mode == USB_MODE)
	{
		if(push_data_out_fifo(command->pcData, OUT_PROT_REPORT, command->ucLen))
			return BLE_SUCCESS;

		//	return bls_att_pushNotifyData(PROTOCOL_TX_DP_H, command->pcData, command->ucLen);
	}

	return HCI_ERR_CONN_FAILED_TO_ESTABLISH;
}

void _action_resp(u8 sequence, u32 err_code)
{
	struct command_data *event = get_protocol_manager_instance()->ack(
		sequence, err_code, NULL, 0);
	if (event)
	{
		if (BLE_SUCCESS == get_logic_protocol_manager()->send_notify_data(event))
		{
			//printf("resp: %d \n", sequence);
		}
	}
}

void _ack_resp(u8 sequence, u32 err_code, u8 *data, u32 len)
{
	struct command_data *event = get_protocol_manager_instance()->ack(
		sequence, err_code, data, len);
	if (event)
	{
		if (BLE_SUCCESS == get_logic_protocol_manager()->send_notify_data(event))
		{
		}
	}
}

struct command_data *_get_command_pack(u8 sequence, u8 *data, u32 len)
{
	return get_protocol_manager_instance()->get_prop_resp(
		sequence, ERR_SUCCESS, data, len);
}

bool _notify_data(struct command_data *data)
{
	if (data)
	{
		if (BLE_SUCCESS == get_logic_protocol_manager()->send_notify_data(data))
			return true;
	}
	return false;
}

void _prop_resp(struct protocol_header *header)
{
	if (NULL == header)
		return;

	if (header->xHead.ulLength == 0)
		return;

	u8 type = header->pcPayload[0];
	u8 sequence = header->xHead.ucSequence;

	struct command_data *event = NULL;

	if (type == VERSION_MOURSE)
	{
		event = _get_command_pack(sequence, keyboard_version, 4);
	}
	else if (type == MAC_MOURSE)
	{
		event = _get_command_pack(sequence, keyboard_mac_address, 6);
	}
#if (BATT_CHECK_ENABLE  && BATT_ENABLE_AAA)
	else if (type == BATTERY)
	{
		if(charging_flag){
			event = get_protocol_manager_instance()->event_pack(TYPE_BATTERY, 5000);
		}
		else event = get_protocol_manager_instance()->event_pack(TYPE_BATTERY, batt_vol_mv_real);
	}
#endif
	else if (type == PRODUCT)
	{
		event = _get_command_pack(sequence, device_product_no, 4);
	}
	else if(SLEEP_TIME == type)
	{
		event = _get_command_pack(sequence, (u8 *)&flash_keyboard_param.sleep_time, 4);
	}
	else if (type == FINGER)
	{
		u8 finger[16] = {0};
		u8 len = ucMblueGetFinger(finger);
		if (len == 16)
		{

			event = _get_command_pack(sequence, finger, len);
		}
		else
		{
			_action_resp(sequence, ERR_FAIL);
		}
	}
	else if (type == PROTOCOL_VERSION)
	{
		event = _get_command_pack(sequence, (u8 *)&device_protocol_version, 4);
	}
	else if(type == CUSTOM_BUTTON)
	{
		if (header->xHead.ulLength == 2)
		{
			u8 key_index = header->pcPayload[1];
			u8 ack_data[10] = {0};
			ack_data[0] = custom_button_param[key_index].flag;
			ack_data[1] = custom_button_param[key_index].type;
			memcpy(ack_data + 2, custom_button_param[key_index].value, 8);
			event = _get_command_pack(sequence, (u8 *)ack_data, 10);
		}
		else{
			_action_resp(sequence, ERR_FAIL);
		}

	}
	else if(type == CONNECTION_TYPE)
	{
		event = _get_command_pack(sequence, (u8 *)&fun_mode, 1);
	}
	else if(type == RGB_STATUS)
	{
		u32 notify_data = 0;
		u8 rgbStatus = header->pcPayload[1];
		if(rgbStatus == RGB_MODE)
			notify_data = (sonix_keyboard_instance()->kleds.mode<<8)+RGB_MODE;
		else if(rgbStatus == RGB_BRIGHTNESS)
			notify_data = ((sonix_keyboard_instance()->kleds.brightness*85)<<8)+RGB_BRIGHTNESS;
		else if(rgbStatus == RGB_SPEED)
			notify_data = (sonix_keyboard_instance()->kleds.speed<<8)+RGB_SPEED;
		event = get_protocol_manager_instance()->event_pack(TYPE_RGB_STATUS, notify_data);
	}
	else if(type == SONIX_VERSION)
	{
		sonix_keyboard_instance()->get_sonix_param(GET_SONIX_VERSION);
		event = _get_command_pack(sequence, sonix_keyboard_instance()->sonix_version, 2);
	}
	else if(type == WHEEL_FUNCTION)
	{
		u32 data = flash_keyboard_param.wheel_func;
		event = _get_command_pack(sequence, (u8 *)&data, 4);
	}
	else
	{
		_action_resp(sequence, ERR_UNKNOW);
		return;
	}

	get_logic_protocol_manager()->send_notify_data(event);
}

static void _authenticate_action(struct protocol_header *header)
{
	if (NULL == header)
		return;
	if (header->xHead.ulLength <= 1)
		return;
	u8 phase = header->pcPayload[0];
	u32 data_len = header->xHead.ulLength - 1;
	struct command_data *command = NULL;
	switch (phase)
	{
	case PHASE2:
		if (data_len >= 16)
		{
			if (ucCheckIdentificationClientSalt(header->pcPayload + 1, 16))
			{
				vSetIdentificationClientRandom(header->pcPayload + 17, data_len - 16);
				u8 salt[16] = {0};
				vGetIdentificationSalt(salt);
				command = get_protocol_manager_instance()->authenticate(header->xHead.ucSequence, PHASE3, salt, 16);
			}
			else
			{
				//	printf("phase2 check fail \n");
			}
		}
		break;
	case PHASE4:
		break;
	default:
		break;
	}

	if (NULL != command)
	{
		bool send_flag = _notify_data(command);
		if (PHASE2 == phase)
		{
			authenticate_success_flag = send_flag;
			ANALOG_WRITE(AUTHENTICATION_REG, authenticate_success_flag, 1);
		}
	}
}

#if SINCHECK_PCM_DATA_ENABLE
static void _set_mfins(struct protocol_header *header)
{
	if (header->xHead.ulLength > 0)
	{
		//	toHex(header->payload, header->head.length);
		u8 type = header->pcPayload[0];
	//	u8 data_len = header->xHead.ulLength - 1;
		if (SINCHECK_IFLY == type)
		{
			_action_resp(header->xHead.ucSequence, ERR_SUCCESS);
			start_sincheck();
		}
		else
		{
			_action_resp(header->xHead.ucSequence, ERR_UNKNOW);
		}
	}
}
#endif
static void _clear_all_index_pairs()
{
	flash_erase_sector(BONDING_FLASH_ADDR);
	clear_last_pair_device();
	clear_latest_mac_addr();
	remove_all();
}
static void _clear_all_users_setting()
{
	flash_erase_sector(0x7e000);	// CUSTOM_BUTTON_VALUE_ADDR
	flash_erase_sector(KEYBOARD_PARAM_ADDR);// 0x7a000 wheel
	flash_erase_sector(SONIX_RGB_INFO_ADDR);// 0x73000 light
}
static void _set_prop(struct protocol_header *header)
{
	if (header->xHead.ulLength > 0)
	{
		//	toHex(header->payload, header->head.length);
		u8 type = header->pcPayload[0];
		u8 data_len = header->xHead.ulLength - 1;

		if (type == RECORD_CHANNEL)
		{
			if (data_len == 4)
			{
				u32 value = byteToInt(header->pcPayload + 1, 4);
				record_channel = value;
			}
			_action_resp(header->xHead.ucSequence, ERR_SUCCESS);
		}
		else if (CLIENT_FINGER == type)
		{
			if (data_len == 16)
			{
				vSetIdentificationClientFinger(header->pcPayload + 1, 16);
			}
			_action_resp(header->xHead.ucSequence, ERR_SUCCESS);
		}
		else if (IFLY_AUTHEN == type)
		{
			if (data_len == 8)
			{
				u8 out_data[32] = {0};
				authentic(header->pcPayload + 1, 8, out_data, 32);
				_ack_resp(header->xHead.ucSequence, ERR_SUCCESS, out_data, 32);
				return;
			}
		}
		else if (FORBID_AHTUENTICATE == type)
		{
			authenticate_success_flag = 1;
			if (data_len == 4)
			{
				forbid_authentication = byteToInt(header->pcPayload + 1, 4);
			}
		}
		else if (CUSTOM_BUTTON == type) //按键按下指令切换
		{
			if (ui_mic_enable == 0)
			{
				if (data_len == 11)
				{
					set_custom_button_param(header->pcPayload + 1, data_len);
				}
				_action_resp(header->xHead.ucSequence, ERR_SUCCESS);
			}
			else
			{
				_action_resp(header->xHead.ucSequence, ERR_FAIL);
			}
		}
		else if (OS_TYPE == type)
		{
			if (data_len == 4)
			{
				u32 value = byteToInt(header->pcPayload + 1, 4);
				if (value <= 4)
					mode_keycode_flag = value;
			}
		//	_action_resp(header->xHead.ucSequence, ERR_SUCCESS);
		}else if(SLEEP_TIME == type){
			if(data_len == 4){
				u32 value = byteToInt(header->pcPayload + 1, 4);
				if(flash_keyboard_param.sleep_time != value)
				{
					flash_keyboard_param.sleep_time = value;
					save_keyboard_info_flash();
				}
				_action_resp(header->xHead.ucSequence, ERR_SUCCESS);
			}
			else
				_action_resp(header->xHead.ucSequence, ERR_FAIL);
		}
		else if(RGB_STATUS == type)
		{
			u8 type = *(header->pcPayload+1);
			u8 param = *(header->pcPayload+2);
    	    set_keyboard_up(1);
			if(type == RGB_MODE)
			{
				if(param == 0xff)
				{
					sonix_keyboard_instance()->set_rgb_param(SET_LEDS_MODE, param, FALSE);
				}
				else if(param <= RGB_MODE_NUM){
					sonix_keyboard_instance()->kleds.mode = param;
					sonix_keyboard_instance()->set_rgb_param(SET_LEDS_MODE, sonix_keyboard_instance()->kleds.mode, FALSE);// 唤醒MCU
					sleep_ms(2);
					sonix_keyboard_instance()->set_rgb_param(SET_LEDS_MODE, sonix_keyboard_instance()->kleds.mode, TRUE);
				}
				else
				{
					_action_resp(header->xHead.ucSequence, ERR_FAIL);
					return;
				}
			}
			else if(type == RGB_BRIGHTNESS)
			{
				if(param==0 || param==85 || param==170 || param==255)
				{
					sonix_keyboard_instance()->kleds.brightness = param/85;
					sonix_keyboard_instance()->set_rgb_param(SET_LEDS_BRIGHTNESS, sonix_keyboard_instance()->kleds.brightness, FALSE);
					sleep_ms(1);
					sonix_keyboard_instance()->set_rgb_param(SET_LEDS_BRIGHTNESS, sonix_keyboard_instance()->kleds.brightness, TRUE);
				}
				else
				{
					_action_resp(header->xHead.ucSequence, ERR_FAIL);
					return;
				}
			}
			else if(type == RGB_SPEED)
			{
				if(param<=3)
				{
					sonix_keyboard_instance()->kleds.speed = param;
					sonix_keyboard_instance()->set_rgb_param(SET_LEDS_SPEED, sonix_keyboard_instance()->kleds.speed, FALSE);
					sleep_ms(1);
					sonix_keyboard_instance()->set_rgb_param(SET_LEDS_SPEED, sonix_keyboard_instance()->kleds.speed, TRUE);
				}
				else
				{
					_action_resp(header->xHead.ucSequence, ERR_FAIL);
					return;
				}
			}
			else
			{
				_action_resp(header->xHead.ucSequence, ERR_FAIL);
				return;
			}
			_action_resp(header->xHead.ucSequence, ERR_SUCCESS);
		}
		else if(WHEEL_FUNCTION == type)
		{
			if(data_len == 4)
			{
				u32 value = byteToInt(header->pcPayload + 1, 4);
				flash_keyboard_param.wheel_func = value;
				save_keyboard_info_flash();
				_action_resp(header->xHead.ucSequence, ERR_SUCCESS);
			}
			else
				_action_resp(header->xHead.ucSequence, ERR_FAIL);
		}
		else if(DEVICE_ACTION == type)
		{
			if(data_len == 4)
			{
				u8 result = 0;
				u32 value = byteToInt(header->pcPayload + 1, 4);
				switch(value)
				{
				case 1:// all
					_clear_all_index_pairs();
					_clear_all_users_setting();
					result = 1;
					break;
				case 2:// channel
					_clear_all_index_pairs();
					result = 1;
					break;
				case 3:// user define
					_clear_all_users_setting();
					result = 1;
					break;
				default:
					result = 0;
					break;
				}
				if(result){
					_action_resp(header->xHead.ucSequence, ERR_SUCCESS);
					while(1);
				}
				else
					_action_resp(header->xHead.ucSequence, ERR_FAIL);
			}
			else
				_action_resp(header->xHead.ucSequence, ERR_FAIL);
		}
		else if(IFLY_ANDROID == type){
			if(data_len == 4){
				ifly_android_flag = byteToInt(header->pcPayload + 1, 4);
			}
		}
		else
		{
			_action_resp(header->xHead.ucSequence, ERR_UNKNOW);
		}
	}
}

#if PROTOCOL_OTA_ENABLE
static void _ota_logic(struct protocol_header *header)
{
	if (header->xHead.ulLength > 0){
		u8 type = header->pcPayload[0];
		if(type == 1){
			u8 ota_type = header->pcPayload[1];
			u8 ota_data_len = header->xHead.ulLength - 2;
			if(ota_type == START_OTA){
				u8 out_data[16] = {0};
				if(identification_client_session(&header->pcPayload[2], ota_data_len, out_data)){
					if(init_ota_register())
					{
						_ack_resp(header->xHead.ucSequence, ERR_SUCCESS, out_data, 16);
						return;
					}
				}
			}
			else if(ota_type == UPTATE_OTA){
				u8 firmware_data[32];
				u32 firmware_hash = identification_firmware(&header->pcPayload[2], ota_data_len, firmware_data);
				if(send_ota_data(firmware_data, ota_data_len))
				{
					_ack_resp(header->xHead.ucSequence, ERR_SUCCESS, (u8 *)&firmware_hash, 4);
					return;
				}
			}
			else if(ota_type == END_OTA){
				if(ota_data_len == 5){
					u32 firmware_hash = get_firmware_crc32_sum();
					if(!memcmp(&firmware_hash, &header->pcPayload[3], ota_data_len - 1)){
						if(enable_ota_data(header->pcPayload[2]))
						{
							_action_resp(header->xHead.ucSequence, ERR_SUCCESS);
							return;
						}
					}
				}
			}
		}
	}
	_action_resp(header->xHead.ucSequence, ERR_FAIL);
}
#endif

static u8 cmd_temp_cache[64] = {0};
static u8 cmd_cache_len = 0;
static void clear_cmd_cache()
{
	memset(cmd_temp_cache, 0x0, sizeof(cmd_temp_cache));
	cmd_cache_len = 0;
}

u8 _protocol_parse(u8 *data, u32 len)
{
	memcpy(cmd_temp_cache + cmd_cache_len, data, len);
	cmd_cache_len += len;
	struct protocol_header *header = get_protocol_manager_instance()->parse_pack(cmd_temp_cache, cmd_cache_len);
	if (!header)
	{
		if(cmd_cache_len >= 64)
			clear_cmd_cache();
		return 0;
	}

	else
	{
		clear_cmd_cache();
	}
	switch (header->xHead.ucCmd)
	{
	case START_RECORD:
	{
#if (AUTHENTICATION_FLAG)
		if (!authenticate_success_flag)
		{
			_action_resp(header->xHead.ucSequence, ERR_FAIL);
			break;
		}
#endif
		protocol_start_record_cmd = START_RECORD;
		protocol_start_record_sequence = header->xHead.ucSequence;
		break;
	}
	case STOP_RECORD:
	{
#if (AUTHENTICATION_FLAG)
		if (!authenticate_success_flag)
		{
			_action_resp(header->xHead.ucSequence, ERR_FAIL);
			break;
		}
#endif
		protocol_stop_record_cmd = STOP_RECORD;
		protocol_stop_record_sequence = header->xHead.ucSequence;
		break;
	}
	case GET_SLAVE_PROP:
		_prop_resp(header);
		break;
	case SET_SLAVE_PROP:
		_set_prop(header);
		break;
#if SINCHECK_PCM_DATA_ENABLE
	case GET_MF_INS:
		_set_mfins(header);
		break;
#endif
	case HANDSHAKE:
	{
		u32 reg_flag = ERR_SUCCESS;

		Set_web_flag();
		authenticate_success_flag = ANALOG_READ(AUTHENTICATION_REG,1);
		if(authenticate_success_flag)
		{
			reg_flag |= 0x10000;
			_action_resp(header->xHead.ucSequence, reg_flag);
			return 1;
		}
		_action_resp(header->xHead.ucSequence, reg_flag);
		if (forbid_authentication == 0)
		{
			unsigned char random[16] = {0};
			vGetIdentificationRandom(random);
			struct command_data *command = get_protocol_manager_instance()->authenticate(header->xHead.ucSequence, PHASE1, random, 16);
			if (NULL != command)
			{
				printf("start authen\n");
				_notify_data(command);
			}
		}
	}
	break;
	case AUTHENTICATE:
		_authenticate_action(header);
		break;
#if PROTOCOL_OTA_ENABLE
	case WRITE_DATA:
		_ota_logic(header);
		break;
#endif
	default:
		_action_resp(header->xHead.ucSequence, ERR_UNKNOW);
		break;
	}

	//  _cmd_respond(header->head.cmd, ERR_SUCCESS);
	return 1;
}

void _start_record_respond(u32 error_code)
{
	_action_resp(protocol_start_record_sequence, error_code);
}

void _stop_record_respond(u32 error_code)
{
	_action_resp(protocol_stop_record_sequence, error_code);
}

static struct logic_protocol_manager *instance;
static struct logic_protocol_manager logic_manager =
{
	.start_record_respond = _start_record_respond,
	.stop_record_respond = _stop_record_respond,
	.send_notify_data = _protocol_notify_data,
	.parse = _protocol_parse,
	.clear_cache = clear_cmd_cache,
};

struct logic_protocol_manager *get_logic_protocol_manager()
{
	if (NULL == instance)
		instance = &logic_manager;
	return instance;
}
