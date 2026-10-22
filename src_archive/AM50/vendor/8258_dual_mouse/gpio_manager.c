
#include "gpio_manager.h"
#include "tl_common.h"
#include "drivers.h"

static void _device_gpio_on_off(struct gpio_manager *lm, u8 on)
{
    if (!lm)
        return;

    gpio_write(lm->gpio_param.gpio_port, on ^ lm->gpio_param.polar);
 //   gpio_set_output_en(lm->gpio_param.gpio_port, on);
    lm->gpio_param.isOn = on;
}

static void _init(struct gpio_manager *lm, u32 gpio, u8 polarity)
{
	lm->gpio_param.gpio_port = gpio;
	lm->gpio_param.polar = !polarity;
	gpio_set_func(lm->gpio_param.gpio_port, AS_GPIO);
	gpio_set_input_en(lm->gpio_param.gpio_port, 0);
	gpio_set_output_en(lm->gpio_param.gpio_port, 1);
	_device_gpio_on_off(lm, 0);
  //  gpio_write(lm->gpio_param.gpio_port, lm->gpio_param.polar);
}

static int _set(struct gpio_manager *lm, gpio_cfg cfg)
{
    if (lm->gpio_param.repeatCount && lm->gpio_param.priority >= cfg.priority)
    {
        return 0; //new led event priority not higher than the not ongoing one
    }
    else
    {
        lm->gpio_param.onTime_ms = cfg.onTime_ms;
        lm->gpio_param.offTime_ms = cfg.offTime_ms;
        lm->gpio_param.repeatCount = cfg.repeatCount;
        lm->gpio_param.priority = cfg.priority;
        if (cfg.repeatCount == 0xff)
        { //for long on/long off
            lm->gpio_param.repeatCount = 0;
        }
        else
        { //process one of on/off Time is zero situation
            if (!lm->gpio_param.onTime_ms)
            { //onTime is zero
                lm->gpio_param.offTime_ms *= lm->gpio_param.repeatCount;
                lm->gpio_param.repeatCount = 1;
            }
            else if (!lm->gpio_param.offTime_ms)
            {
                lm->gpio_param.onTime_ms *= lm->gpio_param.repeatCount;
                lm->gpio_param.repeatCount = 1;
            }
        }
        lm->gpio_param.startTick = clock_time();
        _device_gpio_on_off(lm, lm->gpio_param.onTime_ms ? 1 : 0);
        return 1;
    }
}

static int _proc(struct gpio_manager *lm)
{

    if(lm->gpio_param.repeatCount){
    	// printf("%d, %d \n", lm->gpio_param.repeatCount, lm->gpio_param.gpio_port);
    	    if (lm->gpio_param.isOn)
    	    {
    	        if (clock_time_exceed(lm->gpio_param.startTick, lm->gpio_param.onTime_ms * 1000))
    	        {
    	        	_device_gpio_on_off(lm, 0);
    	            if (lm->gpio_param.offTime_ms)
    	            { //offTime not zero
    	                lm->gpio_param.startTick += lm->gpio_param.onTime_ms * CLOCK_16M_SYS_TIMER_CLK_1MS;
    	            }
    	            else
    	            {
    	                lm->gpio_param.repeatCount = 0;
    	            }
    	        }
    	    }
    	    else
    	    {
    	        if (clock_time_exceed(lm->gpio_param.startTick, lm->gpio_param.offTime_ms * 1000))
    	        {
    	            if (--lm->gpio_param.repeatCount)
    	            {
    	            	_device_gpio_on_off(lm, 1);
    	                lm->gpio_param.startTick += lm->gpio_param.offTime_ms * CLOCK_16M_SYS_TIMER_CLK_1MS;
    	            }
    	        }
    	    }
    }
return 1;
}

void gpio_manager_construct(struct gpio_manager *lm)
{
    if (lm)
    {
        lm->init = _init;
        lm->set = _set;
        lm->proc = _proc;
    }
}
