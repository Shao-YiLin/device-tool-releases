
#include "AAA_public_config.h"
#include "keyboard_scan.h"
#include "logic_protocol_manager.h"
#include "custom_button_logic_manager.h"
#include "../../third/protocol/protocol_define.h"
#include "../../third/protocol/protocol_manager.h"

#define NKRO_KRYBOARD_SIZE 14
#define SIXKRO_KRYBOARD_SIZE 8
struct record_all_key_param
{
	u32 time_tick;
	u32 duration_ms;
	u8 last_status;
	u8 current_status;
	u8 timer_en;
	u8 reserve;
};

typedef enum
{
	CONSUMER,
	RESET,
	CHANNEL1,
	CHANNEL2,
	CHANNEL3,
	TRANS_LATE,
	VOICE,
	OCR,
	DIATATION,
	FN_LOCK,
	BOOTLOADER,
	MAX_KEY
}KEY_INDEX;

typedef enum
{
	KEY_STATE_UP,
	KEY_STATE_DOWN,
}KEY_STATE;

static struct record_all_key_param all_key_flag[MAX_KEY];

static inline void set_key_current_status(KEY_INDEX index, KEY_STATE state)
{
	all_key_flag[index].current_status = state;
}

static void start_key_timer(KEY_INDEX index)
{
	all_key_flag[index].timer_en |= 1;
	all_key_flag[index].time_tick = clock_time();
	all_key_flag[index].duration_ms = 0;
}

static inline void disable_key_timer(KEY_INDEX index)
{
	all_key_flag[index].timer_en = 0;
}

static inline KEY_STATE get_key_last_status(KEY_INDEX index)
{
	return all_key_flag[index].last_status;
}

static u32 get_key_state_duration(KEY_INDEX index, KEY_STATE state) //
{
	if((all_key_flag[index].last_status == state) && all_key_flag[index].timer_en)
		return all_key_flag[index].duration_ms;
	return 0;
}

static void key_status_loop()
{
	u32 current_time = clock_time();
	for(int i = 0; i < MAX_KEY; i++)
	{
		if(all_key_flag[i].timer_en)
		{
			if((current_time - all_key_flag[i].time_tick) > CLOCK_16M_SYS_TIMER_CLK_1S)
			{
				all_key_flag[i].time_tick += CLOCK_16M_SYS_TIMER_CLK_1S;
				all_key_flag[i].duration_ms++;
			}
		}
	}
}

extern void voice_btn_press(bool bpressed, u32 index);
//static u8 voice_key_flag = 0;
static u8 custom_key_value[SCAN_KEYBOARD_SIZE];
u32 ifly_android_flag = 1;

// Key-value conversion management
static inline bool _key_is_pressed(u8 index, u8 *data, u8 size)
{
	return (data[VK_ADDR(index)] & VK_VALUE(index)) ? true : false;
}
static inline void _reset_key_value(u8 index, u8 *data, u8 size)
{
	data[VK_ADDR(index)] &= ~VK_VALUE(index);
}
static inline void _add_key_value(u8 index, u8 *data, u8 size)
{
	data[(index - 1) >> 3] |= (1 << ((index - 1) & 0x07));
}

static void __send_keyboard_data(u8 *data, int size)
{
	push_data_out_fifo(data, OUT_KEYBOARD_REPORT, size);
}

static u8 current_keyboard_key_nkro[NKRO_KRYBOARD_SIZE] = {0};
static u8 current_keyboard_key_sixkro[SIXKRO_KRYBOARD_SIZE] = {0};

static void _send_keyboard_data(u8 *data, int size)
{
	if(fun_mode == USB_MODE || keyboard_data_mode==NKRO)// USB模式或蓝牙模式的全键无冲
	{
		if(data == NULL || size != NKRO_KRYBOARD_SIZE)
			return;
		if(memcmp(current_keyboard_key_nkro, data, size))
		{
			__send_keyboard_data(data, size);
			memcpy(current_keyboard_key_nkro, data, NKRO_KRYBOARD_SIZE);
		}
	}
	else if(fun_mode == RF_1M_BLE_MODE && keyboard_data_mode==SIXKRO)// 蓝牙模式6键无冲
	{
		if(data == NULL || size != SIXKRO_KRYBOARD_SIZE)
			return;
		if(memcmp(current_keyboard_key_sixkro, data, size))
		{
			__send_keyboard_data(data, size);
			memcpy(current_keyboard_key_sixkro, data, SIXKRO_KRYBOARD_SIZE);
		}
	}
}


static void __send_consumer_data(u8 *data, int size)
{
	push_data_out_fifo(data, OUT_CONSUMER_REPORT, size);
}

