/*
 * PWM_base.c
 *
 *  Created on: 2022-7-7
 *      Author: xdhuang
 */
#include "RGB_base.h"
#include "PWM_platform_define.h"
#include "common/types.h"
#include "common/string.h"
#define MAX_DUTY_CYCLE 0XFF

static void init_single_pwm(gpio_and_pwm_channel pwm_gpio_param)
{
	set_pwm_gpio(pwm_gpio_param.gpio_point, pwm_gpio_param.pwm_channel);
	if(pwm_gpio_param.polarity)
		overturn_pwm(pwm_gpio_param.pwm_channel);
}

static void init_pwm_gpio(int_pwm_channel gpio_param)
{
	set_pwm_clk();
	init_single_pwm(gpio_param.gpio_r);
	init_single_pwm(gpio_param.gpio_g);
	init_single_pwm(gpio_param.gpio_b);
}

static void set_single_rgb_value(s16 channel, s16 vlue)
{
	set_pwm_duty_cycle(channel, MAX_DUTY_CYCLE, vlue & 0xff);
	start_pwm(channel);
}

static void stop_single_rgb(s16 channel)
{
	set_pwm_duty_cycle(channel, MAX_DUTY_CYCLE, 0);
	stop_pwm(channel);
}

static void stop_rgb(int_pwm_channel gpio_param)
{
	stop_single_rgb(gpio_param.gpio_r.pwm_channel);
	stop_single_rgb(gpio_param.gpio_g.pwm_channel);
	stop_single_rgb(gpio_param.gpio_b.pwm_channel);
}

static void set_rgb_value(int_pwm_channel gpio_param, rgb_duty_cycle rgb_value, u8 turnon_flag)
{
	if(turnon_flag)
	{
		set_single_rgb_value(gpio_param.gpio_r.pwm_channel, rgb_value.led_r);
		set_single_rgb_value(gpio_param.gpio_g.pwm_channel, rgb_value.led_g);
		set_single_rgb_value(gpio_param.gpio_b.pwm_channel, rgb_value.led_b);
	}
	else
		stop_rgb(gpio_param);

}

static u8 verify_duty_cycle(s16 init_value, s16 tar_vlue, s16 cur_value)
{
	return (tar_vlue >= init_value) ? (cur_value >= tar_vlue) : (cur_value <= tar_vlue);
}

