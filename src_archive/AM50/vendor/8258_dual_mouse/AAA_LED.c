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

#if BLT_APP_LED_ENABLE

typedef enum
{
	ADV_DIRECTE_LED_PRIORITY,
	ADV_UNDIRECTE_LED_PRIORITY,
	CONNECTED_LED_PRIORITY,
	CLOSE_CHANNEL_PRIORITY,
} LED_CFG_ENUM_AAA;

struct gpio_manager blue_ble_led_handle;
struct gpio_manager green_ble_led_handle;
struct gpio_manager red_ble_led_handle;

const gpio_cfg adv_undirecte_ble_led = {200, 200, 1, ADV_UNDIRECTE_LED_PRIORITY};
const gpio_cfg adv_directe_ble_led = {1000, 1000, 1, ADV_DIRECTE_LED_PRIORITY};//1000ms
const gpio_cfg connected_led = {3000, 0, 1, CONNECTED_LED_PRIORITY};
const gpio_cfg close_channel={0, 1, 1, CLOSE_CHANNEL_PRIORITY};			//关闭当前通道蓝牙灯

u8 get_ble_led_condition()
{
	return (blue_ble_led_handle.gpio_param.repeatCount || green_ble_led_handle.gpio_param.repeatCount || red_ble_led_handle.gpio_param.repeatCount);
}

void led_close_channel(u8 ble_channel)
{
	switch (ble_channel)
	{
	case CHANNEL_1:
		blue_ble_led_handle.set(&blue_ble_led_handle,close_channel);
		break;
	case CHANNEL_2:
		green_ble_led_handle.set(&green_ble_led_handle,close_channel);
		break;
	case CHANNEL_3:
		red_ble_led_handle.set(&red_ble_led_handle,close_channel);
		break;
	default:
		break;
	}
}

void directe_adv_blue_set()
{
	led_close_channel(CHANNEL_3);//拉高是关闭，只需要关闭上一种颜色的灯光即可
	blue_ble_led_handle.set(&blue_ble_led_handle,adv_directe_ble_led);
}

void directe_adv_green_set()
{
	led_close_channel(CHANNEL_1);
	green_ble_led_handle.set(&green_ble_led_handle,adv_directe_ble_led);
}

void directe_adv_red_set()
{
	led_close_channel(CHANNEL_2);
	red_ble_led_handle.set(&red_ble_led_handle,adv_directe_ble_led);
}

void undirecte_adv_blue_set()
{
	led_close_channel(CHANNEL_3);
	blue_ble_led_handle.set(&blue_ble_led_handle,adv_undirecte_ble_led);
}

void undirecte_adv_green_set()
{
	led_close_channel(CHANNEL_1);
	green_ble_led_handle.set(&green_ble_led_handle,adv_undirecte_ble_led);
}

void undirecte_adv_red_set()
{
	led_close_channel(CHANNEL_2);
	red_ble_led_handle.set(&red_ble_led_handle,adv_undirecte_ble_led);
}

void led_ble_Adv()
{
#if BLE_LED_ENABLE
	if (direct_ble_flag)
	{
		switch (ble_adv_channel)
		{
		case CHANNEL_1:
			directe_adv_blue_set();
			break;
		case CHANNEL_2:
			directe_adv_green_set();
			break;
		case CHANNEL_3:
			directe_adv_red_set();
			break;
		default:
			break;
		}
	}
	else
	{
		switch (ble_adv_channel)
		{
		case CHANNEL_1:
			undirecte_adv_blue_set();
			break;
		case CHANNEL_2:
			undirecte_adv_green_set();
			break;
		case CHANNEL_3:
			undirecte_adv_red_set();
			break;
		default:
			break;
		}
	}
#endif
}

void led_Connected()
{
#if BLE_LED_ENABLE
	switch (ble_adv_channel)
	{
	case CHANNEL_1:
		led_close_channel(CHANNEL_3);
		blue_ble_led_handle.set(&blue_ble_led_handle,connected_led);
		break;
	case CHANNEL_2:
		led_close_channel(CHANNEL_1);
		green_ble_led_handle.set(&green_ble_led_handle,connected_led);
		break;
	case CHANNEL_3:
		led_close_channel(CHANNEL_2);
		red_ble_led_handle.set(&red_ble_led_handle,connected_led);
		break;
	default:
		break;
	}
#endif
}

void led_hw_init()
{
	gpio_manager_construct(&blue_ble_led_handle);
	gpio_manager_construct(&red_ble_led_handle);
	gpio_manager_construct(&green_ble_led_handle);

	blue_ble_led_handle.init(&blue_ble_led_handle, PIN_BLE_BLUE_LED, LED_ON_AAA);
	red_ble_led_handle.init(&red_ble_led_handle, PIN_BLE_RED_LED, LED_ON_AAA);
	green_ble_led_handle.init(&green_ble_led_handle, PIN_BLE_GREEN_LED, LED_ON_AAA);
}

void led_hw_proc()
{
	blue_ble_led_handle.proc(&blue_ble_led_handle);
	red_ble_led_handle.proc(&red_ble_led_handle);
	green_ble_led_handle.proc(&green_ble_led_handle);
}

#endif