static u8 current_consumer_key[2] = {0};
static void _send_consumer_data(u8 *data, int size)
{
	if(memcmp(data, current_consumer_key, size))
	{
		memcpy(current_consumer_key, data, size);
		__send_consumer_data(data, size);
	}
	set_key_current_status(CONSUMER, KEY_STATE_DOWN);
}

static void set_manu_os_type(u8 os_type)
{
	if(mode_keycode_flag==os_type)
		return ;
	set_os_type(os_type);
	if(fun_mode == RF_1M_BLE_MODE)
		force_save_current_device(TRUE);
}

void _special_key_proc(u8 *data, u8 size)
{
	if(_key_is_pressed(MS_W, data, size))
	{
//		_reset_key_value(MS_W, data, size);
		set_manu_os_type(OS_TYPE_WINDOWS);
//		led_com_mode_change();
	}
	else if(_key_is_pressed(MS_A, data, size))
	{
//		_reset_key_value(MS_A, data, size);
		set_manu_os_type(OS_TYPE_ANDROID);
//		led_com_mode_change();
	}
	else if(_key_is_pressed(MS_I, data, size))
	{
//		_reset_key_value(MS_A, data, size);
		set_manu_os_type(OS_TYPE_ANDROID);
//		led_com_mode_change();
	}
	else if(_key_is_pressed(MS_M, data, size))
	{
//		_reset_key_value(MS_M, data, size);
		set_manu_os_type(OS_TYPE_MAC);
//		led_com_mode_change();
	}

}

static inline u8 _is_reset_pairs_proc(u8 *data,u8 size)
{
	return _key_is_pressed(MS_C, data, size) && _key_is_pressed(MS_L, data, size);
}

static void __lock_key_action(u8 *data, u8 size)
{
	if (OS_TYPE_MAC == mode_keycode_flag)
	{
		_add_key_value(MS_Q,data,size);
		_add_key_value(MS_LCtrl,data,size);
		_add_key_value(MS_LWin,data,size);
	}
	else if (OS_TYPE_WINDOWS == mode_keycode_flag)
	{
		_add_key_value(MS_L,data,size);
		_add_key_value(MS_LWin,data,size);
	}
}

void reset_multi_key_value(u8 *data, u8 size){
	if(data == NULL || size != 15)
		return;
	_reset_key_value(MS_F1, data, size);
	_reset_key_value(MS_F2, data, size);
	_reset_key_value(MS_F3, data, size);
	_reset_key_value(MS_F4, data, size);
	_reset_key_value(MS_F5, data, size);
	_reset_key_value(MS_F6, data, size);
	_reset_key_value(MS_F7, data, size);
	_reset_key_value(MS_F8, data, size);
	_reset_key_value(MS_F9, data, size);
	_reset_key_value(MS_F10, data, size);
	_reset_key_value(MS_F11, data, size);
	_reset_key_value(MS_F12, data, size);
}

#define CONSUMER_SIZE (12)
static u16 function_key_consumer[CONSUMER_SIZE] = {
	0X0000,
	0x0223, // home
	0x0221, // search
	0x0070,	// Brightness Decrement
	0x006f, // Brightness Increment
	0x00b6, // pre
	0x00cd, // play & pause
	0x00b5, // next
	0x0224,  // back(android)
	0x019e  // sleep(android)
};

