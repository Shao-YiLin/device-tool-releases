#include "logic_protocol_manager.h"
#include "AAA_public_Config.h"
#include "mouse_effect_logic_manager.h"
#include "../../proj_lib/authentic.h"
#include "OTA_manager.h"

#define AUTHENTICATION_REG DEEP_ANA_REG1
mouse_propertie_param_arrays mouse_propertie_param;
//static bool authenticate_success_flag;
u32 device_protocol_version = 1;
u8 device_product_no[] = {'1', '0', '2', '3'};
static u8 protocol_start_record_sequence;
static u8 protocol_stop_record_sequence;
u32 forbid_authentication = 0;

extern u32 byteToInt(u8 *data, int len);
//extern u8 push_data_fifo_aaa(u8*, u8, u8);

ble_sts_t _protocol_notify_data(struct command_data *command)
{
	if (NULL == command)
		return HCI_ERR_UNKNOWN_CONN_ID;

	if (blc_ll_getCurrentState() == BLS_LINK_STATE_CONN || flash_dev_info.mode == USB_MODE)
	{
		//	push_data_fifo_aaa(command->pcData, HAS_PROT_REPORT, command->ucLen);
		push_data_out_fifo(command->pcData, OUT_PROT_REPORT, command->ucLen);
		return BLE_SUCCESS;
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
		event = _get_command_pack(sequence, mouse_version, 4);
	}
	else if (type == MAC_MOURSE)
	{
		event = _get_command_pack(sequence, mouse_mac_address, 6);
	}
#if (BATT_CHECK_ENABLE)
	else if (type == BATTERY)
	{
		if(charging_batt_count){
			event = get_protocol_manager_instance()->event_pack(TYPE_BATTERY, 5000);
		}
		else event = get_protocol_manager_instance()->event_pack(TYPE_BATTERY, batt_vol_mv_real);
	}
#endif
	else if (type == PRODUCT)
	{
		event = _get_command_pack(sequence, device_product_no, 4);
	}
	else if (SLEEP_TIME == type)
	{
		event = _get_command_pack(sequence, (u8 *)&flash_dpi_info.ble_time_out, 4);
	}
	else if (type == DPI)
	{
		u32 prop_dpi = flash_dpi_info.bt_dpi;
		event = _get_command_pack(sequence, (u8 *)&prop_dpi, 4);
	}
#if PWM_LED_ENABLE
	else if (type == GET_THEME) // get rbg value of theme
	{
		if(mouse_propertie_param.RGB_theme==RGB_CLOSE_MODE || mouse_propertie_param.RGB_theme==RGB_NORMAL_MODE)
		{
			u8 get_color[8] = {0};
			get_color[0] = mouse_propertie_param.RGB_theme;
			memcpy(get_color + 1, (u8 *)&mouse_propertie_param.breathe_RGB_param.cycle_time, 4);
			get_color[5] = mouse_propertie_param.userdefined_RGB_param.rgb_data.rgb_red;
			get_color[6] = mouse_propertie_param.userdefined_RGB_param.rgb_data.rgb_green;
			get_color[7] = mouse_propertie_param.userdefined_RGB_param.rgb_data.rgb_blue;
			event = _get_command_pack(sequence, (u8 *)&get_color, 8);
		}
		else
		{
			u8 get_color[22] = {0};
			get_color[0] = mouse_propertie_param.RGB_theme;
			memcpy(get_color + 1, (u8 *)&mouse_propertie_param.breathe_RGB_param.cycle_time, 4);
			memcpy(get_color + 5, &mouse_propertie_param.breathe_RGB_param.rgb_data,mouse_propertie_param.breathe_RGB_param.rgb_data_len * 3);
			event = _get_command_pack(sequence, (u8 *)get_color, mouse_propertie_param.breathe_RGB_param.rgb_data_len * 3 + 5);
		}
	}
#endif
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
			u8 ack_data[11] = {0};
			ack_data[0] = header->pcPayload[1];
			if(ack_data[0] == 1)
			{
				ack_data[1] = mouse_propertie_param.side_button1_param.flag;
				ack_data[2] = mouse_propertie_param.side_button1_param.type;
				memcpy(ack_data + 3, mouse_propertie_param.side_button1_param.data, 8);
			}
			else if(ack_data[0] == 2)
			{
				ack_data[1] = mouse_propertie_param.side_button2_param.flag;
				ack_data[2] = mouse_propertie_param.side_button2_param.type;
				memcpy(ack_data + 3, mouse_propertie_param.side_button2_param.data, 8);
			}

			event = _get_command_pack(sequence, (u8 *)ack_data, 11);
		}
		else{
			_action_resp(sequence, ERR_FAIL);
		}
	}
	else if(MOUSE_FREQUENCY == type)
	{
		u32 mouse_frequency;
		if(flash_dev_info.mode == USB_MODE)
		{
			mouse_frequency = flash_polling_rate.usb_frequency;
		}
		else if(flash_dev_info.mode == RF_1M_BLE_MODE)
		{
			mouse_frequency = (super_mouse_flag ? flash_polling_rate.ble_frequency : 125);
		}
		else
			return _action_resp(sequence, ERR_UNKNOW);
		event = _get_command_pack(sequence, (u8 *)&mouse_frequency, 4);
	}
	else
	{
		_action_resp(sequence, ERR_UNKNOW);
		return;
	}
	if(event != NULL)
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
			analog_write(AUTHENTICATION_REG, authenticate_success_flag);
		}
	}
}

