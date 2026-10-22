
#ifndef SENSOR_PLATFORM_DEFINE_H_
#define SENSOR_PLATFORM_DEFINE_H_

#include "drivers/8258/gpio.h"
#include "drivers/8258/pm.h"
#include "common/types.h"
#include "drivers/8258/irq.h"
#include "drivers/8258/clock.h"
#include "drivers/8258/timer.h"
#if SENSOR_FUN_ENABLE_AAA

#define int8_t s8
#define uint8_t u8
#define uint16_t u16
#define int16_t	s16
#define uint32_t u32

#define HIGH_MOUSE_RESOLUTION_ENABLE	0

#define SET_SDIO_OUTPUT_ENABLE() gpio_set_output_en(PIN_SIF_SDA, 1)
#define SET_SDIO_OUTPUT_DISABLE() gpio_set_output_en(PIN_SIF_SDA, 0)
#define SET_SDIO_INPUT_ENABLE() gpio_set_input_en(PIN_SIF_SDA, 1)
#define SET_SDIO_INPUT_DISABLE() gpio_set_input_en(PIN_SIF_SDA, 0)
#if SENSOR_CS_ENABLE
#define	SET_CS_HIGH()		gpio_write(PIN_SENSOR_CS, 1)	/*  */
#define	SET_CS_LOW()	gpio_write(PIN_SENSOR_CS, 0)		/*  */
#else
#define	SET_CS_HIGH()
#define	SET_CS_LOW()
#endif
#define SET_CLK_LOW() gpio_write(PIN_SIF_SCL, 0) /*  */
#define SET_CLK_HIGH() gpio_write(PIN_SIF_SCL, 1) /*  */
#define CS_DELAY() CLOCK_DLY_6_CYC																		 /*  */
#define CLOCK_DELAY() CLOCK_DLY_6_CYC
#define SLEEP_DELAY(time) sleep_us(time)																 /*  */
#define read_sdio_bit() gpio_read(PIN_SIF_SDA)				 /*  */
#define write_sdio_bit(bit) gpio_write(PIN_SIF_SDA, bit)
#define set_sensor_wakeup(level, enable) cpu_set_gpio_wakeup(PIN_SIF_MOTION, level, enable)

#define os_enter_lock() irq_disable()
#define os_leave_lock(r) irq_restore(r)

#endif 
#endif

