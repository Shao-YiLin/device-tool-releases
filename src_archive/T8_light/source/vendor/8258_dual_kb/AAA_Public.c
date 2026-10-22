/********************************************************************************************************
 * @file     aaa_public.c
 *
 * @brief    for TLSR chips
 *
 * @author	 telink
 * @date     Sep. 30, 2010
 *
 * @par      Copyright (c) 2010, Telink Semiconductor (Shanghai) Co., Ltd.
 *           All rights reserved.
 *           
 *			 The information contained herein is confidential and proprietary property of Telink 
 * 		     Semiconductor (Shanghai) Co., Ltd. and is available under the terms 
 *			 of Commercial License Agreement between Telink Semiconductor (Shanghai) 
 *			 Co., Ltd. and the licensee in separate contract or the terms described here-in. 
 *           This heading MUST NOT be removed from this file.
 *
 * 			 Licensees are granted free, non-transferable use of the information in this 
 *			 file under Mutual Non-Disclosure Agreement. NO WARRENTY of ANY KIND is provided. 
 *           
 *******************************************************************************************************/

#include "AAA_public_config.h"
#include "../../third/protocol/protocol_manager.h"
#include "../../third/protocol/protocol_define.h"
#include "../../third/hash/mblue_hash.h"
#include "logic_protocol_manager.h"
#include "USB_manager.h"
#include "custom_button_logic_manager.h"

_attribute_data_retention_user u8 device_name_len = 0;
_attribute_data_retention_user u8 connect_ok = 0;
u8 dongle_web_flag = 0;

_attribute_data_retention_user u8 keyboard_mac_address[6] = {0};
//_attribute_data_retention_user u8 mouse_mac_address[] = {0x12, 0x34, 0x56, 0x78, 0x11, 0x54};
// 版本号
#define VERSION_BUILD (14)
_attribute_data_retention_user u8 keyboard_version[4] = {0x1, 0x0, 0x0, VERSION_BUILD};
_attribute_data_retention_user u32 record_channel = 1;
u8 has_web_key_flag = 0;
//_attribute_data_retention_user u8 mode_keycode_flag = 0; //1-windows;2-android;3-mac

u32 protocol_send_flag = 0;


void save_dev_info_flash()
{
	dev_info_idx += sizeof(FLASH_DEV_INFO_AAA);
	flash_write_page(CFG_DEVICE_MODE_ADDR + dev_info_idx, sizeof(FLASH_DEV_INFO_AAA), (u8 *)&flash_dev_info.dongle_id);
//	printf("save dev info\n");
	//sleep_us(50000);
}

void save_keyboard_info_flash()
{
	flash_keyboard_param.idx += sizeof(FLASH_KEYBOARD_INFO);
	//gpio_write(PIN_FN_LOCK_LED, flash_keyboard_param.fn_lock);
	flash_write_page(KEYBOARD_PARAM_ADDR + flash_keyboard_param.idx, sizeof(FLASH_KEYBOARD_INFO), (u8 *)&flash_keyboard_param);
}

_attribute_data_retention_user custom_cfg_t user_cfg;
_attribute_data_retention_user u8 fun_mode = RF_1M_BLE_MODE;
_attribute_data_retention_user u8 keyboard_data_mode = NKRO;
u8 mode_keycode_flag = OS_TYPE_WINDOWS; ////1-windows;2-android;3-mac
_attribute_data_retention_user FLASH_DEV_INFO_AAA flash_dev_info;
_attribute_data_retention_user FLASH_KEYBOARD_INFO flash_keyboard_param;

_attribute_data_retention_user u8 suspend_wake_up_enable = 1;
_attribute_data_retention_user u8 deep_flag = POWER_ON_ANA_AAA;
_attribute_data_retention_user u8 pair_flag = 0;

//_attribute_data_retention_user TX_FIFO_AAA_STRUCT tx_fifo_aaa;

_attribute_data_retention_user u32 power_tick = 0;