static u8 function_key_proc(u8 *data, u8 size)
{
	u8 consumer_key_index = 0;
	u8 fn_special_keycode[2] = {0};
	if(_key_is_pressed(MS_F1, data, size)){
		reset_multi_key_value(data, size);
		consumer_key_index = 1;
	}
	else if(_key_is_pressed(MS_F2, data, size)){
		reset_multi_key_value(data, size);
		consumer_key_index = 2;
	}
	else if(_key_is_pressed(MS_F3, data, size)){
		reset_multi_key_value(data, size);
		consumer_key_index = 3;
	}
	else if(_key_is_pressed(MS_F4, data, size)){
		reset_multi_key_value(data, size);
		consumer_key_index = 4;
	}
	else if(_key_is_pressed(MS_F5, data, size)){
		reset_multi_key_value(data, size);
		consumer_key_index = 5;
	}
	else if(_key_is_pressed(MS_F6, data, size)){
		reset_multi_key_value(data, size);
		consumer_key_index = 6;
	}
	else if(_key_is_pressed(MS_F7, data, size)){
		reset_multi_key_value(data, size);
		consumer_key_index = 7;

	}
	else if(_key_is_pressed(MS_F8, data, size)){
		reset_multi_key_value(data, size);
		if (OS_TYPE_WINDOWS == mode_keycode_flag)
		{
			custom_key_value[(MS_Left - 1) >> 3] |= (1 << ((MS_Left - 1) & 0x07));
			custom_key_value[(MS_LAlt - 1) >> 3] |= (1 << ((MS_LAlt - 1) & 0x07));
			return 1;
		}
		else if (OS_TYPE_ANDROID == mode_keycode_flag)
		{
			consumer_key_index = 8;
		}
		else
		{
			consumer_key_index = 1;
		}
	}
	else if(_key_is_pressed(MS_F9, data, size))
	{
		reset_multi_key_value(data, size);
		if (OS_TYPE_ANDROID == mode_keycode_flag)
		{
			consumer_key_index = 9;
		}
		else
			__lock_key_action(data,size);
	}
	if(fun_mode == RF_1M_BLE_MODE)
	{
		if(_key_is_pressed(MS_F10, data, size))
		{
			reset_multi_key_value(data, size);
			set_key_current_status(CHANNEL1, KEY_STATE_DOWN);
			if(get_key_last_status(CHANNEL1) == KEY_STATE_UP)
				start_key_timer(CHANNEL1);
			return 1;
		}
		else if(_key_is_pressed(MS_F11, data, size))
		{

			reset_multi_key_value(data, size);
			set_key_current_status(CHANNEL2, KEY_STATE_DOWN);
			if(get_key_last_status(CHANNEL2) == KEY_STATE_UP)
				start_key_timer(CHANNEL2);
			return 1;
		}
		else if(_key_is_pressed(MS_F12, data, size))
		{
			reset_multi_key_value(data, size);
			set_key_current_status(CHANNEL3, KEY_STATE_DOWN);
			if(get_key_last_status(CHANNEL3) == KEY_STATE_UP)
				start_key_timer(CHANNEL3);
			return 1;
		}

	}
	if (consumer_key_index > 0 && consumer_key_index < CONSUMER_SIZE)
	{
		memcpy(fn_special_keycode, (u8 *)&function_key_consumer[consumer_key_index], 2);
		_send_consumer_data(fn_special_keycode, sizeof(fn_special_keycode));
		return 1;
	}
	return 0;
}

static u32 get_brightness_tick = 0;
static u8 get_brightness_time = 0;
void set_brightness(u8 brightness)
{
	u8 time = 0;
	do{
		sonix_keyboard_instance()->set_rgb_param(SET_LEDS_BRIGHTNESS, brightness, FALSE);
		sonix_keyboard_instance()->get_sonix_param(GET_LEDS_BRIGHTNESS);
		time++;
	}while(time<=5 && sonix_keyboard_instance()->kleds.brightness != brightness);// 如果连续设置5次都失败就算了
	if(sonix_keyboard_instance()->kleds.brightness == brightness)
	{
		write_rgb_flash(sonix_keyboard_instance()->kleds);
		u32 notify_data = ((sonix_keyboard_instance()->kleds.brightness*85)<<8)+RGB_BRIGHTNESS;
		get_logic_protocol_manager()->send_notify_data(get_protocol_manager_instance()->event_pack(TYPE_RGB_STATUS,notify_data));
	}
}
void mode_set(u8 mode)
{
	sonix_keyboard_instance()->set_rgb_param(SET_LEDS_MODE, mode, TRUE);
	u32 notify_data = (sonix_keyboard_instance()->kleds.mode<<8)+RGB_MODE;
	get_logic_protocol_manager()->send_notify_data(get_protocol_manager_instance()->event_pack(TYPE_RGB_STATUS,notify_data));
	// 切换灯光时亮度变为2
	if(sonix_keyboard_instance()->kleds.brightness == 0)
	{
		sonix_keyboard_instance()->kleds.brightness = 2;
		sonix_keyboard_instance()->set_rgb_param(SET_LEDS_BRIGHTNESS, DEFAULT_BRIGHTNESS, TRUE);
		notify_data = ((sonix_keyboard_instance()->kleds.brightness*85)<<8)+RGB_BRIGHTNESS;
		get_logic_protocol_manager()->send_notify_data(get_protocol_manager_instance()->event_pack(TYPE_RGB_STATUS,notify_data));
	}
	get_brightness_tick = clock_time()|1;
	get_brightness_time = 0;

}
// 灯光设置快捷键

