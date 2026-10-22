#include "drivers/8258/timer.h"
#include "drivers/8258/gpio_8258.h"
#include "drivers/8258/pwm.h"
#define channel_change(channel) ((channel - 20) % 6)
#define set_pwm_clk()	pwm_set_clk(CLOCK_SYS_CLOCK_HZ, CLOCK_SYS_CLOCK_HZ)
#define set_pwm_mode(channel)	pwm_set_mode(channel_change(channel), PWM_NORMAL_MODE)
#define set_pwm_gpio(gpio, channel) gpio_set_func(gpio, channel);pwm_set_mode(channel_change(channel), PWM_NORMAL_MODE);
#define overturn_pwm(channel) channel > AS_PWM5 ? pwm_n_revert(channel_change(channel)) : pwm_revert(channel_change(channel))
#define set_pwm_duty_cycle(channel, cycle_tick, duty_cycle) pwm_set_cycle_and_duty(channel_change(channel), cycle_tick, duty_cycle)
#define start_pwm(channel) pwm_start(channel_change(channel))
#define stop_pwm(channel) pwm_stop(channel_change(channel))

static inline unsigned long current_time()
{
	return clock_time() | 1;
}
static inline unsigned int current_time_exceed(unsigned int ref, unsigned int us)
{
	return ((unsigned int)(clock_time() - ref) > us * 16);
}

static inline unsigned int calculation_time_offset_ms(unsigned long current_time, unsigned int ref)
{
	return (current_time - ref)/CLOCK_16M_SYS_TIMER_CLK_1MS;
}
