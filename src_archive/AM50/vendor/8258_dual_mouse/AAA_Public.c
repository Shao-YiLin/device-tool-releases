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
#include "../../third/hash/mblue_hash.h"
#include "logic_protocol_manager.h"
#include "application/usbstd/usbkeycode.h"

#include "AAA_public_config.h"
#include "../../third/protocol/protocol_manager.h"
#include "../../third/protocol/protocol_define.h"
#include "mouse_effect_logic_manager.h"
#include "USB_manager.h"
#include "sensor/sensor_proxy.h"
#include "third/fifo/mblue_fifo.h"
#if PWM_LED_ENABLE
#include "RGB/RGB_config_file.h"
#endif

#define COMB_BTN_PAIR (MS_BTN_LEFT | MS_BTN_MIDDLE | MS_BTN_RIGHT)

_attribute_data_retention_user u8 web_flag = 0;
//_attribute_data_retention_user u8 device_name_len = 0;
_attribute_data_retention_user u8 connect_ok = 0;
_attribute_data_retention_user u8 ui_mtu_size_exchange_req = 0;
_attribute_data_retention_user short voice_power;
_attribute_data_retention_user custom_cfg_t user_cfg;
_attribute_data_retention_user FLASH_DEV_INFO_AAA flash_dev_info;
_attribute_data_retention_user FLASH_DPI_INFO_AAA flash_dpi_info;
_attribute_data_retention_user POLLING_RATE_INFO flash_polling_rate;

_attribute_data_retention_user u8 suspend_wake_up_enable = 1;
_attribute_data_retention_user u8 deep_flag = POWER_ON_ANA_AAA;

_attribute_data_retention_user u8 has_new_report_aaa;
_attribute_data_retention_user u8 has_new_key_event = 0;


//u8 last_mosue_btn_in_sensor;
_attribute_data_retention_user mouse_data_t ms_data;

_attribute_data_retention_user u8 wheel_pre = 0;
_attribute_data_retention_user u8 wheel_status;
_attribute_data_retention_user u8 connect_count;
u8 protocol_send_flag;
u8 sensor_operation_flag = 1;
u32 sensor_operation_tick = 0;
u8 mouse_effect_save_flag = 0;
u8 dongle_ble_web = 0;
u8 super_mouse_flag = 0;
void save_dev_info_flash()
{
    dev_info_idx += SAVE_MAX_IN_FLASH;
    flash_write_page(CFG_DEVICE_MODE_ADDR + dev_info_idx, SAVE_MAX_IN_FLASH, (u8 *)&flash_dev_info.dongle_id);
    //sleep_us(50000);
}

void save_sensor_info()
{
#if SENSOR_FUN_ENABLE_AAA
	mos_proxy_get_instance()->set_cpi(flash_dpi_info.bt_dpi);
#if DPI_SAVE_FLASH
	flash_dpi_info.idx += sizeof(flash_dpi_info);
	if(flash_dpi_info.idx < 4096)
	{
		flash_write_page(MOUSE_DPI_ADDR + flash_dpi_info.idx, sizeof(flash_dpi_info), (u8 *)&flash_dpi_info.bt_dpi);
	}
	else
	{
		flash_dpi_info.idx -= sizeof(flash_dpi_info);
	}
#endif
#endif
}

void save_polling_rate_info()
{
	flash_polling_rate.idx += sizeof(POLLING_RATE_INFO);
	if(flash_polling_rate.idx < 4096)
	{
		flash_write_page(MOUSE_POLLING_RATE_ADDR + flash_polling_rate.idx, sizeof(POLLING_RATE_INFO), (u8 *)&flash_polling_rate);
	}
	else
	{
		flash_polling_rate.idx -= sizeof(POLLING_RATE_INFO);
	}
}