void S1_set_wakeup_level(u8 enable)
{
#if S1_FUN_ENABLE_AAA
    if (gpio_read(S1_FUN_ENABLE_AAA))
    {
        cpu_set_gpio_wakeup(S1_FUN_ENABLE_AAA, 0, enable); //low wakeup suspend
    }
    else
    {
        cpu_set_gpio_wakeup(S1_FUN_ENABLE_AAA, 1, enable);
    }

#endif
}

void wheel_set_wakeup_level(u8 enable)
{
#if WHEEL_FUN_ENABLE_AAA
    if (gpio_read(PIN_WHEEL_1))
    {
        cpu_set_gpio_wakeup(PIN_WHEEL_1, 0, enable); //low wakeup suspend
    }
    else
    {
        cpu_set_gpio_wakeup(PIN_WHEEL_1, 1, enable);
    }

    if (gpio_read(PIN_WHEEL_2))
    {
        cpu_set_gpio_wakeup(PIN_WHEEL_2, 0, enable); //low wakeup suspend
    }
    else
    {
        cpu_set_gpio_wakeup(PIN_WHEEL_2, 1, enable);
    }
    if (gpio_read(PIN_BTN_WHEEL))
    {
        cpu_set_gpio_wakeup(PIN_BTN_WHEEL, 0, enable); //low wakeup suspend
    }
    else
    {
        cpu_set_gpio_wakeup(PIN_BTN_WHEEL, 1, enable);
    }

#endif
}

u8 ble_notify_data_proc(u16 type, u8 *data, u32 size)
{
	u16 data_type = 0;
	if (ble_status_aaa != T5S_CONNECTED_STATUS_AAA)
		return 1;
	switch(type)
	{
		case OUT_KEYBOARD_REPORT:
			data_type = HID_NORMAL_KB_REPORT_INPUT_DP_H;
			break;
		case OUT_CONSUMER_REPORT:
			data_type = HID_CONSUME_REPORT_INPUT_DP_H;
			break;
		case OUT_PROT_REPORT:
			data_type = PROTOCOL_TX_DP_H;
			break;
		case OUT_SPP_REPORT:
		{
#if BLE_WEB_ENABLE
			has_web_key_flag = 1;
			return 1;
#else
			data_type = SPP_INPUT_DP_H;
#endif

			break;
		}
		default:
		{
			protocol_send_flag = 0;
			return 1;
		}
	}
	u8 txFifoNumber = blc_ll_getTxFifoNumber();
	if (txFifoNumber < RF_TX_FIFO_ALLOW_NUM)
	{
		u8 status = bls_att_pushNotifyData(data_type, data, size); //HID_CONSUME_REPORT_INPUT_DP_H
		if (status == BLE_SUCCESS)
		{
			return 1;
		}
	}
	return 0;
}