static u8 set_breathing_loop(rgb_base_param *rgb_param, u8 up_flag)
{
	u32 timer = current_time();
	u32 time_offset_ms_r = calculation_time_offset_ms(timer, rgb_param->startTick[0]);
	u32 time_offset_ms_g = calculation_time_offset_ms(timer, rgb_param->startTick[1]);
	u32 time_offset_ms_b = calculation_time_offset_ms(timer, rgb_param->startTick[2]);
	rgb_duty_cycle next_colour;
	rgb_duty_cycle current_colour;
	if(up_flag == 1)
	{
		next_colour = rgb_param->rgb_cfg_info.on_colour[rgb_param->colour_index];
		current_colour = rgb_param->rgb_cfg_info.on_colour[MAX_BREATH_COLOUR - 1];
	}
	else if(up_flag == 2)
	{
		current_colour = rgb_param->rgb_cfg_info.on_colour[rgb_param->colour_index];
		next_colour = rgb_param->rgb_cfg_info.on_colour[(rgb_param->colour_index + 1) % ((rgb_param->rgb_cfg_info.repeatCount > 1) ? rgb_param->rgb_cfg_info.colour_len : MAX_BREATH_COLOUR)];
	}
	else
	{
		current_colour = rgb_param->rgb_cfg_info.on_colour[rgb_param->colour_index];
		next_colour = rgb_param->rgb_cfg_info.on_colour[MAX_BREATH_COLOUR - 1];
	}
	if(rgb_param->rgb_cfg_info.changeTime_ms == 0)
	{
		set_rgb_value(rgb_param->gpio_param, next_colour, rgb_param->turnon_flag);
		return 1;
	}
	s32 offset_r = (s32)((next_colour.led_r - current_colour.led_r) * time_offset_ms_r) / rgb_param->rgb_cfg_info.changeTime_ms;
	s32 offset_g = (s32)((next_colour.led_g - current_colour.led_g) * time_offset_ms_g) / rgb_param->rgb_cfg_info.changeTime_ms;
	s32 offset_b = (s32)((next_colour.led_b - current_colour.led_b) * time_offset_ms_b) / rgb_param->rgb_cfg_info.changeTime_ms;
	if(offset_r != 0 || offset_g != 0 || offset_b != 0 || time_offset_ms_r >= rgb_param->rgb_cfg_info.changeTime_ms || time_offset_ms_g >= rgb_param->rgb_cfg_info.changeTime_ms || time_offset_ms_b >= rgb_param->rgb_cfg_info.changeTime_ms)
	{
		if(time_offset_ms_r >= rgb_param->rgb_cfg_info.changeTime_ms || offset_r != 0)
		{
			rgb_param->startTick[0] += time_offset_ms_r * CLOCK_16M_SYS_TIMER_CLK_1MS;
			rgb_param->current_colour.led_r += offset_r;
			if(verify_duty_cycle(current_colour.led_r, next_colour.led_r, rgb_param->current_colour.led_r))
			{
				rgb_param->current_colour.led_r = next_colour.led_r;
			}
		}
		if(time_offset_ms_g >= rgb_param->rgb_cfg_info.changeTime_ms || offset_g != 0)
		{
			rgb_param->current_colour.led_g += offset_g;
			rgb_param->startTick[1] += time_offset_ms_g * CLOCK_16M_SYS_TIMER_CLK_1MS;
			if(verify_duty_cycle(current_colour.led_g, next_colour.led_g, rgb_param->current_colour.led_g))
			{
				rgb_param->current_colour.led_g = next_colour.led_g;
			}
		}
		if(time_offset_ms_b >= rgb_param->rgb_cfg_info.changeTime_ms || offset_b != 0)
		{
			rgb_param->current_colour.led_b += offset_b;
			rgb_param->startTick[2] += time_offset_ms_b * CLOCK_16M_SYS_TIMER_CLK_1MS;
			if(verify_duty_cycle(current_colour.led_b, next_colour.led_b, rgb_param->current_colour.led_b))
			{
				rgb_param->current_colour.led_b = next_colour.led_b;
			}
		}
		if((rgb_param->current_colour.led_r == next_colour.led_r) && (rgb_param->current_colour.led_g == next_colour.led_g) && (rgb_param->current_colour.led_b == next_colour.led_b))
		{
			rgb_param->startTick[3] += rgb_param->rgb_cfg_info.changeTime_ms * CLOCK_16M_SYS_TIMER_CLK_1MS;
			rgb_param->startTick[0] = rgb_param->startTick[3];
			rgb_param->startTick[1] = rgb_param->startTick[3];
			rgb_param->startTick[2] = rgb_param->startTick[3];
			set_rgb_value(rgb_param->gpio_param, rgb_param->current_colour, rgb_param->turnon_flag);
			return 1;
		}
		set_rgb_value(rgb_param->gpio_param, rgb_param->current_colour, rgb_param->turnon_flag);
	}
	return 0;
}

static void _init(struct rgb_base_class *rgb_handle, int_pwm_channel gpio_param)
{
	rgb_handle->rgb_param.gpio_param = gpio_param;
	init_pwm_gpio(gpio_param);
	stop_rgb(gpio_param);
}