u8 mouse_mac_address[6] = {0};
#define Pro_version (21)
_attribute_data_retention_user u8 mouse_version[4] = {0x01, 0x00, 0x00, Pro_version}; //Hardware_version,Firmware_version_official,Firmware_version_test1,Firmware_version_test0
_attribute_data_retention_user u8 dongle_version[4] = {0};
_attribute_data_retention_user u8 charging_batt_count = 0;
_attribute_data_retention_user u8 has_web_key_flag = 0;
_attribute_data_retention_user u8 ble_adv_channel = 1;
_attribute_data_retention_user u32 record_channel = 1;

_attribute_data_retention_user u32 btn_tick = 0;
_attribute_data_retention_user u32 btn_pins[BTN_NUM_AAA] = BTN_MATRIX;
_attribute_data_retention_user u16 btn_value = 0;
void btn_set_wakeup_level_suspend(u8 enable)
{
    for (u8 i = 0; i < BTN_NUM_AAA; i++)
    {
        if (gpio_read(btn_pins[i]))
        {
            cpu_set_gpio_wakeup(btn_pins[i], 0, enable); //low wakeup suspend
        }
        else
        {
            cpu_set_gpio_wakeup(btn_pins[i], 1, enable);
        }
    }
}

void btn_set_wakeup_level_deep()
{
    for (u8 i = 0; i < BTN_NUM_AAA; i++)
    {
        if (gpio_read(btn_pins[i]))
        {
            cpu_set_gpio_wakeup(btn_pins[i], 0, 1); //low wakeup suspend
            //connected_idle_time_count_ykq=0;
        }
        else
        {
            cpu_set_gpio_wakeup(btn_pins[i], 1, 1);
        }
    }
}

static u8 VOICE_BUTTON_INDEX = 0;
static u8 side_1_btn_state = 0;
static u8 side_2_btn_state = 0;
static u8 reset_cmd = 0;
static u8 key_up_buf[8] = {0, 0, 0, 0, 0, 0, 0, 0};
extern void reset_rgb();