#if BLE_WEB_ENABLE
#define WEB_HID_DELAY 0
static int web_count = 0;
y_kbdata_t web_data_sixkro[25+PRODUCT_NO_LENGTH+PRODUCT_NO_LENGTH];
static int web_count_nkro = 0;
y_kbdata_t web_data_nkro[25+PRODUCT_NO_LENGTH+PRODUCT_NO_LENGTH];
//http://product_NO.madaogo.com
void web_data_nkro_init()
{
	int i;
	char coiled = 0;// 标注重复字符
	unsigned short coiled_count = 0;
	y_kbdata_t web_head[] = {
		{MS_LWin, MS_R, 50},
		{0, MS_H, WEB_HID_DELAY},
		{0, MS_T, WEB_HID_DELAY},
		{0, MS_NULL, WEB_HID_DELAY},
		{0, MS_T, WEB_HID_DELAY},
		{0, MS_P, WEB_HID_DELAY},
	//	{0, MS_S, 0},
		{MS_LShift, MS_Fenhao, WEB_HID_DELAY}, //:;
		{0, MS_Wenhao, WEB_HID_DELAY},				  // /
		{0, MS_NULL, WEB_HID_DELAY},
		{0, MS_Wenhao, WEB_HID_DELAY},
	};
	y_kbdata_t web_tail[] = {
		{0, MS_Juhao, WEB_HID_DELAY}, //.>
		{0, MS_M, WEB_HID_DELAY},
		{0, MS_A, WEB_HID_DELAY},
		{0, MS_D, WEB_HID_DELAY},
		{0, MS_A, WEB_HID_DELAY},
		{0, MS_O, WEB_HID_DELAY},
		{0, MS_G, WEB_HID_DELAY},
		{0, MS_O, WEB_HID_DELAY},
		{0, MS_Juhao, WEB_HID_DELAY},
		{0, MS_C, WEB_HID_DELAY},
		{0, MS_O, WEB_HID_DELAY},
		{0, MS_M, WEB_HID_DELAY},
		{MS_LShift, WEB_HID_DELAY, 20},
		{0, MS_Enter, WEB_HID_DELAY},
		{0, MS_NULL, WEB_HID_DELAY},
	};
	u8 product_NO[PRODUCT_NO_LENGTH] = DEVICE_PRODUCT_NO;
	memcpy(web_data_nkro,web_head,10*sizeof(y_kbdata_t));
	for(i=0;i<PRODUCT_NO_LENGTH;i++)
	{
		if(product_NO[i]==coiled)
		{
			web_data_nkro[10+i] = (y_kbdata_t){0, VK_NONE, WEB_HID_DELAY};
			coiled_count++;
		}
		if(product_NO[i]>='A'&&product_NO[i]<='Z')
			web_data_nkro[10+i+coiled_count] = (y_kbdata_t){0,product_NO[i]-'A'+MS_A, WEB_HID_DELAY};
		else if(product_NO[i]>='1'&&product_NO[i]<='9')
			web_data_nkro[10+i+coiled_count] = (y_kbdata_t){0,product_NO[i]-'1'+MS_1, WEB_HID_DELAY};
		else if(product_NO[i]=='0')
			web_data_nkro[10+i+coiled_count] = (y_kbdata_t){0,MS_0, WEB_HID_DELAY};
		coiled = product_NO[i];
	}
	memcpy(web_data_nkro+(10+PRODUCT_NO_LENGTH+coiled_count),web_tail,15*sizeof(y_kbdata_t));
	web_count_nkro = 25+PRODUCT_NO_LENGTH+coiled_count;
}

//http://product_NO.madaogo.com
void web_data_init()
{
	int i;
	char coiled = 0;
	unsigned short coiled_count = 0;
	y_kbdata_t web_head[] = {
		{VK_MSK_LWIN, VK_R, 50},
		{0,VK_H, WEB_HID_DELAY},
		{0,VK_T, WEB_HID_DELAY},
		{0,VK_NONE, WEB_HID_DELAY},
		{0,VK_T, WEB_HID_DELAY},
		{0,VK_P, WEB_HID_DELAY},
		{VK_MSK_LSHIFT,VK_SEMICOLON, WEB_HID_DELAY},
		{0,VK_SLASH, WEB_HID_DELAY},
		{0,VK_NONE, WEB_HID_DELAY},
		{0,VK_SLASH, WEB_HID_DELAY},
	};
	y_kbdata_t web_tail[] = {
		{0,VK_PERIOD, WEB_HID_DELAY},
		{0,VK_M, WEB_HID_DELAY},
		{0,VK_A, WEB_HID_DELAY},
		{0,VK_D, WEB_HID_DELAY},
		{0,VK_A, WEB_HID_DELAY},
		{0,VK_O, WEB_HID_DELAY},
		{0,VK_G, WEB_HID_DELAY},
		{0,VK_O, WEB_HID_DELAY},
		{0,VK_PERIOD, WEB_HID_DELAY},
		{0,VK_C, WEB_HID_DELAY},
		{0,VK_O, WEB_HID_DELAY},
		{0,VK_M, WEB_HID_DELAY},
		{VK_MSK_LSHIFT, 0, WEB_HID_DELAY},
		{0, VK_ENTER, WEB_HID_DELAY},
		{0, VK_NONE, WEB_HID_DELAY},
	};
	u8 product_NO[PRODUCT_NO_LENGTH] = DEVICE_PRODUCT_NO;
	memcpy(web_data_sixkro,web_head,10*sizeof(y_kbdata_t));
	for(i=0;i<PRODUCT_NO_LENGTH;i++)
	{
		if(product_NO[i]==coiled)
		{
			web_data_sixkro[10+i] = (y_kbdata_t){0, VK_NONE, WEB_HID_DELAY};
			coiled_count++;
		}
		if(product_NO[i]>='A'&&product_NO[i]<='Z')
			web_data_sixkro[10+i+coiled_count] = (y_kbdata_t){0,product_NO[i]-'A'+VK_A, WEB_HID_DELAY};
		else if(product_NO[i]>='1'&&product_NO[i]<='9')
			web_data_sixkro[10+i+coiled_count] = (y_kbdata_t){0,product_NO[i]-'1'+VK_1, WEB_HID_DELAY};
		else if(product_NO[i]=='0')
			web_data_sixkro[10+i+coiled_count] = (y_kbdata_t){0,VK_0, WEB_HID_DELAY};
		coiled = product_NO[i];
	}
	memcpy(web_data_sixkro+(10+PRODUCT_NO_LENGTH+coiled_count),web_tail,15*sizeof(y_kbdata_t));
	web_count = 25+PRODUCT_NO_LENGTH+coiled_count;

}

