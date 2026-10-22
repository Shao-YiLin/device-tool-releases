/*
 * sensor_manager_SG8912.c
 *
 *  Created on: 2022-7-12
 *      Author: xdhuang
 */
#include "sensor_manager.h"
#include "sensor_base.h"
enum MX8912_OPTICAL_SENSOR_REGISTER
{
	MX8912_PRODUCT_ID1 = 0,
	MX8912_PRODUCT_ID2,
	MX8912_MOTION_STATUS,
	MX8912_DELTA_X_L,
	MX8912_DELTA_Y_L,
	MX8912_OPERATION_MODE,
	MX8912_CONFIGURATION,
	MX8912_ENTER_SLEEP1_TIME,
	MX8912_SLEEP1_SETTING,
	MX8912_WRITE_PROTECT,
	MX8912_ENTER_SLEEP2_TIME,
	MX8912_SLEEP2_SETTING,
	MX8912_CPI_CONFIG_X = 0x0d,
	MX8912_CPI_CONFIG_Y,
	MX8912_ENTER_SLEEP3_TIME,
	MX8912_SLEEP3_SETTING,
	MX8912_DELTA_XY_HI = 0x12,
	MX8912_SQUAL,
	MX8912_SHUTTER_H,
	MX8912_SHUTTER_L,
	MX8912_MAX,
	MX8912_FRAME_AVG,
	MX8912_MIN,
	MX8912_MOUSE_OPTION,
	MX8912_SUM,
	MX8912_SPI_MODE = 0x26,
	MX8912_SG_WP = 0x7f,
};

const uint16_t mx8912_cpi_tbl[] = {0x320, 0x4b0, 0x640, 0x7d0, 0xbb8, 0xfa0, 0x1388};//	800	1200	1600	2000	3000	4000	5000
const uint8_t config_8912[] = {
		0X06, 0X84,
		0X7F, 0XA7,
		0XFF, 0XA7,
		0X9C, 0X2B,
		0X67, 0X13,
		0XFF, 0X00,
		0X7F, 0X00,
};

static void set_8912_sensor_wakeup_level(uint8_t enable)
{
	base_get_construct()->set_wakeup_level(enable);
}

static uint8_t sensor_8912_motion_status()
{
	return (base_get_construct()->read_byte(MX8912_MOTION_STATUS) & 0x80);
}

static int16_t sensor_8912_movementx(){
#if HIGH_MOUSE_RESOLUTION_ENABLE
	uint16_t reg_x = (uint16_t)base_get_construct()->read_byte(MX8912_DELTA_X_L);
	reg_x |= ((uint16_t)base_get_construct()->read_byte(MX8912_DELTA_XY_HI) & 0xf0) << 4;
	reg_x = reg_x << 4;
	int16_t reg_out = (int16_t)reg_x;
	reg_out = reg_out / 16;
	return reg_out;
#else
	int8_t reg_y = (int8_t)base_get_construct()->read_byte(MX8912_DELTA_X_L);
	return (int16_t)reg_y;
#endif
}

static int16_t sensor_8912_movementy(){
#if HIGH_MOUSE_RESOLUTION_ENABLE
	uint16_t reg_y = (uint16_t)base_get_construct()->read_byte(MX8912_DELTA_Y_L);
	reg_y |= ((uint16_t)base_get_construct()->read_byte(MX8912_DELTA_XY_HI) & 0x0f) << 8;
	reg_y = reg_y << 4;
	int16_t reg_out = (int16_t)reg_y;
	reg_out = reg_out / 16;
	return reg_out;
#else
	int8_t reg_y = (int8_t)base_get_construct()->read_byte(MX8912_DELTA_Y_L);
	return (int16_t)reg_y;
#endif
}

static void _reset_8912(){
	u8 config = base_get_construct()->read_byte(MX8912_CONFIGURATION);
	base_get_construct()->write_byte(MX8912_CONFIGURATION, config | 0x80);//(1<<7) = 0X08;
}

