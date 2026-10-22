/********************************************************************************************************
 * @file     aaa_led.c
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
#include "led_manager.h"

#if BLT_APP_LED_ENABLE
typedef enum
{
	INFINITE_LED_PRIORITY,
	BAT_SLVD_LED_PRIORITY,
	BAT_LVD_LED_PRIORITY,
	BAT_NORMAL_LED_PRIORITY,
	CHARGEING_BATT_PRIORITY,
	MODE_CHANGE_LED_PRIORITY,
	CHOOSE_CHANNEL_PRIORITY,
	DIRE_LED_PRIORITY,
	UNDIRE_LED_PRIORITY,
	START_AUDIO_PRIORITY,
	STOP_AUDIO_PRIORITY,
	CONN_LED_PRIORITY,
	FIRST_CONN_LED_PRIORITY,
	CLOSE_CHANNEL_PRIORITY,
} LED_CFG_ENUM_AAA;

static struct led_manager ble_led_handle1;
static struct led_manager ble_led_handle2;
static struct led_manager ble_led_handle3;
static struct led_manager battery_led_handle;
static struct led_manager fulled_batt_led_handle;
static struct led_manager voice_led_handle;


const led_cfg infinite_led = {1000, 0, 1, INFINITE_LED_PRIORITY};			//指示灯常亮
const led_cfg bat_lvd_led = {200, 5000, 1, BAT_LVD_LED_PRIORITY};		   	//低电
const led_cfg bat_slvd_led= {100, 100, 1, BAT_SLVD_LED_PRIORITY};			//严重低电
const led_cfg bat_normal_led= {0, 1, 1, BAT_NORMAL_LED_PRIORITY};			//正常电量
const led_cfg charging_batt_led= {1, 0, 0xff, CHARGEING_BATT_PRIORITY};		//正在充电
const led_cfg dirce_ble_led = {1000, 1000, 1, DIRE_LED_PRIORITY};	   		//定向广播
const led_cfg undirec_ble_led = {300, 300, 1, UNDIRE_LED_PRIORITY};	   		//全向广播
const led_cfg mode_change_led = {3000, 0, 1, MODE_CHANGE_LED_PRIORITY}; 	//模式切换
const led_cfg first_connected_led = {3000, 0, 1, FIRST_CONN_LED_PRIORITY}; 	//首次连接
const led_cfg start_audio_led = {1, 0, 0xff, START_AUDIO_PRIORITY};			//语音开
const led_cfg stop_audio_led = {0, 1, 1, STOP_AUDIO_PRIORITY};				//语音关
const led_cfg connected_led = {3000, 0, 1, CONN_LED_PRIORITY};				//连接
const led_cfg choose_channel={3000, 0, 1, CHOOSE_CHANNEL_PRIORITY};			//选择当前通道
const led_cfg close_channel={0, 1, 1, CLOSE_CHANNEL_PRIORITY};				//关闭当前通道蓝牙灯

inline u8 get_ble_led_condition()
{
	return ble_led_handle1.led_param.repeatCount || ble_led_handle3.led_param.repeatCount || ble_led_handle2.led_param.repeatCount;
}

inline u8 get_ble_led_state()
{
	return ble_led_handle1.led_param.isOn | (ble_led_handle3.led_param.isOn << 2) | (ble_led_handle2.led_param.isOn << 1);
}

void ble_indicator_light_on()
{
	ble_led_handle1.set(&ble_led_handle1, infinite_led);
	ble_led_handle2.set(&ble_led_handle2, infinite_led);
	ble_led_handle3.set(&ble_led_handle3, infinite_led);
}

void ble_close_all_led()
{
	ble_led_handle1.set(&ble_led_handle1, close_channel);
	ble_led_handle2.set(&ble_led_handle2, close_channel);
	ble_led_handle3.set(&ble_led_handle3, close_channel);
}

void led_low_battery_display(u8 lowbatt_flag)//低电
{
	if(!battery_get_detect_enable()){
		if(lowbatt_flag==1){
			battery_led_handle.set(&battery_led_handle, bat_lvd_led);
		}
		else if(lowbatt_flag==2){
			battery_led_handle.set(&battery_led_handle, bat_slvd_led);
		}
	}
}
void led_close_channel()
{
	if(ble_adv_channel == CHANNEL_1)
	{
		ble_led_handle1.set(&ble_led_handle1, close_channel);
	}
	else if(ble_adv_channel==CHANNEL_2)
	{
		ble_led_handle2.set(&ble_led_handle2, close_channel);
	}
	else if(ble_adv_channel==CHANNEL_3)
	{
		ble_led_handle3.set(&ble_led_handle3, close_channel);
	}
}

void led_choose_channel()
{
	if(ble_adv_channel==CHANNEL_1)
	{
		ble_led_handle1.set(&ble_led_handle1, choose_channel);
	}
	else if(ble_adv_channel==CHANNEL_2)
	{
		ble_led_handle2.set(&ble_led_handle2, choose_channel);
	}
	else if(ble_adv_channel==CHANNEL_3)
	{
		ble_led_handle3.set(&ble_led_handle3, choose_channel);
	}
}

void led_caps_set(u8 status)
{
#if !UART_PRINT_DEBUG_ENABLE
	gpio_write(PIN_CAPS_LED, status);
#endif
}

void close_all_led()
{
	ble_close_all_led();
	voice_led_handle.set(&voice_led_handle, close_channel);
	led_caps_set(0);
}

void led_ble_mode_display()
{
	if(ble_adv_channel==CHANNEL_1)
	{
		if(directed_adv_flag)
		{
			ble_led_handle1.set(&ble_led_handle1, dirce_ble_led);
		}
		else
		{
			ble_led_handle1.set(&ble_led_handle1, undirec_ble_led);
		}
	}
	else if(ble_adv_channel==CHANNEL_2)
	{
		if(directed_adv_flag)
		{
			ble_led_handle2.set(&ble_led_handle2, dirce_ble_led);
		}
		else
		{
			ble_led_handle2.set(&ble_led_handle2, undirec_ble_led);
		}
	}
	else if(ble_adv_channel==CHANNEL_3)
	{
		if(directed_adv_flag)
		{
			ble_led_handle3.set(&ble_led_handle3, dirce_ble_led);
		}
		else
		{
			ble_led_handle3.set(&ble_led_handle3, undirec_ble_led);
		}
	}

}

void led_com_mode_change()
{
	if(ble_adv_channel==CHANNEL_1)
	{
		ble_led_handle1.set(&ble_led_handle1, mode_change_led);
	}
	else if(ble_adv_channel==CHANNEL_2)
	{
		ble_led_handle2.set(&ble_led_handle2, mode_change_led);
	}
	else if(ble_adv_channel==CHANNEL_3)
	{
		ble_led_handle3.set(&ble_led_handle3, mode_change_led);
	}
}

void led_ble_first_connected_display()
{
	if(ble_adv_channel==CHANNEL_1)
	{
		ble_led_handle1.set(&ble_led_handle1, first_connected_led);
	}
	else if(ble_adv_channel==CHANNEL_2)
	{
		ble_led_handle2.set(&ble_led_handle2, first_connected_led);
	}
	else if(ble_adv_channel==CHANNEL_3)
	{
		ble_led_handle3.set(&ble_led_handle3, first_connected_led);
	}
}

void led_ble_connected_display()
{
	if(ble_adv_channel==CHANNEL_1)
	{
		ble_led_handle1.set(&ble_led_handle1, connected_led);
	}
	else if(ble_adv_channel==CHANNEL_2)
	{
		ble_led_handle2.set(&ble_led_handle2, connected_led);
	}
	else if(ble_adv_channel==CHANNEL_3)
	{
		ble_led_handle3.set(&ble_led_handle3, connected_led);
	}
}

// charger manager
void RGB_charging_light()// 正在充电
{
	// 关闭绿灯，打开红灯
	fulled_batt_led_handle.set(&fulled_batt_led_handle, bat_normal_led);// close full light
	battery_led_handle.set(&battery_led_handle, charging_batt_led);// open charging light
//	gpio_write(PIN_FILLED_LED, 1);
//	gpio_write(PIN_BAT_LED, 0);
}

void RGB_fully_charged_batt_light()// 充满电
{
	// 关闭红灯，打开绿灯
	battery_led_handle.set(&battery_led_handle, bat_normal_led);
	fulled_batt_led_handle.set(&fulled_batt_led_handle, charging_batt_led);
//	gpio_write(PIN_BAT_LED, 1);
//	gpio_write(PIN_FILLED_LED, 0);
}

void RGB_pull_out_light()// 拔出充电器灯光状态（关闭）
{
	// close all battery/charging LED
	battery_led_handle.set(&battery_led_handle, bat_normal_led);
	fulled_batt_led_handle.set(&fulled_batt_led_handle, bat_normal_led);
}

void start_audio_light()
{
	voice_led_handle.set(&voice_led_handle, start_audio_led);
}

void stop_audio_light()
{
	voice_led_handle.set(&voice_led_handle, stop_audio_led);
}

void led_hw_init()
{
	led_manager_construct(&ble_led_handle1);
	led_manager_construct(&ble_led_handle2);
	led_manager_construct(&ble_led_handle3);
	led_manager_construct(&battery_led_handle);
	led_manager_construct(&fulled_batt_led_handle);
	led_manager_construct(&voice_led_handle);

	ble_led_handle1.init(&ble_led_handle1, PIN_BLE1_LED, LED_ON_AAA);
	ble_led_handle2.init(&ble_led_handle2, PIN_BLE2_LED, LED_ON_AAA);
	ble_led_handle3.init(&ble_led_handle3, PIN_BLE3_LED, LED_ON_AAA);
	battery_led_handle.init(&battery_led_handle, PIN_BAT_LED, LED_OFF_AAA);
	fulled_batt_led_handle.init(&fulled_batt_led_handle, PIN_FILLED_LED, LED_OFF_AAA);
	voice_led_handle.init(&voice_led_handle, PIN_VOICE_LED, LED_ON_AAA);

}

void led_hw_proc()
{
	if(!battery_get_detect_enable())
	{
		battery_led_handle.proc(&battery_led_handle);
		battery_led_handle.proc(&fulled_batt_led_handle);
	}
	ble_led_handle1.proc(&ble_led_handle1);
	ble_led_handle2.proc(&ble_led_handle2);
	ble_led_handle3.proc(&ble_led_handle3);
	voice_led_handle.proc(&voice_led_handle);

}

#endif