void open_website()
{
	static u8 addr_cnt = 0;
	static u8 flag = 0;
	static u32 tick = 0;
	u8 buf[8] = {0, 0, 0, 0, 0, 0, 0 ,0};

	if (flag == 0)
	{
		buf[0] = web_data_sixkro[addr_cnt].ctrl_key;
		buf[2] = web_data_sixkro[addr_cnt].keycode;
		if (push_data_out_fifo(buf, OUT_KEYBOARD_REPORT, 8))
		{
			flag = 1;
		}
	}
	else if (flag == 1)
	{
		if (web_data_sixkro[addr_cnt].next_interval)
		{
			tick = clock_time();
			flag = 2;
		}
		else
		{
			flag = 3;
		}
	}
	else if ((flag == 2) && clock_time_exceed(tick, web_data_sixkro[addr_cnt].next_interval * 10000))
	{
		flag = 3;
	}
	else if (flag == 3)
	{
		flag = 0;
		addr_cnt++;
		if (addr_cnt >= web_count)		{
			addr_cnt = 0;
			has_web_key_flag = 0;
		}
	}
}

void open_website_nkro()
{
	static u8 addr_cnt = 0;
	static u8 flag = 0;
	static u32 tick = 0;
	u8 buf[15] = {0, 0, 0, 0, 0, 0, 0 ,0, 0, 0, 0, 0, 0, 0, 0};

	if (flag == 0)
	{

		ARRAY_MS_TO_VK(buf,web_data_nkro[addr_cnt].ctrl_key);
		ARRAY_MS_TO_VK(buf,web_data_nkro[addr_cnt].keycode);
		if (push_data_out_fifo(buf+1,OUT_KEYBOARD_REPORT,14))// 第一个字节是 自定义的功能按键
		{
			flag = 1;
		}

	}
	else if (flag == 1)
	{
		if (web_data_nkro[addr_cnt].next_interval)
		{
			tick = clock_time();
			flag = 2;
		}
		else
		{
			flag = 3;
		}
	}
	else if ((flag == 2) && clock_time_exceed(tick, web_data_nkro[addr_cnt].next_interval * 10000))
	{
		flag = 3;
	}
	else if (flag == 3)
	{
		flag = 0;
		addr_cnt++;
		if (addr_cnt >= web_count_nkro)		{
			addr_cnt = 0;
			has_web_key_flag = 0;
		}
	}
}
#endif

static void wheel_init_hw()
{
    write_reg8(0xd2, WHEEL_ADDRES_D2); // different gpio different value
    write_reg8(0xd3, WHEEL_ADDRES_D3);
    write_reg8(0xd7, 0x01);    //BIT(0) 0: 1赂帽        1:  2赂帽      	BIT(1)  wakeup enable
    write_reg8(0xd1, 0x01);    //filter   00-07     00 is best
    reg_rst0 |= FLD_RST0_QDEC; // for8258  power on
    reg_rst0 &= (~FLD_RST0_QDEC);
    rc_32k_cal();
    BM_SET(reg_clk_en0, FLD_CLK0_QDEC_EN);
}

