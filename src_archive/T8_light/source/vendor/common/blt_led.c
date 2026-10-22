/********************************************************************************************************
 * @file     blt_led.c 
 *
 * @brief    for TLSR chips
 *
 * @author	 public@telink-semi.com;
 * @date     May. 12, 2018
 *
 * @par      Copyright (c) Telink Semiconductor (Shanghai) Co., Ltd.
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
/*
 * blt_led.c
 *
 *  Created on: 2016-1-29
 *      Author: Administrator
 */
#include "tl_common.h"
#include "drivers.h"
#include "../common/blt_led.h"



device_led_t device_led;
device_led_t1 device_led1;
device_led_t2 device_led2;


void device_led_on_off(u8 on)
{
	gpio_write( device_led.gpio_led, on^device_led.polar );
	gpio_set_output_en(device_led.gpio_led, on);
	device_led.isOn = on;
}
void device_led_on_off1(u8 on)
{
	gpio_write( device_led1.gpio_led1, on^device_led1.polar1 );
	gpio_set_output_en(device_led1.gpio_led1, on);
	device_led1.isOn1 = on;
}
void device_led_on_off2(u8 on)
{
	gpio_write( device_led2.gpio_led2, on^device_led2.polar2 );
	gpio_set_output_en(device_led2.gpio_led2, on);
	device_led2.isOn2 = on;
}

void device_led_init(u32 gpio,u8 polarity){  //polarity: 1 for high led on, 0 for low led on
#if 0
#if (BLT_APP_LED_ENABLE)
	device_led.gpio_led = gpio;
	device_led.polar = !polarity;
	gpio_write( gpio, !polarity );
#endif
#else
	device_led.gpio_led = gpio;
	device_led.polar = !polarity;
    gpio_set_func(device_led.gpio_led,AS_GPIO);
    gpio_set_input_en(device_led.gpio_led,0);
    gpio_set_output_en(device_led.gpio_led,0);

    device_led_on_off(0);
#endif
}

void device_led_init1(u32 gpio,u8 polarity){  //polarity: 1 for high led on, 0 for low led on
#if 0
#if (BLT_APP_LED_ENABLE)
	device_led.gpio_led = gpio;
	device_led.polar = !polarity;
	gpio_write( gpio, !polarity );
#endif
#else
	device_led1.gpio_led1 = gpio;
	device_led1.polar1 = !polarity;
    gpio_set_func(device_led1.gpio_led1,AS_GPIO);
    gpio_set_input_en(device_led1.gpio_led1,0);
    gpio_set_output_en(device_led1.gpio_led1,0);

    device_led_on_off1(0);
#endif
}

void device_led_init2(u32 gpio,u8 polarity){  //polarity: 1 for high led on, 0 for low led on
#if 0
#if (BLT_APP_LED_ENABLE)
	device_led.gpio_led = gpio;
	device_led.polar = !polarity;
	gpio_write( gpio, !polarity );
#endif
#else
	device_led2.gpio_led2 = gpio;
	device_led2.polar2 = !polarity;
    gpio_set_func(device_led2.gpio_led2,AS_GPIO);
    gpio_set_input_en(device_led2.gpio_led2,0);
    gpio_set_output_en(device_led2.gpio_led2,0);

    device_led_on_off2(0);
#endif
}

int device_led_setup(led_cfg_t led_cfg)
{
#if (BLT_APP_LED_ENABLE)
	if( device_led.repeatCount &&  device_led.priority >= led_cfg.priority){
		return 0; //new led event priority not higher than the not ongoing one
	}
	else{
		device_led.onTime_ms = led_cfg.onTime_ms;
		device_led.offTime_ms = led_cfg.offTime_ms;
		device_led.repeatCount = led_cfg.repeatCount;
		device_led.priority = led_cfg.priority;

        if(led_cfg.repeatCount == 0xff){ //for long on/long off
        	device_led.repeatCount = 0;
        }
        else{ //process one of on/off Time is zero situation
        	if(!device_led.onTime_ms){  //onTime is zero
        		device_led.offTime_ms *= device_led.repeatCount;
        		device_led.repeatCount = 1;
        	}
        	else if(!device_led.offTime_ms){
        		device_led.onTime_ms *= device_led.repeatCount;
        	    device_led.repeatCount = 1;
        	}
        }

        device_led.startTick = clock_time();
        device_led_on_off(device_led.onTime_ms ? 1 : 0);

		return 1;
	}
#endif
}