u8 bootLoaderBuffer = 0;
u32 bootLoader_tick = 0;
void led_key_proc(u8 *data, u8 size)
{
	// 模式
	static u8 button_flag = 0;
	u32 notify_data = 0;
	if (_key_is_pressed(MS_Pgup, data, size))
	{
		_reset_key_value(MS_Pgup, data, size);
		if((button_flag&BIT(0)) == 0)//解决一个按键多个信号的BUG
		{
			sonix_keyboard_instance()->kleds.mode = sonix_keyboard_instance()->kleds.mode==RGB_MODE_NUM?0:sonix_keyboard_instance()->kleds.mode+1;
			mode_set(sonix_keyboard_instance()->kleds.mode);

			button_flag |= BIT(0);
		}
	}else if((button_flag&BIT(0)))
		button_flag &= ~BIT(0);

	if (_key_is_pressed(MS_Pgdn, data, size))
	{
		_reset_key_value(MS_Pgdn, data, size);
		if((button_flag&BIT(1)) == 0)
		{
			sonix_keyboard_instance()->kleds.mode = sonix_keyboard_instance()->kleds.mode==0?RGB_MODE_NUM:sonix_keyboard_instance()->kleds.mode-1;
			mode_set(sonix_keyboard_instance()->kleds.mode);

			button_flag |= BIT(1);
		}
	}else if((button_flag&BIT(1)))
		button_flag &= ~BIT(1);

	// 速度
	if (_key_is_pressed(MS_Insert, data, size))
	{
		_reset_key_value(MS_Insert, data, size);
		if((button_flag&BIT(2)) == 0)//解决一个按键多个信号的BUG
		{
			sonix_keyboard_instance()->kleds.speed = (sonix_keyboard_instance()->kleds.speed<3)?(sonix_keyboard_instance()->kleds.speed+1):3;
			sonix_keyboard_instance()->set_rgb_param(SET_LEDS_SPEED, sonix_keyboard_instance()->kleds.speed, TRUE);
			notify_data = (sonix_keyboard_instance()->kleds.speed<<8)+RGB_SPEED;
			get_logic_protocol_manager()->send_notify_data(get_protocol_manager_instance()->event_pack(TYPE_RGB_STATUS,notify_data));

			button_flag |= BIT(2);
		}
	}
	else if((button_flag&BIT(2)))
		button_flag &= ~BIT(2);

	if (_key_is_pressed(MS_Delete, data, size))
	{
		_reset_key_value(MS_Delete, data, size);
		if((button_flag&BIT(3)) == 0)//解决一个按键多个信号的BUG
		{
			sonix_keyboard_instance()->kleds.speed = (sonix_keyboard_instance()->kleds.speed>0)?(sonix_keyboard_instance()->kleds.speed-1):0;
			sonix_keyboard_instance()->set_rgb_param(SET_LEDS_SPEED, sonix_keyboard_instance()->kleds.speed, TRUE);
			notify_data = (sonix_keyboard_instance()->kleds.speed<<8)+RGB_SPEED;
			get_logic_protocol_manager()->send_notify_data(get_protocol_manager_instance()->event_pack(TYPE_RGB_STATUS,notify_data));

			button_flag |= BIT(3);
		}
	}
	else if((button_flag&BIT(3)))
		button_flag &= ~BIT(3);

	// 亮度
	if (_key_is_pressed(MS_Home, data, size))
	{
		_reset_key_value(MS_Home, data, size);
		if((button_flag&BIT(4)) == 0)//解决一个按键多个信号的BUG
		{
			sonix_keyboard_instance()->kleds.brightness = (sonix_keyboard_instance()->kleds.brightness<3)?(sonix_keyboard_instance()->kleds.brightness+1):3;
			sonix_keyboard_instance()->set_rgb_param(SET_LEDS_BRIGHTNESS, sonix_keyboard_instance()->kleds.brightness, TRUE);
			notify_data = ((sonix_keyboard_instance()->kleds.brightness*85)<<8)+RGB_BRIGHTNESS;
			get_logic_protocol_manager()->send_notify_data(get_protocol_manager_instance()->event_pack(TYPE_RGB_STATUS,notify_data));

			button_flag |= BIT(4);
		}
	}
	else if((button_flag&BIT(4)))
		button_flag &= ~BIT(4);

	if (_key_is_pressed(MS_End, data, size))
	{
		_reset_key_value(MS_End, data, size);
		if((button_flag&BIT(5)) == 0)//解决一个按键多个信号的BUG
		{
			sonix_keyboard_instance()->kleds.brightness = (sonix_keyboard_instance()->kleds.brightness>0)?(sonix_keyboard_instance()->kleds.brightness-1):0;
			sonix_keyboard_instance()->set_rgb_param(SET_LEDS_BRIGHTNESS, sonix_keyboard_instance()->kleds.brightness, TRUE);
			notify_data = ((sonix_keyboard_instance()->kleds.brightness*85)<<8)+RGB_BRIGHTNESS;
			get_logic_protocol_manager()->send_notify_data(get_protocol_manager_instance()->event_pack(TYPE_RGB_STATUS,notify_data));

			button_flag |= BIT(5);
		}
	}
	else if((button_flag&BIT(5)))
		button_flag &= ~BIT(5);

	// boot loader
	if (_key_is_pressed(MS_B, data, size) && _key_is_pressed(MS_L, data, size))
	{
		_reset_key_value(MS_B, data, size);
		_reset_key_value(MS_L, data, size);
		set_key_current_status(BOOTLOADER, KEY_STATE_DOWN);
		if(get_key_last_status(BOOTLOADER) == KEY_STATE_UP)
			start_key_timer(BOOTLOADER);
	}

//	if (_key_is_pressed(MS_Enter, data, size))
//	{
//		_reset_key_value(MS_Enter, data, size);
//		ble_status_aaa = DEEP_SLEEPE_STATUS_AAA;
//	}
//	if (_key_is_pressed(MS_Back, data, size))
//	{
//		_reset_key_value(MS_Back, data, size);
//		u8 batVal[1] = {100};
//		bls_att_pushNotifyData(BATT_LEVEL_INPUT_DP_H, batVal, 1);
//	}
//	if (_key_is_pressed(MS_LKuohao, data, size))
//	{
//		_reset_key_value(MS_LKuohao, data, size);
//		sonix_keyboard_instance()->get_sonix_param(GET_LEDS_MODE);

//		static u8 state = 0;
//		state = !state;
//		if(sonix_keyboard_instance()->set_single_led_ack(9,state))
//			printf("state_true");
//	}
//	if (_key_is_pressed(MS_RKuohao, data, size))
//	{
//		_reset_key_value(MS_RKuohao, data, size);
//		static u8 state = 0;
//		state = !state;
//		if(sonix_keyboard_instance()->get_sonix_param(GET_SINGLE_LEDS))
//			printf("state:%x,%x",sonix_keyboard_instance()->sonix_sled_state[0],sonix_keyboard_instance()->sonix_sled_state[1]);
//	}
//	if (_key_is_pressed(MS_Xiegang, data, size))
//	{
//		_reset_key_value(MS_Xiegang, data, size);
//		if(flash_keyboard_param.numberKro == SIXKRO)
//		{
//			flash_keyboard_param.numberKro = NKRO;
//			bls_att_setAttributeTable((u8 *)my_Attributes);
//			bls_att_pushNotifyData(HID_REPORT_MAP_DP_H, (u8*)nkroReportMap, nkroReportMap_size);
//		}
//		else if(flash_keyboard_param.numberKro == NKRO)
//		{
//			flash_keyboard_param.numberKro = SIXKRO;
//			bls_att_setAttributeTable((u8 *)my_Attributes_nkro);
//			bls_att_pushNotifyData(HID_REPORT_MAP_DP_H, (u8*)sixkroReportMap, sixkroReportMap_size);
//		}
//		save_keyboard_info_flash();
//	}
}