u32 mouse_wheel_prepare_tick(void)
{
    write_reg8(0xd8, 0x01);
    return clock_time();
}

_attribute_ram_code_ s8 mouse_wheel_process()
{
    s8 ret = 0;
    u32 wheel_prepare_tick = mouse_wheel_prepare_tick();
    while (read_reg8(0xd8) & 0x01)
    {
        if (clock_time_exceed(wheel_prepare_tick, 260)) //4 cylce is enough: 4*1/32k = 1/8 ms
        {
            write_reg8(0xd6, 0x01); //reset  d6[0]
            write_reg8(0xd6, 0x00);
            break;
        }
    }
    _attribute_data_retention_user static signed char accumulate_wheel_cnt;
    _attribute_data_retention_user static signed char wheel_cnt;
    wheel_cnt = read_reg8(0xd0);

    wheel_cnt += accumulate_wheel_cnt;

    if (wheel_cnt & 1) //脝忙
    {
        accumulate_wheel_cnt = wheel_cnt > 0 ? 1 : -1;
    }
    else //脜录
    {
        accumulate_wheel_cnt = 0;
    }
    ret = (wheel_cnt / 2);
    return ret;
}

void hw_init()
{
#if BLT_APP_LED_ENABLE
	led_hw_init();
	caps_flag = analog_read(DEEP_ANA_REG1);
	host_keyboard_status = caps_flag;
	if(!gpio_read(PIN_ON_OFF_STATE))
		led_caps_set(caps_flag & 2);
#endif
#if WHEEL_FUN_ENABLE_AAA
    wheel_init_hw();
#endif
    init_button_value_flash();
}



static int wheel_num = 0;
static u8 audio_change_flag = 0;
static u8 wheel_flag = 0;
void whlee_monitor_pro()
{
#if WHEEL_FUN_ENABLE_AAA
    u8 key_value[2] = {0x00, 0x00};
	s8 wheel_now = mouse_wheel_process();
	if(wheel_now && wheel_flag == 0)
	{
		set_keyboard_up(0);
		wheel_flag = 1;
	}
	else if(wheel_flag)
	{
		wheel_flag = 0;
		set_keyboard_up(1);
	}

	if(wheel_now < 0){
		if(flash_keyboard_param.wheel_func == AUDIO_MANAGER)
		{
			audio_change_flag = 1;
			key_value[0] = 0xe9;
			push_data_out_fifo(key_value, OUT_CONSUMER_REPORT, 2);
		}
		else if(flash_keyboard_param.wheel_func == BLIGHT_MANAGER)
		{
			wheel_num = wheel_num>0?0:wheel_num;
			wheel_num += wheel_now;
		}
		reset_idle_status();
//		push_data_out_fifo(key_value, OUT_CONSUMER_REPORT, 2);
		//if(custom_button_param[MS_WHL -1].flag == 1 && custom_button_param[MS_WHL -1].type == 1)
		//{
		//	if(custom_button_param[MS_WHL -1].value[0] == 0xcd)
		//		key_value[0] = 0xb5;
		//	else key_value[0] = 0xe9;
		//}

	}
	else if(wheel_now > 0){
		if(flash_keyboard_param.wheel_func == AUDIO_MANAGER)
		{
			audio_change_flag = 1;
			key_value[0] = 0xea;
			push_data_out_fifo(key_value, OUT_CONSUMER_REPORT, 2);
		}
		else if(flash_keyboard_param.wheel_func == BLIGHT_MANAGER)
		{
			wheel_num = wheel_num<0?0:wheel_num;
			wheel_num += wheel_now;
		}
		reset_idle_status();
//		push_data_out_fifo(key_value, OUT_CONSUMER_REPORT, 2);
		//if(custom_button_param[MS_WHL -1].flag == 1 && custom_button_param[MS_WHL -1].type == 1)
		//{
		//	if(custom_button_param[MS_WHL -1].value[0] == 0xcd)
		//		key_value[0] = 0xb6;
		//	else key_value[0] = 0xea;
		//}

	}
	else if(audio_change_flag && flash_keyboard_param.wheel_func == AUDIO_MANAGER){
		audio_change_flag = 0;
		push_data_out_fifo(key_value, OUT_CONSUMER_REPORT, 2);
	}
	if(flash_keyboard_param.wheel_func == BLIGHT_MANAGER)
	{
		if(wheel_num <= -5)
		{
			wheel_num += 5;
			sonix_keyboard_instance()->kleds.mode = sonix_keyboard_instance()->kleds.mode==RGB_MODE_NUM?0:sonix_keyboard_instance()->kleds.mode+1;
			mode_set(sonix_keyboard_instance()->kleds.mode);
		}else if(wheel_num >= 5){
			wheel_num -= 5;
			sonix_keyboard_instance()->kleds.mode = sonix_keyboard_instance()->kleds.mode==0?RGB_MODE_NUM:sonix_keyboard_instance()->kleds.mode-1;
			mode_set(sonix_keyboard_instance()->kleds.mode);
		}
	}
#endif
}