static u16 side_1_btn_action(unsigned int gpio_state)
{
	u16 now_value = 0;
    if (!gpio_state)
    {
    	now_value |= MS_BTN_SIDE1;
    	if(mouse_propertie_param.side_button1_param.flag == 3)
    		now_value |= mouse_propertie_param.side_button1_param.data[0];
        if (!side_1_btn_state)
        {
            side_1_btn_state = 1;
            switch (mouse_propertie_param.side_button1_param.flag)
            {
            case 1:
            {
                if (mouse_propertie_param.side_button1_param.type == 0) //normal keyboard
                {
                	push_data_out_fifo(mouse_propertie_param.side_button1_param.data, OUT_KEYBOARD_REPORT, 8);
                }
                else if (mouse_propertie_param.side_button1_param.type == 1) //consume keyboard
                {
                	push_data_out_fifo(mouse_propertie_param.side_button1_param.data, OUT_CONSUMER_REPORT, 2);
                }
                break;
            }
            case 3:
            	break;
            default:
            	get_logic_protocol_manager()->send_notify_data(get_protocol_manager_instance()->event_pack(TYPE_KEY, SIDE_BUTTON_1_INDEX | KEY_DOWN));
            	break;
            }
        }
    }
    else
    {
        if (side_1_btn_state)
        {
            side_1_btn_state = 0;
            switch (mouse_propertie_param.side_button1_param.flag)
            {
            case 1:
            {
                if (mouse_propertie_param.side_button1_param.type == 0) //normal keyboard
                {
                	push_data_out_fifo(key_up_buf, OUT_KEYBOARD_REPORT, 8);
                }
                else if (mouse_propertie_param.side_button1_param.type == 1) //consume keyboard
                {
                	push_data_out_fifo(key_up_buf, OUT_CONSUMER_REPORT, 2);
                }
            }
            break;
            case 0:
            {
				if(mouse_propertie_param.side_button1_param.type == 0)
				{
					if(mouse_propertie_param.RGB_theme == RGB_NORMAL_MODE)
					{
						mouse_propertie_param.RGB_index = (mouse_propertie_param.RGB_index + 1) % 5;
						mouse_propertie_param.userdefined_RGB_param.rgb_data = get_rgb_value_by_index(mouse_propertie_param.RGB_index);
						if(ui_mic_enable == 0 && lowbatt_flag == 0 && charging_batt_count == 0)
						{
							rgb_back()->set_single_colour(rgb_back(), close_all_rgb);
							rgb_front()->set_single_colour(rgb_front(), close_all_rgb);
						}
						mouse_effect_save_flag = 1;
					}
					else if(mouse_propertie_param.RGB_theme==RGB_TRIGGER_MODE)
					{
						mouse_propertie_param.RGB_index = (mouse_propertie_param.RGB_index + 1) % 5;
						mouse_propertie_param.breathe_RGB_param.rgb_data[0] = get_rgb_value_by_index(mouse_propertie_param.RGB_index);
						if(ui_mic_enable == 0 && lowbatt_flag == 0 && charging_batt_count == 0)
						{
							rgb_back()->set_single_colour(rgb_back(), close_all_rgb);
							rgb_back()->set_gradient_colour(rgb_back(), BREATHING_PRIORITY, 1, 0, mouse_propertie_param.breathe_RGB_param.cycle_time,
									(rgb_data *)mouse_propertie_param.breathe_RGB_param.rgb_data, mouse_propertie_param.breathe_RGB_param.rgb_data_len);
							rgb_front()->set_single_colour(rgb_front(), close_all_rgb);
							rgb_front()->set_gradient_colour(rgb_front(), BREATHING_PRIORITY, 1, 0, mouse_propertie_param.breathe_RGB_param.cycle_time,
									(rgb_data *)mouse_propertie_param.breathe_RGB_param.rgb_data, mouse_propertie_param.breathe_RGB_param.rgb_data_len);
						}
						mouse_effect_save_flag = 1;
					}
				}
				else if(mouse_propertie_param.side_button1_param.type == 1)
				{
#if SENSOR_FUN_ENABLE_AAA
					flash_dpi_info.bt_dpi++;
					if (flash_dpi_info.bt_dpi > 6)
						flash_dpi_info.bt_dpi = 0;
					save_sensor_info();
#endif
				}
            	get_logic_protocol_manager()->send_notify_data(get_protocol_manager_instance()->event_pack(TYPE_KEY, SIDE_BUTTON_1_INDEX | KEY_UP));
            	break;
            }
            case 3:
				break;
            default:
            	get_logic_protocol_manager()->send_notify_data(get_protocol_manager_instance()->event_pack(TYPE_KEY, SIDE_BUTTON_1_INDEX | KEY_UP));
            	break;
            }
        }
    }
    return now_value;
}

