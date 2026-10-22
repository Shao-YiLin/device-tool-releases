/*
 * RGB_ins1_manager.c
 *
 *  Created on: 2022-7-8
 *      Author: xdhuang
 */
//#include"RGB_base.h"
#include "PWM_platform_define.h"
#include "RGB_ins_manager.h"
#include "common/types.h"
#include "common/string.h"
static int_pwm_channel gpio_int_param1 = {{0, AS_PWM3, GPIO_PB0},
										{0, AS_PWM1, GPIO_PA3},
										{1, AS_PWM4_N, GPIO_PC6}};
static int_pwm_channel gpio_int_param2 = {{0, AS_PWM2, GPIO_PA4},
										{1, AS_PWM5_N, GPIO_PC7},
										{1, AS_PWM0_N, GPIO_PA0}};



static void _init(struct RGB_manager *mg)
{
	mg->rgb_handle.init(&mg->rgb_handle, mg->rgb_handle.rgb_param.gpio_param);
}

void _set_gradient_colour(struct RGB_manager *mg, u8 priority, u8 count, u16 on_time, u16 change_time, rgb_data *colour, u8 len)
{
	rgb_cfg cfg_current;
	memset(&cfg_current, 0, sizeof(rgb_cfg));
	cfg_current.gradual_flag = GRADIENT;
	cfg_current.colour_len = len;
	cfg_current.changeTime_ms = change_time;
	cfg_current.onTime_ms = on_time;
	//cfg_current.offTime_ms = cfg.changeTime_ms;
	cfg_current.priority = priority;
	cfg_current.repeatCount = count;
	for(int i = 0; i < len; i++)
	{
		cfg_current.on_colour[i].led_r = (s16)(colour[i].rgb_red);
		cfg_current.on_colour[i].led_g = (s16)(colour[i].rgb_green);
		cfg_current.on_colour[i].led_b = (s16)(colour[i].rgb_blue);
	}
	mg->rgb_handle.set(&mg->rgb_handle, cfg_current);
}

void _set_breathing_colour(struct RGB_manager *mg, u8 priority, u8 count, u16 on_time, u16 off_time, u16 change_time, rgb_data *colour, u8 len)
{
	rgb_cfg cfg_current;
	memset(&cfg_current, 0, sizeof(rgb_cfg));
	cfg_current.gradual_flag = BREATH;
	cfg_current.colour_len = len;
	cfg_current.onTime_ms = on_time;
	cfg_current.offTime_ms = off_time;
	cfg_current.changeTime_ms = change_time;
	cfg_current.priority = priority;
	cfg_current.repeatCount = count;
	for(int i = 0; i < len; i++)
	{
		cfg_current.on_colour[i].led_r = (s16)(colour[i].rgb_red);
		cfg_current.on_colour[i].led_g = (s16)(colour[i].rgb_green);
		cfg_current.on_colour[i].led_b = (s16)(colour[i].rgb_blue);
	}
	mg->rgb_handle.set(&mg->rgb_handle, cfg_current);
}

unsigned int _set_single_colour(struct RGB_manager *mg, single_cfg cfg)
{
	rgb_cfg cfg_current;
	memset(&cfg_current, 0, sizeof(rgb_cfg));
	cfg_current.gradual_flag = NO_GRADUAL;
	cfg_current.on_colour[0].led_r = (s16)cfg.colour_value.rgb_red;
	cfg_current.on_colour[0].led_g = (s16)cfg.colour_value.rgb_green;
	cfg_current.on_colour[0].led_b = (s16)cfg.colour_value.rgb_blue;
	cfg_current.onTime_ms = cfg.onTime_ms;
	cfg_current.offTime_ms = cfg.offTime_ms;
	cfg_current.priority = cfg.priority;
	cfg_current.repeatCount = cfg.repeatCount;
	return mg->rgb_handle.set(&mg->rgb_handle, cfg_current);
}

unsigned char _get_repeatCount(struct RGB_manager *mg)
{
	return mg->rgb_handle.rgb_param.rgb_cfg_info.repeatCount;
}

void _loop(struct RGB_manager *mg, unsigned char turnon_flag)
{
	mg->rgb_handle.proc(&mg->rgb_handle, turnon_flag);
}

void RGB_manager_construct(struct RGB_manager *mg)
{
	if(mg != NULL)
	{
		mg->init = _init;
		mg->set_breathing_colour = _set_breathing_colour;
		mg->set_gradient_colour = _set_gradient_colour;
		mg->set_single_colour = _set_single_colour;
		mg->loop = _loop;
		mg->get_repeatCount = _get_repeatCount;
		rgb_base_construct(&mg->rgb_handle);
	}
}

struct RGB_manager _rgb_manager_instance1;
struct RGB_manager _rgb_manager_instance2;
struct RGB_manager *rgb_manager_instance1 = NULL;
struct RGB_manager *rgb_manager_instance2 = NULL;

struct RGB_manager *rgb_front()
{
	if(rgb_manager_instance1 == NULL)
	{
		rgb_manager_instance1 = &_rgb_manager_instance1;
		RGB_manager_construct(rgb_manager_instance1);
		rgb_manager_instance1->rgb_handle.rgb_param.gpio_param = gpio_int_param1;
	}
	return rgb_manager_instance1;
}

struct RGB_manager *rgb_back()
{
	if(rgb_manager_instance2 == NULL)
	{
		rgb_manager_instance2 = &_rgb_manager_instance2;
		RGB_manager_construct(rgb_manager_instance2);
		rgb_manager_instance2->rgb_handle.rgb_param.gpio_param = gpio_int_param2;
	}
	return rgb_manager_instance2;
}
