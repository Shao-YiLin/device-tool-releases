#include "AAA_public_config.h"
#include "AAA_battery_check.h"
#if (BATT_CHECK_ENABLE)

extern u8 low_Batt_flag;
extern u8 ultralow_Batt_flag;
extern u8 first_lower_power_flag;

static battery_status_change_cb_fn battery_cb = NULL;
static int battery_check_time_interval_us = 5000000;
static u32 battery_detect_time_tick = 0; 
//static void _battery_check_action();

//static void charging_gpio_init()
//{
//#if (BATT_ENABLE_AAA)
//#endif
//}

void battery_manager_init(int check_interval_us, battery_status_change_cb_fn cb)
{
	//charging_gpio_init();
    battery_check_time_interval_us = check_interval_us;
    battery_cb = cb;
    //_battery_check_action();
}


bool battery_is_full() //false:not a full charge ; true:fully charge
{
    return gpio_read(GPIO_LI_CHARG) ? false : true;
}

bool battery_is_charging() //false:no charging ; true:be charging
{
	return gpio_read(GPIO_BATT_CHECK) ? true : false;
}


static u8 _battery_power_check()
{
   return app_adc_check(ADC_INPUT_PCHN);
}

static u8 _battery_check_action()
{
	u8 out_state =  _battery_power_check();
	if (!out_state)
		return 0;
    if (low_Batt_flag)
    {
    	if(ultralow_Batt_flag)  //电池电量极低
    	{
    		if (battery_cb)
    			battery_cb(2);
    	}
    	else
    	{
    		if (battery_cb)
    			battery_cb(1);
    	}
        if (first_lower_power_flag)
        {
            if (battery_cb)
                battery_cb(3);
            first_lower_power_flag = 0;
        }
    }
    else
    {
        if (battery_cb)
            battery_cb(0);
    }
    return 1;
}

void reset_battery_detect_time()
{
	battery_detect_time_tick = 0;
}

//extern int battery_get_detect_enable(void);

u8 battery_manager_check_proc()
{
	if(!ui_mic_enable && !charging_flag)
	{
		if (battery_detect_time_tick == 0 || clock_time_exceed(battery_detect_time_tick, battery_check_time_interval_us))
		{
			if(!battery_get_detect_enable())
				battery_set_detect_enable(1);
			if(_battery_check_action()){
				battery_detect_time_tick = clock_time() | 1;
				return 1;
			}
		}
	}
	return 0;
}
#endif
