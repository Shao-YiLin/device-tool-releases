/*
 * sensor_manager_PAW3220B.c
 *
 *  Created on: 2022-7-12
 *      Author: xdhuang
 */

#include "sensor_manager.h"
#include "sensor_base.h"
enum PAW3220_OPTICAL_SENSOR_REGISTER
{
	PAW3220_PRODUCT_ID1 = 0,
	PAW3220_PRODUCT_ID2,
	PAW3220_MOTION_STATUS,
	PAW3220_DELTA_X,
	PAW3220_DELTA_Y,
	PAW3220_OPERATION_MODE,
	PAW3220_CONFIGURATION,

	PAW3220_WRITE_PROTECT = 9,
	PAW3220_SLEEP_1,
	PAW3220_SLEEP_2,
	PAW3220_SLEEP_3,
	PAW3220_CPI_X,
	PAW3220_CPI_Y,
	PAW3220_DELTA_XY_HI = 0X12,
	PAW3220_IQC,
	PAW3220_SHUTTER,
	PAW3220_FRAME_AVG = 0x17,
	PAW3220_MOUSE_OPTION = 0x19,
	PAW3220_SPI_MODE = 0x26,
	PAW3220_LED_OPTION = 0x5c,
};

const uint8_t paw3220_cpi_tbl_x[] = {21, 31, 41, 52, 78, 103}; //data*39	800		1600	2000	3000	4000
const uint8_t paw3220_cpi_tbl_y[] = {21, 31, 41, 52, 78, 103}; //data*39	800		1600	2000	3000	4000

const uint8_t config_3220[] = {//3_wired spi , high voltage segment configuration
    0x09, 0x5a,
    0x0A, 0x87,
    0x0D, 0x1A,
    0x0E, 0x1B,
    0x2B, 0x6C,
    0x30, 0x2E,
    0x32, 0x0F,
    0x35, 0x8C,
    0x36, 0x32,
    0x3E, 0x26,
    0x5C, 0xD6,
    0x7F, 0x01,
    0x00, 0x40,
    0x06, 0x28,
    0x07, 0x08,
    0x16, 0x18,
    0x1B, 0x4C,
    0x36, 0x88,
    0x3A, 0x57,
    0x3B, 0x34,
    0x41, 0x22,
    0x42, 0x6D,
    0x43, 0x82,
    0x44, 0x6D,
    0x45, 0x82,
    0x46, 0x22,
    0x47, 0x7C,
    0x48, 0x82,
    0x49, 0x7C,
    0x4A, 0x84,
    0x55, 0x10,
    0x5F, 0x04,
    0x60, 0x02,
    0x62, 0x5C,
    0x64, 0x43,
    0x65, 0x54,
    0x67, 0x93,
    0x68, 0x15,
    0x71, 0x2C,
    0x72, 0x40,
    0x79, 0x08,
    0x7F, 0x00,
    0x2B, 0x6D,
    0x09, 0x00,
};

static uint8_t sensor_3220_motion_status()
{
	return (base_get_construct()->read_byte(PAW3220_MOTION_STATUS) & 0x80);
}

static void set_3220_sensor_wakeup_level(uint8_t enable)
{
	base_get_construct()->set_wakeup_level(enable);
}


static uint16_t paw3220_xy_hi;
static int16_t sensor_3220_movementx(){
#if HIGH_MOUSE_RESOLUTION_ENABLE
	uint16_t reg_x = (uint16_t)base_get_construct()->read_byte(PAW3220_DELTA_X);
	paw3220_xy_hi = (uint16_t)base_get_construct()->read_byte(PAW3220_DELTA_XY_HI);
	reg_x |= (paw3220_xy_hi & 0xf0) << 4;
	reg_x = reg_x << 4;
	int16_t reg_out = (int16_t)reg_x;
	reg_out = reg_out / 16;
	return reg_out;
#else
	int8_t reg_x = (int8_t)base_get_construct()->read_byte(PAW3220_DELTA_X);
	return (int16_t)reg_x;
#endif
}