static u8 dataB2[2] = {0xb2, 0x00};
static u8 up_data[SCAN_KEYBOARD_SIZE] = {0};
static void _voice_down_action()
{
	set_key_current_status(VOICE, KEY_STATE_DOWN);
	if(get_key_last_status(VOICE) == KEY_STATE_UP)
	{
		start_key_timer(VOICE);
		voice_btn_press(true, VOICE_INDEX);
		if (mode_keycode_flag == OS_TYPE_ANDROID && ifly_android_flag != 0)
			__send_consumer_data(dataB2, 2);
	}
}

//static void __custom_btn_action(u8 index, u8 *data, u8 size)
//{
//	if(_key_is_pressed(index, data, size))
//	{
//		_reset_key_value(index, data, size);
//		if(custom_button_param[index - 1].flag == 1)
//		{
//			if(custom_button_param[index - 1].type == 0)
//			{
//				custom_key_value[1] = custom_button_param[index - 1].value[0];
//				for(int i = 2; i< 8; i++)
//				{
//					u8 key_value = custom_button_param[index - 1].value[i];
//					if(key_value)
//						custom_key_value[(key_value >> 3) + 2] |= (1 << (key_value & 0x07));
//					else break;
//				}
//			}
//			else if(custom_button_param[index - 1].type == 1)
//			{
//				_send_consumer_data(custom_button_param[index - 1].value, 2);
//			}
//		}
//		else //if (custom_button_param[index - 1].flag == 2)
//		{
//			s8 key_value = index - 87;
//			key_value = key_value > 0 ? key_value : 0;
//			key_value = key_value > 2 ? 2 : key_value;
//			all_key_flag.current_key_flag.custom_keys[key_value] = 1;
//			if(all_key_flag.last_key_flag.custom_keys[key_value] == 0)
//				voice_btn_press(true, index - 1);
//		}
//	}
//}
static void _voice_up_action()
{
	voice_btn_press(false, VOICE_INDEX);
	if (mode_keycode_flag == OS_TYPE_ANDROID && ifly_android_flag != 0)
		__send_consumer_data(up_data, 2);
}