static u16 side_2_btn_action(unsigned int gpio_state)
{
	u16 now_value = 0;
    if (!gpio_state)
    {
    	now_value |= MS_BTN_SIDE2;
    	if(mouse_propertie_param.side_button2_param.flag == 3)
			now_value |= mouse_propertie_param.side_button2_param.data[0];
        if (side_2_btn_state == 0)
        {
            side_2_btn_state = 1;

            switch (mouse_propertie_param.side_button2_param.flag)
            {
            case 1:
            {
                if (mouse_propertie_param.side_button2_param.type == 0) //normal keyboard
                {
                	push_data_out_fifo(mouse_propertie_param.side_button2_param.data, OUT_KEYBOARD_REPORT, 8);
                }
                else if (mouse_propertie_param.side_button2_param.type == 1) //consume keyboard
                {
                	push_data_out_fifo(mouse_propertie_param.side_button2_param.data, OUT_CONSUMER_REPORT, 2);
                }
            }
            break;
            case 3:
				break;
            default:
            {
                get_logic_protocol_manager()->send_notify_data(get_protocol_manager_instance()->event_pack(TYPE_KEY, SIDE_BUTTON_2_INDEX | KEY_DOWN));
            }
            break;
            }
        }
    }
    else
    {
        if (side_2_btn_state == 1)
        {
            side_2_btn_state = 0;

			switch (mouse_propertie_param.side_button2_param.flag)
			{
			case 1:
			{
				if (mouse_propertie_param.side_button2_param.type == 0) //normal keyboard
				{
					push_data_out_fifo(&key_up_buf[0], OUT_KEYBOARD_REPORT, 8);
				}
				else if (mouse_propertie_param.side_button2_param.type == 1) //consume keyboard
				{
					push_data_out_fifo(&key_up_buf[0], OUT_CONSUMER_REPORT, 2);
				}
			}
			break;
			case 0:
			{
				if(mouse_propertie_param.side_button2_param.type == 0)
				{
					if(mouse_propertie_param.RGB_theme == RGB_NORMAL_MODE)
					{
						mouse_propertie_param.RGB_index = (mouse_propertie_param.RGB_index + 1) % 5;
						mouse_propertie_param.userdefined_RGB_param.rgb_data = get_rgb_value_by_index(mouse_propertie_param.RGB_index);
						if(ui_mic_enable == 0 && lowbatt_flag == 0 && charging_batt_count == 0)
						{
							rgb_back()->set_single_colour(rgb_back(), close_all_rgb);
							rgb_front()->set_single_colour(rgb_front(), close_all_rgb);
						}
						mouse_effect_save_flag = 1;
					}
					else if(mouse_propertie_param.RGB_theme==RGB_TRIGGER_MODE)
					{
						mouse_propertie_param.RGB_index = (mouse_propertie_param.RGB_index + 1) % 5;
						mouse_propertie_param.breathe_RGB_param.rgb_data[0] = get_rgb_value_by_index(mouse_propertie_param.RGB_index);
						if(ui_mic_enable == 0 && lowbatt_flag == 0 && charging_batt_count == 0)
						{
							rgb_back()->set_single_colour(rgb_back(), close_all_rgb);
							rgb_back()->set_gradient_colour(rgb_back(), BREATHING_PRIORITY, 1, 0, mouse_propertie_param.breathe_RGB_param.cycle_time,
									(rgb_data *)mouse_propertie_param.breathe_RGB_param.rgb_data, mouse_propertie_param.breathe_RGB_param.rgb_data_len);
							rgb_front()->set_single_colour(rgb_front(), close_all_rgb);
							rgb_front()->set_gradient_colour(rgb_front(), BREATHING_PRIORITY, 1, 0, mouse_propertie_param.breathe_RGB_param.cycle_time,
									(rgb_data *)mouse_propertie_param.breathe_RGB_param.rgb_data, mouse_propertie_param.breathe_RGB_param.rgb_data_len);
						}
						mouse_effect_save_flag = 1;
					}
				}
				else if(mouse_propertie_param.side_button2_param.type == 1)
				{
#if SENSOR_FUN_ENABLE_AAA
					flash_dpi_info.bt_dpi++;
					if (flash_dpi_info.bt_dpi > 6)
						flash_dpi_info.bt_dpi = 0;
					save_sensor_info();
#endif
				}
				get_logic_protocol_manager()->send_notify_data(get_protocol_manager_instance()->event_pack(TYPE_KEY, SIDE_BUTTON_2_INDEX | KEY_UP));
				break;
			}
			case 3:
				break;
			default:
				get_logic_protocol_manager()->send_notify_data(get_protocol_manager_instance()->event_pack(TYPE_KEY, SIDE_BUTTON_2_INDEX | KEY_UP));
				break;
			}
        }
    }
    return now_value;
}