static int16_t sensor_3220_movementy(){
#if HIGH_MOUSE_RESOLUTION_ENABLE
	uint16_t reg_y = (uint16_t)base_get_construct()->read_byte(PAW3220_DELTA_Y);
	reg_y |= (paw3220_xy_hi & 0x0f) << 8;
	reg_y = reg_y << 4;
	int16_t reg_out = (int16_t)reg_y;
	reg_out = reg_out / 16;
	return reg_out;
#else
	int8_t reg_y = (int8_t)base_get_construct()->read_byte(PAW3220_DELTA_Y);
	return (int16_t)reg_y;
#endif
}

static void _reset_3220(){
	u8 config = base_get_construct()->read_byte(PAW3220_CONFIGURATION);
	base_get_construct()->write_byte(PAW3220_CONFIGURATION, config | 0x80);//(1<<7) = 0X08;
}


static void _set_3220_power_mode(uint8_t mode){
	uint8_t config;
	switch(mode)
	{
	case OPERATING_MODE:
		base_get_construct()->write_byte(PAW3220_OPERATION_MODE, 0xb9);
		config = base_get_construct()->read_byte(PAW3220_CONFIGURATION);
		if(config & (1<<3))
			base_get_construct()->write_byte(PAW3220_CONFIGURATION, config & 0xf7);//(~(1<<3)) = 0XF7
		break;
	case SLEEP_MODE:
		base_get_construct()->write_byte(PAW3220_OPERATION_MODE, 0xbc);
		config = base_get_construct()->read_byte(PAW3220_CONFIGURATION);
		if(config & (1<<3))
			base_get_construct()->write_byte(PAW3220_CONFIGURATION, config & 0xf7);//(~(1<<3)) = 0XF7
		break;
	case POWER_DOWN_MODE:
		config = base_get_construct()->read_byte(PAW3220_CONFIGURATION);
		base_get_construct()->write_byte(PAW3220_CONFIGURATION, config | 0x08);//(1<<3) = 0X08;
		break;
	default:
		break;
	}
}

static uint8_t sensor_3220_set_cpi(uint8_t dpi_index){
	base_get_construct()->write_byte(PAW3220_WRITE_PROTECT, 0x5a);
	for(uint8_t i = 0; i< 32; i++)
	{
		base_get_construct()->write_byte(PAW3220_CPI_X, paw3220_cpi_tbl_x[dpi_index]);
		base_get_construct()->write_byte(PAW3220_CPI_Y, paw3220_cpi_tbl_y[dpi_index]);
		if((base_get_construct()->read_byte(PAW3220_CPI_X) == paw3220_cpi_tbl_x[dpi_index]) && (base_get_construct()->read_byte(PAW3220_CPI_Y) == paw3220_cpi_tbl_y[dpi_index]))
		{
			base_get_construct()->write_byte(PAW3220_WRITE_PROTECT, 0x00);
			return 1;
		}
	}
	base_get_construct()->write_byte(PAW3220_WRITE_PROTECT, 0x00);
	return 0;
}

static uint8_t sensor_3220_init()
{
	base_get_construct()->initial_config(config_3220, sizeof(config_3220));
#if HIGH_MOUSE_RESOLUTION_ENABLE
	for (uint8_t i = 0; i < 8; i++)
	{
		base_get_construct()->write_byte(PAW3220_WRITE_PROTECT, 0x5a);
		base_get_construct()->write_byte(PAW3220_MOUSE_OPTION, 0x04);
		base_get_construct()->write_byte(PAW3220_WRITE_PROTECT, 0x00);
		if (base_get_construct()->read_byte(PAW3220_MOUSE_OPTION) == 0x04)
		{
			return 1;
		}
	}
	return 0;
#else
	return 1;
#endif
}

static struct mos_template __sensor_3220 = {
	.init = sensor_3220_init,
	.set_wakeup_level = set_3220_sensor_wakeup_level,
	.query_motion_status = sensor_3220_motion_status,
	.query_deltax	= sensor_3220_movementx,
	.query_deltay	= sensor_3220_movementy,
	.set_cpi = sensor_3220_set_cpi,
	.reset = _reset_3220,
	.set_power_mode = _set_3220_power_mode,
};

static struct mos_template *sensor_template = NULL;
struct mos_template *mos_3220_base_get_instance()
{
	if (!sensor_template)
	{
		sensor_template = &__sensor_3220;
		sensor_template->init();
	}
	return sensor_template;
}
