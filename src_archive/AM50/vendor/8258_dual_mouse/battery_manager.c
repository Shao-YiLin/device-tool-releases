#include "battery_manager.h"
#include "AAA_public_config.h"
#include "AAA_battery_check.h"
#if (BATT_CHECK_ENABLE)

extern u8 low_Batt_flag;
extern u8 ultra_low_Batt_flag;
extern u8 first_lower_power_flag;

static battery_status_change_cb_fn battery_cb = NULL;
static int battery_check_time_interval_us = 5000000;
static u32 battery_detect_time_tick = 0;
static void _battery_check_action();

//static void charging_gpio_init()
//{
//    gpio_set_output_en(GPIO_LI_CHARG, 0);
//    gpio_set_input_en(GPIO_LI_CHARG, 1);
//    gpio_setup_up_down_resistor(GPIO_LI_CHARG, PM_PIN_PULLUP_1M);
//    gpio_set_output_en(GPIO_BATT_CHECK, 0);
//    gpio_set_input_en(GPIO_BATT_CHECK, 1);
//    gpio_setup_up_down_resistor(GPIO_BATT_CHECK, PM_PIN_PULLUP_1M);
//}
void battery_manager_init(int check_interval_us, battery_status_change_cb_fn cb)
{
//    charging_gpio_init();
    battery_check_time_interval_us = check_interval_us;
    battery_cb = cb;
//    _battery_check_action();
//    battery_detect_time_tick = clock_time();
}

bool battery_is_full() //false:not a full charge ; true:fully charge
{
     return gpio_read(GPIO_LI_CHARG) ? false : true;
}

bool battery_is_charging() //false:no charging ; true:be charging
{
	return gpio_read(GPIO_BATT_CHECK) ? true : false;
}

static void _battery_power_check()
{
    app_adc_check(ADC_INPUT_PCHN);
}

static void _battery_check_action()
{
    _battery_power_check();
    if (ultra_low_Batt_flag) ////////////ultra_low_pow
    {
        if (battery_cb)
            battery_cb(2);
    }
    else if (low_Batt_flag)
    {
        if (battery_cb) /////////////////low_pow
            battery_cb(1);
        if (first_lower_power_flag) /////send batt to app
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
}

void reset_battery_detect_time()
{
	battery_detect_time_tick = 0;
}

void battery_manager_check_proc()
{
    if (battery_detect_time_tick == 0 || clock_time_exceed(battery_detect_time_tick, battery_check_time_interval_us))
    {
        battery_detect_time_tick = clock_time();
        _battery_check_action();
    }
}
#endif