#if BLE_AUDIO_ENABLE1
u8 web_flag = 0;

void voice_btn_press(bool bpressed, u32 index)
{
	if (bpressed)
	{
		get_logic_protocol_manager()->send_notify_data(get_protocol_manager_instance()->event_pack(TYPE_KEY,
																								(index << 16) | KEY_DOWN));
	}
	else
	{
		get_logic_protocol_manager()->send_notify_data(get_protocol_manager_instance()->event_pack(TYPE_KEY,
																								(index << 16) | KEY_UP));
	}
}

#endif


void clear_pair_flag()
{
	pair_flag = 0;
}
void set_pair_flag()
{
	pair_flag = 1;
}
void write_deep_ana0(u8 buf)
{
	deep_flag = buf;
	ANALOG_WRITE(DEEP_ANA_REG0, buf, 0);
}

void Set_web_flag()
{
	if(!(web_flag & (1 << ble_adv_channel)))
	{
		web_flag |= (1 << ble_adv_channel);
		analog_write(DEEP_ANA_REG9, web_flag);
	}
}

void reset_web_flag()
{
	if(web_flag & (1 << ble_adv_channel))
	{
		web_flag &= ~(u8)(1 << ble_adv_channel);
		analog_write(DEEP_ANA_REG9, web_flag);
	}
}

void set_os_type(u8 os_type){
	mode_keycode_flag = os_type;
	set_current_device_os_type(os_type);
}

int protocolWrite(void *src)
{
	if (!src)
		return 0;

	rf_packet_att_write_t *p = (rf_packet_att_write_t *)src;
	u8 *buf = &p->value;
	u32 len = p->l2capLen - 3;

	if (!buf || len <= 0)
		return 0;

	protocol_send_flag = 1;
	get_logic_protocol_manager()->parse(buf, len);
//	Set_web_flag();

	return 1;
}

int keyboardWrite(void *src)
{
	if (!src)
		return 0;
	rf_packet_att_write_t *p = (rf_packet_att_write_t *)src;
	u8 *buf = &p->value;
	u32 len = p->l2capLen - 3;

	if (!buf || len <= 0)
		return 0;
	if (len == 1)
	{ // caps lock action
		u8 cmd = *buf;
//		printf("cmd :%d\r\n",cmd);
		analog_write(DEEP_ANA_REG1, cmd);
		led_caps_set(cmd & 2);
	}

	return 1;
}

void ble_send_key_up()
{
	u8 data[14] = {0};
	if(keyboard_data_mode==NKRO)
		push_data_out_fifo(data, OUT_KEYBOARD_REPORT, 14);
	else if(keyboard_data_mode==SIXKRO)
		push_data_out_fifo(data, OUT_KEYBOARD_REPORT, 8);
}