int device_led_setup1(led_cfg_t1 led_cfg)
{
#if (BLT_APP_LED_ENABLE)
	if( device_led1.repeatCount1 &&  device_led1.priority1 >= led_cfg.priority1){
		return 0; //new led event priority not higher than the not ongoing one
	}
	else{
		device_led1.onTime_ms1 = led_cfg.onTime_ms1;
		device_led1.offTime_ms1 = led_cfg.offTime_ms1;
		device_led1.repeatCount1 = led_cfg.repeatCount1;
		device_led1.priority1 = led_cfg.priority1;

        if(led_cfg.repeatCount1 == 0xff){ //for long on/long off
        	device_led1.repeatCount1 = 0;
        }
        else{ //process one of on/off Time is zero situation
        	if(!device_led1.onTime_ms1){  //onTime is zero
        		device_led1.offTime_ms1 *= device_led1.repeatCount1;
        		device_led1.repeatCount1 = 1;
        	}
        	else if(!device_led1.offTime_ms1){
        		device_led1.onTime_ms1 *= device_led1.repeatCount1;
        	    device_led1.repeatCount1 = 1;
        	}
        }

        device_led1.startTick1 = clock_time();
        device_led_on_off1(device_led1.onTime_ms1 ? 1 : 0);

		return 1;
	}
#endif
}

int device_led_setup2(led_cfg_t2 led_cfg)
{
#if (BLT_APP_LED_ENABLE)
	if( device_led2.repeatCount2 &&  device_led2.priority2 >= led_cfg.priority2){
		return 0; //new led event priority not higher than the not ongoing one
	}
	else{
		device_led2.onTime_ms2 = led_cfg.onTime_ms2;
		device_led2.offTime_ms2 = led_cfg.offTime_ms2;
		device_led2.repeatCount2 = led_cfg.repeatCount2;
		device_led2.priority2 = led_cfg.priority2;

        if(led_cfg.repeatCount2 == 0xff){ //for long on/long off
        	device_led2.repeatCount2 = 0;
        }
        else{ //process one of on/off Time is zero situation
        	if(!device_led2.onTime_ms2){  //onTime is zero
        		device_led2.offTime_ms2 *= device_led2.repeatCount2;
        		device_led2.repeatCount2 = 1;
        	}
        	else if(!device_led2.offTime_ms2){
        		device_led2.onTime_ms2 *= device_led2.repeatCount2;
        	    device_led2.repeatCount2 = 1;
        	}
        }

        device_led2.startTick2 = clock_time();
        device_led_on_off2(device_led2.onTime_ms2 ? 1 : 0);

		return 1;
	}
#endif
}

void led_proc(void)
{
#if (BLT_APP_LED_ENABLE)
	if(device_led.isOn){
		if(clock_time_exceed(device_led.startTick,device_led.onTime_ms*1000)){
			device_led_on_off(0);
			if(device_led.offTime_ms){ //offTime not zero
				device_led.startTick += device_led.onTime_ms*CLOCK_16M_SYS_TIMER_CLK_1MS;
			}
			else{
				device_led.repeatCount = 0;
			}
		}
	}
	else{
		if(clock_time_exceed(device_led.startTick,device_led.offTime_ms*1000)){
			if(--device_led.repeatCount){
				device_led_on_off(1);
				device_led.startTick += device_led.offTime_ms*CLOCK_16M_SYS_TIMER_CLK_1MS;
			}
		}
	}
#endif
}

void led_proc1(void)
{
#if (BLT_APP_LED_ENABLE)
	if(device_led1.isOn1){
		if(clock_time_exceed(device_led1.startTick1,device_led1.onTime_ms1*1000)){
			device_led_on_off1(0);
			if(device_led1.offTime_ms1){ //offTime not zero
				device_led1.startTick1 += device_led1.onTime_ms1*CLOCK_16M_SYS_TIMER_CLK_1MS;
			}
			else{
				device_led1.repeatCount1 = 0;
			}
		}
	}
	else{
		if(clock_time_exceed(device_led1.startTick1,device_led1.offTime_ms1*1000)){
			if(--device_led1.repeatCount1){
				device_led_on_off1(1);
				device_led1.startTick1 += device_led1.offTime_ms1*CLOCK_16M_SYS_TIMER_CLK_1MS;
			}
		}
	}
#endif
}

void led_proc2(void)
{
#if (BLT_APP_LED_ENABLE)
	if(device_led2.isOn2){
		if(clock_time_exceed(device_led2.startTick2,device_led2.onTime_ms2*1000)){
			device_led_on_off2(0);
			if(device_led2.offTime_ms2){ //offTime not zero
				device_led2.startTick2 += device_led2.onTime_ms2*CLOCK_16M_SYS_TIMER_CLK_1MS;
			}
			else{
				device_led2.repeatCount2 = 0;
			}
		}
	}
	else{
		if(clock_time_exceed(device_led2.startTick2,device_led2.offTime_ms2*1000)){
			if(--device_led2.repeatCount2){
				device_led_on_off2(1);
				device_led2.startTick2 += device_led2.offTime_ms2*CLOCK_16M_SYS_TIMER_CLK_1MS;
			}
		}
	}
#endif
}