static unsigned int _set(struct rgb_base_class *rgb_handle, rgb_cfg cfg)
{
	if(rgb_handle->rgb_param.rgb_cfg_info.repeatCount && (rgb_handle->rgb_param.rgb_cfg_info.priority >= cfg.priority))
		return 0;
	cfg.colour_len &= (MAX_BREATH_COLOUR - 1);
	for(unsigned char i = 0; i < cfg.colour_len; i ++)
	{
		cfg.on_colour[i].led_r &= 0xff;
		cfg.on_colour[i].led_g &= 0xff;
		cfg.on_colour[i].led_b &= 0xff;
	}
	rgb_handle->rgb_param.rgb_cfg_info = cfg;
	rgb_handle->rgb_param.colour_index = 0;
	memcpy((u8 *)&(rgb_handle->rgb_param.current_colour), (u8 *)&(rgb_handle->rgb_param.rgb_cfg_info.on_colour[0]), sizeof(rgb_duty_cycle));
	if(rgb_handle->rgb_param.rgb_cfg_info.gradual_flag == GRADIENT || rgb_handle->rgb_param.rgb_cfg_info.gradual_flag == BREATH)
	{
		rgb_handle->rgb_param.rgb_cfg_info.onTime_ms |= 1;
		set_rgb_value(rgb_handle->rgb_param.gpio_param, rgb_handle->rgb_param.current_colour, rgb_handle->rgb_param.turnon_flag);
	}
	else
	{
		if(rgb_handle->rgb_param.rgb_cfg_info.repeatCount != 0 && rgb_handle->rgb_param.rgb_cfg_info.repeatCount != 0xff)
		{
			if(!rgb_handle->rgb_param.rgb_cfg_info.onTime_ms)
			{
				rgb_handle->rgb_param.rgb_cfg_info.repeatCount = 1;
				rgb_handle->rgb_param.rgb_cfg_info.offTime_ms *= rgb_handle->rgb_param.rgb_cfg_info.repeatCount;
			}
			else if(!rgb_handle->rgb_param.rgb_cfg_info.offTime_ms)
			{
				rgb_handle->rgb_param.rgb_cfg_info.repeatCount = 1;
				rgb_handle->rgb_param.rgb_cfg_info.onTime_ms *= rgb_handle->rgb_param.rgb_cfg_info.repeatCount;
			}
		}
		if(rgb_handle->rgb_param.rgb_cfg_info.onTime_ms)
		{
			//memcpy((u8 *)&(rgb_handle->rgb_param.current_colour), (u8 *)(rgb_handle->rgb_param.rgb_cfg_info.on_colour), sizeof(rgb_duty_cycle));
			set_rgb_value(rgb_handle->rgb_param.gpio_param, rgb_handle->rgb_param.rgb_cfg_info.on_colour[0], rgb_handle->rgb_param.turnon_flag);
		}
		else
		{
			//memset((u8 *)&(rgb_handle->rgb_param.current_colour), 0, sizeof(rgb_duty_cycle));
			stop_rgb(rgb_handle->rgb_param.gpio_param);
		}
	}
	rgb_handle->rgb_param.startTick[0] = current_time();
	rgb_handle->rgb_param.startTick[1] = rgb_handle->rgb_param.startTick[0];
	rgb_handle->rgb_param.startTick[2] = rgb_handle->rgb_param.startTick[1];
	rgb_handle->rgb_param.startTick[3] = rgb_handle->rgb_param.startTick[2];
	rgb_handle->rgb_param.isOn = rgb_handle->rgb_param.rgb_cfg_info.onTime_ms ? STATE_ON : STATE_OFF;
	return 1;
}