static void _set_8912_power_mode(uint8_t mode){
	uint8_t config;
	uint8_t operating_mode;
	switch(mode)
	{
	case OPERATING_MODE:
		operating_mode = base_get_construct()->read_byte(MX8912_OPERATION_MODE);
		base_get_construct()->write_byte(MX8912_OPERATION_MODE, operating_mode & 0xf0);
		config = base_get_construct()->read_byte(MX8912_CONFIGURATION);
		if(config & (1<<3))
			base_get_construct()->write_byte(MX8912_CONFIGURATION, config & 0xf7);//(~(1<<3)) = 0XF7
		break;
	case SLEEP_MODE:
		operating_mode = base_get_construct()->read_byte(MX8912_OPERATION_MODE);
		operating_mode &= 0xf0;
		operating_mode |= 0x06;
		base_get_construct()->write_byte(MX8912_OPERATION_MODE, operating_mode);
		config = base_get_construct()->read_byte(MX8912_CONFIGURATION);
		if(config & (1<<3))
			base_get_construct()->write_byte(MX8912_CONFIGURATION, config & 0xf7);//(~(1<<3)) = 0XF7
		break;
	case POWER_DOWN_MODE:
		config = base_get_construct()->read_byte(MX8912_CONFIGURATION);
		base_get_construct()->write_byte(MX8912_CONFIGURATION, config | 0x08);//(1<<3) = 0X08;
		break;
	default:
		break;
	}
}

static uint8_t sensor_8912_set_cpi(uint8_t dpi_index){
	base_get_construct()->write_byte(MX8912_WRITE_PROTECT, 0x5a);
	//uint8_t data = base_get_construct()->read_byte(0x67);
	//printf("set_cpi:%x\n",data);
	for(uint8_t i = 0; i< 32; i++)
	{
		base_get_construct()->write_byte(MX8912_CPI_CONFIG_X, (uint8_t)(mx8912_cpi_tbl[dpi_index] >> 8));
		base_get_construct()->write_byte(MX8912_CPI_CONFIG_Y, (uint8_t)(mx8912_cpi_tbl[dpi_index] & 0xff));
		if((base_get_construct()->read_byte(MX8912_CPI_CONFIG_X) == (uint8_t)(mx8912_cpi_tbl[dpi_index] >> 8)) && (base_get_construct()->read_byte(MX8912_CPI_CONFIG_Y) == (uint8_t)(mx8912_cpi_tbl[dpi_index] & 0xff)))
		{
			base_get_construct()->write_byte(MX8912_WRITE_PROTECT, 0x00);
			return 1;
		}
	}
	base_get_construct()->write_byte(MX8912_WRITE_PROTECT, 0x00);
	return 0;
}

static uint8_t sensor_8912_init(){
	base_get_construct()->initial_config(config_8912, sizeof(config_8912));
#if HIGH_MOUSE_RESOLUTION_ENABLE
	uint8_t mouse_option = 0x20;
	base_get_construct()->write_byte(MX8912_WRITE_PROTECT, 0x5a);

	for (uint8_t i = 0; i < 8; i++)
	{
		base_get_construct()->write_byte(MX8912_MOUSE_OPTION, mouse_option | 0x04);
		if (base_get_construct()->read_byte(MX8912_MOUSE_OPTION) == (mouse_option | 0x04))
		{
			base_get_construct()->write_byte(MX8912_WRITE_PROTECT, 0x00);
			return 1;
		}
	}
	base_get_construct()->write_byte(MX8912_WRITE_PROTECT, 0x00);
	return 0;
#endif
	return 1;
}

static struct mos_template __sensor_8912 = {
	.init = sensor_8912_init,
	.set_wakeup_level = set_8912_sensor_wakeup_level,
	.query_motion_status = sensor_8912_motion_status,
	.query_deltax	= sensor_8912_movementx,
	.query_deltay	= sensor_8912_movementy,
	.set_cpi = sensor_8912_set_cpi,
	.reset = _reset_8912,
	.set_power_mode = _set_8912_power_mode,
};

static struct mos_template *sensor_template = NULL;
struct mos_template *mos_8912_base_get_instance()
{
	if (!sensor_template)
	{
		sensor_template = &__sensor_8912;
		sensor_template->init();
		//__sensor_8912->reset = base_get_construct()->sensor_resync;
		//__sensor_8912->reset_1 = base_get_construct()->sensor_resync;
	}
	return sensor_template;
}