u16 btn_scan()
{
    u16 now_value = 0;
    if (!reset_cmd)
    {
        if (!gpio_read(PIN_BTN_LEFT))
        {
            now_value |= MS_BTN_LEFT;
        }
        if (!gpio_read(PIN_BTN_RIGHT))
        {
            now_value |= MS_BTN_RIGHT;
        }
        if (!gpio_read(PIN_BTN_MIDDLE))
        {
            now_value |= MS_BTN_MIDDLE;
        }
        ///////////////////////////////////////////////////PIN_BTN_SIDE_BUTTON1////////////////////////////////////////////////////////////////

        now_value |= side_1_btn_action(gpio_read(PIN_BTN_SIDE_BUTTON_1));
        now_value |= side_2_btn_action(gpio_read(PIN_BTN_SIDE_BUTTON_2));

        //////////////////////////////////////////////PIN_BTN_VOICE///////////////////////////////////////////////////////////////
        if (!gpio_read(PIN_BTN_VOICE)) ///////send_key_down
        {
            if (VOICE_BUTTON_INDEX == 0) //////////////////If the flag bit is not set, the data will always be sent up
            {
                VOICE_BUTTON_INDEX = 1;
                get_logic_protocol_manager()->send_notify_data(get_protocol_manager_instance()->event_pack(TYPE_KEY, VOICE_INDEX | KEY_DOWN));
                // printf("KEY_DOWN\r\n");
            }
            now_value |= MS_BTN_VOICE; ///////////////You can press this button to operate the voice. It needs to be turned on. Otherwise, after low power consumption, you can't wake up by pressing this button
        }
        else
        {
            if (VOICE_BUTTON_INDEX == 1)
            {
                VOICE_BUTTON_INDEX = 0;
                get_logic_protocol_manager()->send_notify_data(get_protocol_manager_instance()->event_pack(TYPE_KEY, VOICE_INDEX | KEY_UP));
                // printf("KEY_UP\r\n");
            }
        }
    }
    //////////////////////////////////////////////GPIO_RESET/////////////////////////////////////////////////////////////////////////
    if (!gpio_read(GPIO_RESET))
    {
        now_value |= MS_BTN_RESET;
        reset_cmd = 1;
    }
    else
        reset_cmd = 0;
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    return now_value;
}

u8 btn_get_value()
{
    u8 ret = 0;
    _attribute_data_retention_user static u8 debounce = 0;
    u16 now_value = 0;
    _attribute_data_retention_user static u16 last_value = 0;

    now_value = btn_scan();

    if (last_value != now_value)
    {
        reset_idle_status();
        debounce = 1;
        last_value = now_value;
    }

    if (debounce)
    {
        debounce++;
        if (debounce == 3)
        {
            ret = NEW_KEY_EVENT_AAA;
            ms_data.btn = now_value & 0X1F;
            btn_tick = clock_time();
            debounce = 0;
            if(now_value & (now_value ^ btn_value))
            {
            	if(RGB_TRIGGER_MODE == mouse_propertie_param.RGB_theme && ui_mic_enable == 0 && lowbatt_flag == 0 && charging_batt_count == 0)
				{
					rgb_back()->set_single_colour(rgb_back(), close_all_rgb);
					rgb_back()->set_gradient_colour(rgb_back(), BREATHING_PRIORITY, 1, 0, mouse_propertie_param.breathe_RGB_param.cycle_time,
							(rgb_data *)mouse_propertie_param.breathe_RGB_param.rgb_data, mouse_propertie_param.breathe_RGB_param.rgb_data_len);
					rgb_front()->set_single_colour(rgb_front(), close_all_rgb);
					rgb_front()->set_gradient_colour(rgb_front(), BREATHING_PRIORITY, 1, 0, mouse_propertie_param.breathe_RGB_param.cycle_time,
							(rgb_data *)mouse_propertie_param.breathe_RGB_param.rgb_data, mouse_propertie_param.breathe_RGB_param.rgb_data_len);
				}
            }

            btn_value = now_value;
        }
    }
    // button_process_telink_v21a(ret);
    return ret;
}

#if WHEEL_FUN_ENABLE_AAA
void wheel_set_wakeup_level_suspend(u8 enable)
{
#if 1

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
#endif
}

