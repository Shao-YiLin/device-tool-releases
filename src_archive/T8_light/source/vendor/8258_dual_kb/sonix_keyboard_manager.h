/*
 * uart_dma_manager.h
 *
 *  Created on: 2022-11-14
 *      Author: ylshao
 */

#ifndef _SONIX_KEYBOARD_MANAGER_H_
#define _SONIX_KEYBOARD_MANAGER_H_

#include "drivers/8258/spi.h"
#include "keyboard_scan.h"


#define SONIX_RGB_INFO_ADDR (0x73000)// size 4096 byte
#define MAX_RGB_SIZE 1024
#define RGB_MODE_NUM 17
#define DEFAULT_MODE 6
#define DEFAULT_SPEED 1
#define DEFAULT_BRIGHTNESS 2

typedef enum{
	STANDARD_EKYCODE = 0xA1,
	NKRO_KEYCODE,
	MEDIA_KEYCODE,
	SYS_CONTROL_KEYCODE,
	FN_KEYSTATE
}sonix_keycode;

typedef enum{
	SET_LEDS_MODE = 0xB1,
	SET_LEDS_SPEED,
	SET_LEDS_BRIGHTNESS,
	SET_MCU_MODE,
	SET_SINGLE_LED,
	DATA_RESPONSE = 0xB6// 查询数据相应
}sonix_set_reg;

typedef enum{
	GET_LEDS_MODE = 0xC1,
	GET_LEDS_SPEED,
	GET_LEDS_BRIGHTNESS,
	GET_SONIX_VERSION,
	GET_SINGLE_LEDS
}sonix_get_reg;

typedef enum{
	SET_FAILURE = 0x00,
	MCU_SLEEP = 0x01,
	MCU_BOOTLOADER = 0x02,
	MCU_BLE = 0x03,
	MCU_USB = 0x04,
}mcu_mode;

// 8 byte
typedef struct{
	u8 mode;
	u8 speed;
	u8 brightness;
	u8 valid_flag;
}rgb_param;

typedef struct {
	rgb_param kleds;
	u8 sonix_version[2];
	u8 sonix_sled_state[2];
	void (*init)(); //In the initial state, it is working by default
	void (*sonix_data_scan)();
	u8 (*set_mcu_mode)(mcu_mode mode);
	/**
	 * @brief set led parameter include of mode(0~24) , speed(0~3) and brightness(0~3)
	 *		  设置LED的参数，包括模式（0~24），速度（0~3），亮度（0~3）
	 * @param 	mode SET_LEDS_MODE , SET_LEDS_SPEED, SET_LEDS_BRIGHTNESS
	 *			param Set mode's parameter
	 * @return 	none.
	 */
	void (*set_rgb_param)(u8 mode, u8 param, bool flash_en);
	bool (*set_rgb_param_ack)(u8 mode, u8 param, bool flash_en);
	bool (*set_single_led_ack)(u8 index , u8 state);
	/**
	 * @brief Send the data to the slave machine to get the message, and wait for the data receive to complete
	 *
	 * @param mode 	GET_LEDS_MODE,GET_LEDS_SPEED,GET_LEDS_BRIGHTNESS,GET_SONIX_VERSION
	 *
	 * @return 	Means get data response true or false.
	 */
	bool (*get_sonix_param)(u8 param);
	/**
	 * @brief Just read SPI data
	 *
	 * @param Data
	 *
	 * @return Data length.
	 */
	u8 (*get_sonix_datas)(u8 *data);

	void (*reset)();

}_sonix_keyboard_manager;

_sonix_keyboard_manager *sonix_keyboard_instance();

void read_rgb_flash(rgb_param *prgb);
void write_rgb_flash(rgb_param prgb);

// --------------------- KEYBOARD LIGHT MODE ------------------------------------
typedef enum{
	LIGHT_NULL = 0x00,
	ALL_LEDS_WHITE,
	COLOR_BREATH,
	TRANSVERSE_SCANNING,
	TRIGGER_SINGLE,
	TRIGGER_SQUARE_SWEEP,
	TRIGGER_TRANSVERSE,
	TRANSVERSE_RANIBOW,
	LONGITUDINAL_RANIBOW,
	SPECTRAL_CYCLE,
	RAINBOW_G,
	RAINBOW_WINDDMILL,
	RAINBOW_RIPPLE,
	DAZZLING_COLORFUL,
	SINE_COLORFUL,
	COLORFUL_RAINDROP,
	COLORFUL_HEAVY_RAINDROP,
	PHANTOM_SCANNING,
	FIVE_COLOR_STACK,
}rgb_mode;

#endif /* UART_DMA_MANAGER_H_ */