#if SINCHECK_PCM_DATA_ENABLE
extern void start_sincheck();

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

u32 ifly_android_flag = 1;

static void _clear_all_index_pairs()
{
	flash_erase_sector(BONDING_FLASH_ADDR);
	flash_erase_sector(CFG_DEVICE_MODE_ADDR);
	clear_last_pair_device();
	clear_latest_mac_addr();
	remove_all();
}

static void _clear_all_users_setting()
{
	flash_erase_sector(MOUSE_DEVICE_DATA_ADDR);
	flash_erase_sector(MOUSE_DEVICE_DATA_ADDR1);
//	flash_erase_sector(CFG_ADR_MAC);
	flash_erase_sector(MOUSE_POLLING_RATE_ADDR);
	flash_erase_sector(MOUSE_DPI_ADDR);
	analog_write(AUTHENTICATION_REG, 0);
	analog_write(DEEP_ANA_REG9, 0);
}

static void _set_prop(struct protocol_header *header)
{
	if (header->xHead.ulLength > 0)
	{
		//	toHex(header->payload, header->head.length);
		u8 type = header->pcPayload[0];
		u8 data_len = header->xHead.ulLength - 1;
		//	printf("type: %d %d\r\n", type, data_len);
		if (type == DPI)
		{
			u8 value = byteToInt(header->pcPayload + 1, 4) & 0xff;
			if(flash_dpi_info.bt_dpi != value)
			{
				flash_dpi_info.bt_dpi = value ;
#if SENSOR_FUN_ENABLE_AAA
				save_sensor_info();
#endif
			}
		}
		else if (type == RECORD_CHANNEL)
		{
			if (data_len == 4)
			{
				u32 value = byteToInt(header->pcPayload + 1, 4);
				record_channel = value;
				//	printf("record_channel: %d\r\n");
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
			Set_web_flag();
			if (data_len == 4)
			{
				forbid_authentication = byteToInt(header->pcPayload + 1, 4);
			}
		}
		else if (RGB_EFFECTS == type) //转换灯光
		{
			set_rgb_param(header->pcPayload + 1, data_len);
			_action_resp(header->xHead.ucSequence, ERR_SUCCESS);
		}
		else if (CUSTOM_BUTTON == type) //按键按下指令切换
		{
			if (ui_mic_enable == 0)
			{
				if (data_len <= 11)
				{
					set_side_button_param(header->pcPayload + 1, data_len);
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
		}
		else if (SLEEP_TIME == type)
		{
			if (data_len == 4)
			{
				u32 sleep_time = byteToInt(header->pcPayload + 1, 4);
				if(flash_dpi_info.ble_time_out != sleep_time)
				{
					flash_dpi_info.ble_time_out = sleep_time;
					save_sensor_info();
				}
				_action_resp(header->xHead.ucSequence, ERR_SUCCESS);
				//				deep_sleep_time = value;
			}
		}
		else if (IFLY_ANDROID == type)
		{
			if (data_len == 4)
			{
				ifly_android_flag = byteToInt(header->pcPayload + 1, 4);
			}
		}
		else if(MOUSE_FREQUENCY == type)
		{
			if(data_len == 4)
			{
				u32 frequency = byteToInt(header->pcPayload + 1, 4);
				if(frequency < 125)
					frequency = 125;
				if(flash_dev_info.mode == RF_1M_BLE_MODE && super_mouse_flag)
				{
					if(frequency > 500)
						frequency = 500;
					if(flash_polling_rate.ble_frequency != frequency)
					{
						flash_polling_rate.ble_frequency = frequency;
						save_polling_rate_info();
					}
					_action_resp(header->xHead.ucSequence, ERR_SUCCESS);
				}
				else if(flash_dev_info.mode == USB_MODE)
				{
					if(frequency > 1000)
						frequency = 1000;
					if(flash_polling_rate.usb_frequency != frequency)
					{
						flash_polling_rate.usb_frequency = frequency;
						on_timer_wakeup();
						save_polling_rate_info();
					}
					_action_resp(header->xHead.ucSequence, ERR_SUCCESS);
				}
				else _action_resp(header->xHead.ucSequence, ERR_FAIL);
			}
			else _action_resp(header->xHead.ucSequence, ERR_FAIL);
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
					if(init_ota_register(0))
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

static void get_master_prop(struct protocol_header *header)
{
	if (header->xHead.ulLength > 0)
	{
		u8 type = header->pcPayload[0];
		// u8 data_len = header->xHead.ulLength - 1;
		if(type == MOUSE_FREQUENCY)
		{
			if(flash_dev_info.mode == RF_1M_BLE_MODE)
			{
				if(BLE_SUCCESS == _protocol_notify_data(get_protocol_manager_instance()->event_pack(TYPE_POLLING_RATE, flash_polling_rate.ble_frequency)))
				{
					super_mouse_flag = 1;
				}
			}
		//	else _action_resp(header->xHead.ucSequence, ERR_FAIL);
		}
	//	else _action_resp(header->xHead.ucSequence, ERR_UNKNOW);
	}
}

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
		//authenticate
		u32 reg_flag = ERR_SUCCESS;
		authenticate_success_flag = analog_read(AUTHENTICATION_REG);
		Set_web_flag();
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
	case GET_MASTER_PROP:
		get_master_prop(header);
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