void wheel_set_wakeup_level_deep()
{
#if 1
    if (gpio_read(PIN_WHEEL_1))
    {

        cpu_set_gpio_wakeup(PIN_WHEEL_1, 0, 1);
    }
    else
    {
        cpu_set_gpio_wakeup(PIN_WHEEL_1, 1, 1);
    }

    if (gpio_read(PIN_WHEEL_2))
    {
        cpu_set_gpio_wakeup(PIN_WHEEL_2, 0, 1);
    }
    else
    {
        cpu_set_gpio_wakeup(PIN_WHEEL_2, 1, 1);
    }
#endif
}
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

_attribute_ram_code_ s8 mouse_wheel_process(u32 wheel_prepare_tick)
{
    s8 ret = 0;
    while (read_reg8(0xd8) & 0x01)
    {
        if (clock_time_exceed(wheel_prepare_tick, 260)) //4 cylce is enough: 4*1/32k = 1/8 ms
        {
            write_reg8(0xd6, 0x01); //reset  d6[0]
            write_reg8(0xd6, 0x00);
            break;
        }
    }

#if 1 //(WHEEL_TWO_STEP_PROC)
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
#else
    ret = read_reg8(0xd0);
#endif
    return ret;
}

_attribute_ram_code_ u8 wheel_get_value(u32 wheel_prepare_tick)
{
    u8 ret = 0;
    s8 wheel_now = 0;
    wheel_now = -mouse_wheel_process(wheel_prepare_tick);

//    if (user_cfg.wheel_direct != U8_MAX)
//    {
//        wheel_now = -wheel_now;
//    }
    ms_data.wheel = wheel_now;
    if (wheel_now != 0)
    {
        ret = WHEEL_DATA_EVENT_AAA;
    }
    return ret;
}
#endif

struct mblue_fifo super_mouse_fifo;
static u8 super_mouse_fifo_buffer[256] = {0};
#define SUPER_MOUSE_PACK_SIZE (7)

static void super_mouse_init(){

	mblue_fifo_construct(&super_mouse_fifo, super_mouse_fifo_buffer, sizeof(super_mouse_fifo_buffer));
}

u32 super_mouse_push(u8 *data, u32 size){
	return super_mouse_fifo.put(&super_mouse_fifo, data, size);
}

u32 super_mouse_proc_tick = 0;
void super_mouse_proc(){
	if((flash_polling_rate.ble_frequency <= DEFAULT_BLE_FREQUENCY) || (!super_mouse_flag))
	{
		if(super_mouse_fifo.len(&super_mouse_fifo))
		{
			super_mouse_fifo.reset(&super_mouse_fifo);
		}
		return;
	}
	super_mouse_proc_tick ++;
	if((super_mouse_proc_tick * DEFAULT_BLE_FREQUENCY) < flash_polling_rate.ble_frequency)
		return;
	super_mouse_proc_tick = 0;
	u32 len = super_mouse_fifo.len(&super_mouse_fifo);
	if(len){
		u8 mouse_data[120] = {0};
		u8 ret = super_mouse_fifo.get(&super_mouse_fifo, mouse_data, len);
		if(ret == len){
			struct command_data * data = get_protocol_manager_instance()->large_event_pack(3, mouse_data, ret);
			if(data && data->ucLen > 0){
				push_data_out_fifo(data->pcData, OUT_PROT_REPORT, data->ucLen);
			}
		}
	}
}

void hw_init()
{
#if WHEEL_FUN_ENABLE_AAA
    wheel_init_hw();
#endif
#if BLT_APP_LED_ENABLE
    led_hw_init();
#endif
#if PWM_LED_ENABLE
    rgb_back()->init(rgb_back());
    rgb_front()->init(rgb_front());
#endif
#if SENSOR_FUN_ENABLE_AAA
	mos_proxy_get_instance()->set_cpi(flash_dpi_info.bt_dpi);
#endif
	super_mouse_init();
}

void push_report_fifo()
{
	if (has_new_report_aaa & OUT_MOUSE_REPORT)
	{
		ms_data.y = -ms_data.y;
		push_data_out_fifo(&ms_data.btn, OUT_MOUSE_REPORT, sizeof(mouse_data_t));
		ms_data.x = 0;
		ms_data.y = 0;
		has_new_report_aaa &= ~OUT_MOUSE_REPORT;
	}
}

