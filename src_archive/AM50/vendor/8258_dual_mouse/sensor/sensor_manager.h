/*
 * sensor_manager.h
 *
 *  Created on: 2022-7-12
 *      Author: xdhuang
 */
#ifndef SENSOR_MANAGER_H_
#define SENSOR_MANAGER_H_
#include "sensor_platform_define.h"

#if SENSOR_FUN_ENABLE_AAA
enum POWER_MODE
{
	OPERATING_MODE,
	SLEEP_MODE,
	POWER_DOWN_MODE
};

struct mos_template {
	// optical sensor array
	uint8_t (*init)();
	void (*set_wakeup_level)(uint8_t enable);
	uint8_t (*query_motion_status)();
	int16_t (*query_deltax)();
	int16_t (*query_deltay)();
	uint8_t (*set_cpi)(uint8_t index);
	void (*reset)();
	void (*set_power_mode)(uint8_t mode);
};

struct mos_template *mos_8912_base_get_instance(void);
struct mos_template *mos_3220_base_get_instance(void);
//struct mos_template *mos_3311_base_get_instance(void);
#endif
#endif /* SENSOR_MANAGER_H_ */