static void _proc(struct rgb_base_class *rgb_handle, unsigned char turnon_flag)
{
	if(rgb_handle->rgb_param.turnon_flag != turnon_flag)
	{
		rgb_handle->rgb_param.turnon_flag = turnon_flag;
		set_rgb_value(rgb_handle->rgb_param.gpio_param, rgb_handle->rgb_param.current_colour, rgb_handle->rgb_param.turnon_flag);
	}
	if(rgb_handle->rgb_param.rgb_cfg_info.repeatCount)
	{
		if(rgb_handle->rgb_param.isOn == STATE_ON)
		{
			if(current_time_exceed(rgb_handle->rgb_param.startTick[0], rgb_handle->rgb_param.rgb_cfg_info.onTime_ms * 1000))
			{
				rgb_handle->rgb_param.startTick[0] += rgb_handle->rgb_param.rgb_cfg_info.onTime_ms * CLOCK_16M_SYS_TIMER_CLK_1MS;
				rgb_handle->rgb_param.startTick[1] = rgb_handle->rgb_param.startTick[0];
				rgb_handle->rgb_param.startTick[2] = rgb_handle->rgb_param.startTick[0];
				rgb_handle->rgb_param.startTick[3] = rgb_handle->rgb_param.startTick[0];
				if(rgb_handle->rgb_param.rgb_cfg_info.gradual_flag == GRADIENT)
					rgb_handle->rgb_param.isOn = STATE_SWITCH;
				else
				{
					if(rgb_handle->rgb_param.rgb_cfg_info.changeTime_ms != 0 ||rgb_handle->rgb_param.rgb_cfg_info.offTime_ms != 0 || rgb_handle->rgb_param.rgb_cfg_info.repeatCount != 0xff)
						rgb_handle->rgb_param.isOn = STATE_DOWN;
				}
			}
		}
		else if(rgb_handle->rgb_param.isOn == STATE_DOWN)
		{
			if(set_breathing_loop(&rgb_handle->rgb_param, 0))
			{
				rgb_handle->rgb_param.isOn = STATE_OFF;
				rgb_handle->rgb_param.colour_index ++;
			}
		}
		else if(rgb_handle->rgb_param.isOn == STATE_UP)
		{
			if(rgb_handle->rgb_param.colour_index >= rgb_handle->rgb_param.rgb_cfg_info.colour_len)
			{
				rgb_handle->rgb_param.colour_index = 0;
				if(rgb_handle->rgb_param.rgb_cfg_info.repeatCount != 0xff)
				{
					rgb_handle->rgb_param.rgb_cfg_info.repeatCount --;
				}
			}
			if(rgb_handle->rgb_param.rgb_cfg_info.repeatCount)
			{
				if(set_breathing_loop(&rgb_handle->rgb_param, 1))
					rgb_handle->rgb_param.isOn = STATE_ON;
			}
			else
			{
				rgb_handle->rgb_param.isOn = STATE_OFF;
				stop_rgb(rgb_handle->rgb_param.gpio_param);
			}
		}
		else if(rgb_handle->rgb_param.isOn == STATE_SWITCH)
		{
			if(set_breathing_loop(&rgb_handle->rgb_param, 2))
			{
				if((++rgb_handle->rgb_param.colour_index) >= rgb_handle->rgb_param.rgb_cfg_info.colour_len)
				{
					rgb_handle->rgb_param.colour_index = 0;
					if(rgb_handle->rgb_param.rgb_cfg_info.repeatCount != 0xff)
					{
						rgb_handle->rgb_param.rgb_cfg_info.repeatCount --;
					}
				}
				if(rgb_handle->rgb_param.rgb_cfg_info.repeatCount)
				{
					rgb_handle->rgb_param.isOn = STATE_ON;
				}
				else
				{
					rgb_handle->rgb_param.isOn = STATE_OFF;
					stop_rgb(rgb_handle->rgb_param.gpio_param);
				}
			}
		}
		else if(current_time_exceed(rgb_handle->rgb_param.startTick[0], rgb_handle->rgb_param.rgb_cfg_info.offTime_ms * 1000))
		{
			rgb_handle->rgb_param.startTick[0] += rgb_handle->rgb_param.rgb_cfg_info.offTime_ms * CLOCK_16M_SYS_TIMER_CLK_1MS;
			rgb_handle->rgb_param.startTick[1] = rgb_handle->rgb_param.startTick[0];
			rgb_handle->rgb_param.startTick[2] = rgb_handle->rgb_param.startTick[0];
			rgb_handle->rgb_param.startTick[2] = rgb_handle->rgb_param.startTick[0];
			if(rgb_handle->rgb_param.rgb_cfg_info.repeatCount && (rgb_handle->rgb_param.rgb_cfg_info.onTime_ms || rgb_handle->rgb_param.rgb_cfg_info.changeTime_ms))
			{
				//memcpy((u8 *)&(rgb_handle->rgb_param.current_colour), (u8 *)&(rgb_handle->rgb_param.rgb_cfg_info.on_colour[0]), sizeof(rgb_duty_cycle));
				rgb_handle->rgb_param.isOn = STATE_UP;
			}
			else
			{
				rgb_handle->rgb_param.rgb_cfg_info.repeatCount = 0;
				stop_rgb(rgb_handle->rgb_param.gpio_param);
			}
		}
	}
}

void rgb_base_construct(struct rgb_base_class *lm)
{
	if(lm)
	{
		lm->init = _init;
		lm->set = _set;
		lm->proc = _proc;
	}
}
