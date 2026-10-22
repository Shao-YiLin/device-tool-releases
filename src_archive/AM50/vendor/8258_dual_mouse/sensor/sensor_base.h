/*
 * sensor_base.h
 *
 *  Created on: 2022-7-12
 *      Author: win10
 */

#ifndef SENSOR_BASE_H_
#define SENSOR_BASE_H_
#include "sensor_platform_define.h"

struct sensor_base
{
	void(*set_wakeup_level)(uint8_t enable);
	uint8_t(*read_byte)(uint8_t addr);
	void(*write_byte)(uint8_t addr, uint8_t data);
	void(*initial_config)(const uint8_t *ptbl, uint32_t len);
	void(*sensor_resync)();
	uint16_t(*sensor_identity)(uint8_t pid1_addr, uint8_t pid2_addr);
};

struct sensor_base *base_get_construct();
#endif /* SENSOR_BASE_H_ */