extern void set_adv_type();

void  reset_all_index_pairs()
{
	flash_erase_sector(BONDING_FLASH_ADDR);
	clear_last_pair_device();
	clear_latest_mac_addr();
	remove_all();
	sleep_ms(1000);
	start_reboot();
}
/**
 * @brief  清空当前索引的设备连接数据
 * @note   
 * @param  ble_index: 
 * @retval None
 */
static void send_ble_reset(int ble_index)
{
	// printf("clear:%d \n", ble_index);
	if (connect_ok)
	{
		bls_ll_terminateConnection(HCI_ERR_REMOTE_USER_TERM_CONN);
	}
	led_close_channel();
	if(blc_smp_param_getCurrentBondingDeviceNumber() >= (MAX_BONDING_DEVICE_NUM- 1)){
		reset_all_index_pairs();
	}
	set_device_adv_by_reset(ble_index, true);
	set_adv_type();
}

/**
 * @brief  当前的索引的设备发送广播数据
 * @note   
 * @param  ble_index: 
 * @retval None
 */
static void send_ble_adv(int ble_index)
{
	// printf("adv:%d \n", ble_index);
	pair_device_info_t last_device = get_current_adv_device();
	if (last_device.device_index == ble_index)
	{
		if (connect_ok)
		{
			// printf("connected\n");
			led_choose_channel();
			return;
		}
	}
	else// 切换通道
	{
		led_close_channel();
		if (connect_ok)
		{
			bls_ll_terminateConnection(HCI_ERR_REMOTE_USER_TERM_CONN);
			set_device_adv_by_reset(ble_index, false);
			return;
		}
	}
	set_device_adv_by_reset(ble_index, false);
	set_adv_type();
}

////https://mouse.iflytek.com/
static void send_feishu_web_data()
{
	u8 buf[7] = {0x00, 0xff, 0, 0, 0, 0, 0};
	if(push_data_out_fifo(buf, OUT_SPP_REPORT, 7))
	{
		disable_key_timer(VOICE);
	}
}

/**
 * @brief  key_long_press_action
 * @note   
 * @retval None
 */
static void key_long_press_action()
{
	if(get_key_state_duration(CHANNEL1, KEY_STATE_DOWN) >= 3)
	{
		disable_key_timer(CHANNEL1);
		send_ble_reset(CHANNEL_1);
	}
	if(get_key_state_duration(CHANNEL2, KEY_STATE_DOWN) >= 3)
	{
		disable_key_timer(CHANNEL2);
		send_ble_reset(CHANNEL_2);
	}
	if(get_key_state_duration(CHANNEL3, KEY_STATE_DOWN) >= 3)
	{
		disable_key_timer(CHANNEL3);
		send_ble_reset(CHANNEL_3);
	}
	if(get_key_state_duration(RESET, KEY_STATE_DOWN) > 5)
	{
		disable_key_timer(RESET);
		flash_erase_sector(0x7e000);	// CUSTOM_BUTTON_VALUE_ADDR
		flash_erase_sector(KEYBOARD_PARAM_ADDR);		// 0x7a000
		flash_erase_sector(SONIX_RGB_INFO_ADDR);		// 0x73000
		reset_all_index_pairs();
	}
}
static void __dealwith_special_up_action()
{
	for(int i = 0; i < MAX_KEY; i++)
	{
		if(all_key_flag[i].last_status != all_key_flag[i].current_status)
		{
			if(all_key_flag[i].current_status == KEY_STATE_UP)
			{
				if(i == CONSUMER)
				{
					memset(current_consumer_key, 0, 2);
					__send_consumer_data(up_data, 2);
				}
				else if(i == CHANNEL1)
				{
					if(all_key_flag[i].timer_en)
						send_ble_adv(CHANNEL_1);
				}
				else if(i == CHANNEL2)
				{
					if(all_key_flag[i].timer_en)
						send_ble_adv(CHANNEL_2);
				}
				else if(i == CHANNEL3)
				{
					if(all_key_flag[i].timer_en)
						send_ble_adv(CHANNEL_3);
				}
				else if(i == VOICE)
				{
					_voice_up_action();
				}
				else if(i == OCR)
				{
					voice_btn_press(false, MS_Pause-1);
				}
				else if(i == TRANS_LATE)
				{
					voice_btn_press(false, MS_Print-1);
				}
				else if(i == DIATATION)
				{
					voice_btn_press(false, MS_Scroll-1);
				}
				all_key_flag[i].timer_en = 0;
			}

			all_key_flag[i].last_status = all_key_flag[i].current_status;
		}
		all_key_flag[i].current_status = KEY_STATE_UP;
	}
}

