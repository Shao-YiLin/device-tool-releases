
#include "led_manager.h"
#include "tl_common.h"
#include "drivers.h"

static void _device_led_on_off(struct led_manager *lm, u8 on)
{
    if (!lm)
        return;

    gpio_write(lm->led_param.gpio_led, on ^ lm->led_param.polar);
 //   gpio_set_output_en(lm->led_param.gpio_led, on);
    lm->led_param.isOn = on;
}

static void _init(struct led_manager *lm, u32 gpio, u8 polarity)
{
	lm->led_param.gpio_led = gpio;
	lm->led_param.polar = !polarity;
	gpio_set_func(lm->led_param.gpio_led, AS_GPIO);
	gpio_set_input_en(lm->led_param.gpio_led, 0);
	gpio_set_output_en(lm->led_param.gpio_led, 1);
	_device_led_on_off(lm, 0);
  //  gpio_write(lm->led_param.gpio_led, lm->led_param.polar);
}

static int _set(struct led_manager *lm, led_cfg cfg)
{
    if (lm->led_param.repeatCount && lm->led_param.priority >= cfg.priority)
    {
        return 0; //new led event priority not higher than the not ongoing one
    }
    else
    {
        lm->led_param.onTime_ms = cfg.onTime_ms;
        lm->led_param.offTime_ms = cfg.offTime_ms;
        lm->led_param.repeatCount = cfg.repeatCount;
        lm->led_param.priority = cfg.priority;
        if (cfg.repeatCount == 0xff)
        { //for long on/long off
            lm->led_param.repeatCount = 0;
        }
        else
        { //process one of on/off Time is zero situation
            if (!lm->led_param.onTime_ms)
            { //onTime is zero
                lm->led_param.offTime_ms *= lm->led_param.repeatCount;
                lm->led_param.repeatCount = 1;
            }
            else if (!lm->led_param.offTime_ms)
            {
                lm->led_param.onTime_ms *= lm->led_param.repeatCount;
                lm->led_param.repeatCount = 1;
            }
        }
        lm->led_param.startTick = clock_time();
        _device_led_on_off(lm, lm->led_param.onTime_ms ? 1 : 0);
        return 1;
    }
}

static int _proc(struct led_manager *lm)
{
    if(lm->led_param.repeatCount){
    	// printf("%d, %d \n", lm->led_param.repeatCount, lm->led_param.gpio_led);
    	    if (lm->led_param.isOn)
    	    {
    	        if (clock_time_exceed(lm->led_param.startTick, lm->led_param.onTime_ms * 1000))
    	        {
    	            _device_led_on_off(lm, 0);
    	            if (lm->led_param.offTime_ms)
    	            { //offTime not zero
    	                lm->led_param.startTick += lm->led_param.onTime_ms * CLOCK_16M_SYS_TIMER_CLK_1MS;
    	            }
    	            else
    	            {
    	                lm->led_param.repeatCount = 0;
    	            }
    	        }
    	    }
    	    else
    	    {
    	        if (clock_time_exceed(lm->led_param.startTick, lm->led_param.offTime_ms * 1000))
    	        {
    	            if (--lm->led_param.repeatCount)
    	            {
    	                _device_led_on_off(lm, 1);
    	                lm->led_param.startTick += lm->led_param.offTime_ms * CLOCK_16M_SYS_TIMER_CLK_1MS;
    	            }
    	        }
    	    }
    }
return 1;
}

void led_manager_construct(struct led_manager *lm)
{
    if (lm)
    {
        lm->init = _init;
        lm->set = _set;
        lm->proc = _proc;
    }
}