u8 ble_notify_data_proc(u16 type, u8 *data, u32 size)
{
	u16 data_type = 0;
	if(!connect_ok)
		return 1;
	switch(type)
	{
		case OUT_KEYBOARD_REPORT:
			data_type = HID_NORMAL_KB_REPORT_INPUT_DP_H;
			break;
		case OUT_MOUSE_REPORT:
			if((ble_status_aaa != T5S_CONNECTED_STATUS_AAA))
				return 1;
			if((flash_polling_rate.ble_frequency > DEFAULT_BLE_FREQUENCY) && super_mouse_flag)
			{
				return super_mouse_push(data, size);
			}
			data_type = HID_MOUSE_REPORT_INPUT_DP_H;
			break;
		case OUT_PROT_REPORT:
			data_type = PROTOCOL_TX_DP_H;
			break;
		case OUT_SPP_REPORT:
		{
			data_type = SPP_INPUT_DP_H;
			break;
		}
		default:
		{
			protocol_send_flag = 0;
			return 1;
		}
	}
	u8 txFifoNumber = blc_ll_getTxFifoNumber();
	if (txFifoNumber < RF_TX_FIFO_AUDIO_ALLOW_NUM)
	{
		u8 status = bls_att_pushNotifyData(data_type, data, size);
		if (status == BLE_SUCCESS)
		{
			return 1;
		}
	}
	return 0;
}

#if BLE_WEB_ENABLE
#define WEB_HID_DELAY 0
y_kbdata_t web_data[] = {
	{VK_MSK_LWIN, VK_R, 50},
	{0, VK_H, WEB_HID_DELAY},
	{0, VK_T, WEB_HID_DELAY},
	{0, VK_NONE, WEB_HID_DELAY},
	{0, VK_T, WEB_HID_DELAY},
	{0, VK_P, WEB_HID_DELAY},
	//{0, VK_S, WEB_HID_DELAY},
	{VK_MSK_LSHIFT, VK_SEMICOLON, WEB_HID_DELAY}, //:;
	{0, VK_SLASH, WEB_HID_DELAY},				  // /
	{0, VK_NONE, WEB_HID_DELAY},
	{0, VK_SLASH, WEB_HID_DELAY},
	{0, VK_1, WEB_HID_DELAY},
	{0, VK_0, WEB_HID_DELAY},
	{0, VK_2, WEB_HID_DELAY},
	{0, VK_NONE, WEB_HID_DELAY},
	{0, VK_3, WEB_HID_DELAY},
	{0, VK_PERIOD, WEB_HID_DELAY}, //.>
	{0, VK_M, WEB_HID_DELAY},
	{0, VK_A, WEB_HID_DELAY},
	{0, VK_D, WEB_HID_DELAY},
	{0, VK_A, WEB_HID_DELAY},
	{0, VK_O, WEB_HID_DELAY},
	{0, VK_G, WEB_HID_DELAY},
	{0, VK_O, WEB_HID_DELAY},
	{0, VK_PERIOD, WEB_HID_DELAY},
	{0, VK_C, WEB_HID_DELAY},
	{0, VK_O, WEB_HID_DELAY},
	{0, VK_M, WEB_HID_DELAY},
	{VK_MSK_LSHIFT, 0, WEB_HID_DELAY},
	{0, VK_NONE, 20},
	{0, VK_ENTER, WEB_HID_DELAY},
	{0, VK_NONE, WEB_HID_DELAY},
};