//static void reset_key_down()
//{
//	reset_all_ble_pairs.status = true;
//	reset_all_ble_pairs.time_tick = clock_time();
//	all_key_flag.current_key_flag.reset_key = 1;
//}
//
//static void  reset_key_up()
//{
//	reset_all_ble_pairs.status=false;
//	reset_all_ble_pairs.second_pc=0;
//}
//
//static void reset_loop_action()
//{
//	if (reset_all_ble_pairs.status)
//	{
//		if (clock_time_exceed(reset_all_ble_pairs.time_tick, TIME_1_SECONDS))   //rÿ3s +1
//		{
//			reset_all_ble_pairs.time_tick = clock_time();
//			reset_all_ble_pairs.second_pc++;
//		}
//		if (reset_all_ble_pairs.second_pc > 5)
//		{
////			sonix_keyboard_instance()->reset();
//			reset_all_index_pairs();  //清空ble配对程序
//			reset_all_ble_pairs.second_pc = 0;
//			reset_all_ble_pairs.status = 0;
//		}
//	}
//}

static void convert_keycode(u8 *data, u8 size)
{
	if(data == NULL || size != SCAN_KEYBOARD_SIZE)
		return;
	for(int i = 0; i < SCAN_KEYBOARD_SIZE; i++)
		*(data + i) |= custom_key_value [i];

	if(fun_mode == USB_MODE || keyboard_data_mode==NKRO)
	{
		_send_keyboard_data(data + 1, NKRO_KRYBOARD_SIZE);
	}
	else if(fun_mode == RF_1M_BLE_MODE && keyboard_data_mode==SIXKRO)
	{
		u8 data_key[SIXKRO_KRYBOARD_SIZE] = {0};
		u8 cout = 2;
		data_key[0] = *(data + 1);
		for(int i = 2; i < size; i++)
		{
			if(data[i] & 0xff)
			{
				for(int j = 0; j < 8; j++)
				{
					if((data[i] >> j) & 0x01)
					{
						data_key[cout] = (i - 2) * 8 + j;
						cout ++;
						if(cout >= 8)
							return _send_keyboard_data(data_key, SIXKRO_KRYBOARD_SIZE);
					}
				}
			}
		}
		return _send_keyboard_data(data_key, SIXKRO_KRYBOARD_SIZE);
	}

}

static u8 mextern_btn_flag = 0;
static void matrix_extern_btn()
{
#if WHEEL_BUTTON_ENABLE
    if (!gpio_read(PIN_BTN_WHEEL))
    {
    	if(!(mextern_btn_flag&BIT(0))){
    		mextern_btn_flag|=BIT(0);
    		if(flash_keyboard_param.wheel_func == AUDIO_MANAGER)
    		{
				u8 data_comsumer[2] = {0xe2, 0x00};
				_send_consumer_data(data_comsumer, 2);
    		}
    		else if(flash_keyboard_param.wheel_func == BLIGHT_MANAGER)
			{
        	    set_keyboard_up(0);
				if(sonix_keyboard_instance()->kleds.brightness==0)
				{
					set_brightness(DEFAULT_BRIGHTNESS);
				}
				else
				{
					set_brightness(0);
				}
			}
    		reset_idle_status();
    	}
    }else if(mextern_btn_flag&BIT(0))
    {
    	mextern_btn_flag&=~BIT(0);
		memset(current_consumer_key, 0, 2);
		if(flash_keyboard_param.wheel_func == AUDIO_MANAGER)
			__send_consumer_data(up_data, 2);
		else if(flash_keyboard_param.wheel_func == BLIGHT_MANAGER)
		    set_keyboard_up(1);
		reset_idle_status();
    }
#endif

#if S1_FUN_ENABLE_AAA
    if (gpio_read(PIN_BTN_S1_ROW))
    {
    	if(!(mextern_btn_flag&BIT(1))){
    		mextern_btn_flag|=BIT(1);
			voice_btn_press(true, MS_MIC - 1);
    		reset_idle_status();
    	}
    }else if(mextern_btn_flag&BIT(1))
    {
    	mextern_btn_flag&=~BIT(1);
		voice_btn_press(false, MS_MIC - 1);
		reset_idle_status();
    }
#endif
    // 每隔25ms后设置灯光亮度，共设置10次
    if(get_brightness_tick && clock_time_exceed(get_brightness_tick, 25*1000))
    {
    	get_brightness_tick = 0;
		sonix_keyboard_instance()->set_rgb_param(SET_LEDS_BRIGHTNESS, sonix_keyboard_instance()->kleds.brightness, FALSE);
		if(++get_brightness_time<10)
	    	get_brightness_tick = clock_time()|1;
//    		u32 notify_data = ((sonix_keyboard_instance()->kleds.brightness*85)<<8)+RGB_BRIGHTNESS;
//			get_logic_protocol_manager()->send_notify_data(get_protocol_manager_instance()->event_pack(TYPE_RGB_STATUS,notify_data));
//    	}
    }
}

//static u8 fn_lock_flag = 0;
static bool _dealwith_keyboard_action(u8 *data, u8 size)
{
	if (_key_is_pressed(MS_Voice, data, size))
	{
		_voice_down_action();
		_reset_key_value(MS_Voice, data, size);
	}
	if(_key_is_pressed(MS_Pause, data, size))
	{
		set_key_current_status(OCR, KEY_STATE_DOWN);
		if(get_key_last_status(OCR) == KEY_STATE_UP)
		{
			voice_btn_press(true, MS_Pause-1);
		}
		_reset_key_value(MS_Pause, data, size);
	}
	if(_key_is_pressed(MS_Print, data, size))
	{
		set_key_current_status(TRANS_LATE, KEY_STATE_DOWN);
		if(get_key_last_status(TRANS_LATE) == KEY_STATE_UP)
		{
			voice_btn_press(true, MS_Print-1);
		}
		_reset_key_value(MS_Print, data, size);
	}
	if(_key_is_pressed(MS_Scroll, data, size))
	{
		set_key_current_status(DIATATION, KEY_STATE_DOWN);
		if(get_key_last_status(DIATATION) == KEY_STATE_UP)
		{
			voice_btn_press(true, MS_Scroll-1);
		}
		_reset_key_value(MS_Scroll, data, size);
	}
	if (_key_is_pressed(MS_Fn, data, size))
	{
		if(fun_mode == RF_1M_BLE_MODE && _is_reset_pairs_proc(data,size))
		{
			set_key_current_status(RESET, KEY_STATE_DOWN);
			if(get_key_last_status(RESET) == KEY_STATE_UP)
				start_key_timer(RESET);
		}
		function_key_proc(data, size);
		_special_key_proc(data, size);
		led_key_proc(data, size);
	}
	convert_keycode(data, size);
	return 0;
}

static void dealwith_keyboard_action(u8 *data, u8 size){
	memset(custom_key_value, 0, SCAN_KEYBOARD_SIZE);
	_dealwith_keyboard_action(data, size);
	__dealwith_special_up_action();
}
static u8 key_scan_aaa()
{
	if (scan_data_fifo.len(&scan_data_fifo) > 0)
	{
		u8 scan_buffer[SCAN_KEYBOARD_SIZE] = {0};
		int read_size = 0;
		if ((read_size = scan_data_fifo.get(&scan_data_fifo, scan_buffer, SCAN_KEYBOARD_SIZE)) == SCAN_KEYBOARD_SIZE)
		{
			dealwith_keyboard_action(scan_buffer, SCAN_KEYBOARD_SIZE);
			return 1;
		}
	}
	matrix_extern_btn();
	key_status_loop();
	if (mode_keycode_flag == OS_TYPE_WINDOWS && (!(web_flag & (1 << ble_adv_channel) ) ) ) //WINS
	{
		if(get_key_state_duration(VOICE, KEY_STATE_DOWN) > 5)
		{
			send_feishu_web_data();
		}
	}
	if(get_key_state_duration(BOOTLOADER, KEY_STATE_DOWN) > 5)
	{
		disable_key_timer(BOOTLOADER);
		sonix_keyboard_instance()->set_mcu_mode(MCU_BOOTLOADER);
	}
	if(fun_mode == RF_1M_BLE_MODE)
	{
		key_long_press_action();
	}
	return 0;
}

u8 keyboard_manager_proc()
{
	u8 has_report_f = 0;
	has_report_f = key_scan_aaa();
	return has_report_f;
}