void open_website()
{
	static u8 addr_cnt = 0;
	static u8 flag = 0;
	static u32 tick = 0;
	u8 buf[8] = {0, 0, 0, 0, 0, 0, 0};
	if (flag == 0)
	{
		buf[0] = web_data[addr_cnt].ctrl_key;
		buf[2] = web_data[addr_cnt].keycode;
		if (push_data_out_fifo(buf, OUT_KEYBOARD_REPORT, 8))
		{
			flag = 2;
		}
	}
	else if (flag == 1)
	{
		if (push_data_out_fifo(buf, OUT_KEYBOARD_REPORT, 8))
		{
			flag = 2;
		}
	}
	else if (flag == 2)
	{
		if (web_data[addr_cnt].next_interval)
		{
			tick = clock_time();
			flag = 3;
		}
		else
		{
			flag = 4;
		}
	}
	else if ((flag == 3) && clock_time_exceed(tick, web_data[addr_cnt].next_interval * 10000))
	{
		flag = 4;
	}
	else if (flag == 4)
	{
		flag = 0;
		addr_cnt++;
		if (addr_cnt >= sizeof(web_data)/sizeof(y_kbdata_t))
		{
			addr_cnt = 0;
			has_web_key_flag = 0;
		}
	}
}
#endif

void write_deep_ana0(u8 buf)
{
    deep_flag = buf;
    analog_write(DEEP_ANA_REG0, buf);
}
void user_reboot(u8 reason)
{
    write_deep_ana0(reason);
    start_reboot();
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
    protocol_send_tick = clock_time() | 1;
    get_logic_protocol_manager()->parse(buf, len);
    reset_idle_status();
    return 1;
}
u8 get_ble_data_report_aaa()
{
#if WHEEL_FUN_ENABLE_AAA
    u32 wheel_prepare_tick;
    wheel_prepare_tick = mouse_wheel_prepare_tick();
//    printf("mode_keycode_flag:%d\r\n",mode_keycode_flag);
#endif
#if BUTTON_FUN_ENABLE_AAA
    if (mode_keycode_flag == OS_TYPE_WINDOWS && !((1 << ble_adv_channel) & web_flag)) //未和语音助手进行交互时，(ble_adv_channel & web_flag)为0
    {
        check_web_process_aaa();
    }
    check_reset();
    has_new_key_event |= btn_get_value();
    if(mouse_effect_save_flag)
    {
      if(updata_mouse_device_data(&mouse_propertie_param))
      {
    	  mouse_effect_save_flag = 0;
      }
    }
    if(flash_need_erase_flag1)
    {
		if (bls_ll_requestConnBrxEventDisable()>300)
		{
			bls_ll_disableConnBrxEvent();
			flash_erase_sector(MOUSE_DEVICE_DATA_ADDR);
			bls_ll_restoreConnBrxEvent();
			flash_need_erase_flag1 = 0;
		}
    }
    if(flash_need_erase_flag2)
     {
 		if (bls_ll_requestConnBrxEventDisable()>300)
 		{
 			bls_ll_disableConnBrxEvent();
 			flash_erase_sector(MOUSE_DEVICE_DATA_ADDR1);
 			bls_ll_restoreConnBrxEvent();
 			flash_need_erase_flag2 = 0;

 		}
     }
#endif

#if SENSOR_FUN_ENABLE_AAA
	if(mos_proxy_get_instance()->query_motion_status())
	{
		ms_data.x = mos_proxy_get_instance()->query_deltax();
		ms_data.y = mos_proxy_get_instance()->query_deltay();
		has_new_key_event |= SENSOR_DATA_EVENT_AAA;
		sensor_operation_tick = clock_time();
	}
#endif

#if WHEEL_FUN_ENABLE_AAA
    has_new_key_event |= wheel_get_value(wheel_prepare_tick);
    //sensor_set_wakeup_level_suspend();
#endif

    if (connect_ok && (ble_status_aaa == T5S_CONNECTED_STATUS_AAA)){
    	 if (has_new_key_event & (SENSOR_DATA_EVENT_AAA | WHEEL_DATA_EVENT_AAA | NEW_KEY_EVENT_AAA))
		{
    		reset_idle_status();
			has_new_report_aaa |= OUT_MOUSE_REPORT;
			has_new_key_event = 0;
		}
		push_report_fifo();
    }
    return has_new_key_event;
}
