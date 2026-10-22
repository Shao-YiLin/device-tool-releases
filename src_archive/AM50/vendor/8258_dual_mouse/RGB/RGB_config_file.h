/*
 * RGN_config_file.h
 *
 *  Created on: 2022-7-8
 *      Author: xdhuang
 */

#ifndef RGN_CONFIG_FILE_H_
#define RGN_CONFIG_FILE_H_
#include "RGB_ins_manager.h"
enum RGB_PRIORITY_RANK
{
	BREATHING_PRIORITY,
	CUSTOM_RGB_PRIORITY,
	RECHARGE_RGB_PRIORITY,
	CHARGE_FULL_RGB_PRIORITY,
	VOICE_PRIORITY,
	LOW_POWER_PRIORITY,
	ULTRA_LOW_POWWR_PRIORITY,
	CLOSE_ALL_PRIORITY
};

#define recharge_rgb \
	(single_cfg){\
	.colour_value.rgb_red = 255,\
	.colour_value.rgb_green = 0,\
	.colour_value.rgb_blue = 0,\
	.onTime_ms = 500,\
	.offTime_ms = 0,\
	.repeatCount = 0xff,\
	.priority = RECHARGE_RGB_PRIORITY,\
}

#define charge_full_rgb \
	(single_cfg){\
	.colour_value.rgb_red = 0,\
	.colour_value.rgb_green = 255,\
	.colour_value.rgb_blue = 0,\
	.onTime_ms = 1,\
	.offTime_ms = 0,\
	.repeatCount = 0x0,\
	.priority = CHARGE_FULL_RGB_PRIORITY,\
}

#define custom_theme(rgb, count)\
(single_cfg){\
	.colour_value = *(rgb_data*)(rgb),\
	.onTime_ms = 100,\
	.offTime_ms = 0,\
	.repeatCount = count,\
	.priority = CUSTOM_RGB_PRIORITY,\
}

#define voice_rgb_by_value(r, g, b)\
	(single_cfg){\
	.colour_value.rgb_red = r,\
	.colour_value.rgb_green = g,\
	.colour_value.rgb_blue = b,\
	.onTime_ms = 100,\
	.offTime_ms = 0,\
	.repeatCount = 0xff,\
	.priority = VOICE_PRIORITY,\
}

#define low_battery_rgb \
	(single_cfg){\
	.colour_value.rgb_red = 255,\
	.colour_value.rgb_green = 0,\
	.colour_value.rgb_blue = 0,\
	.onTime_ms = 250,\
	.offTime_ms = 250,\
	.repeatCount = 1,\
	.priority = LOW_POWER_PRIORITY,\
}

#define ultra_low_battery_rgb \
	(single_cfg){\
	.colour_value.rgb_red = 255,\
	.colour_value.rgb_green = 0,\
	.colour_value.rgb_blue = 0,\
	.onTime_ms = 100,\
	.offTime_ms = 100,\
	.repeatCount = 1,\
	.priority = ULTRA_LOW_POWWR_PRIORITY,\
}

#define close_all_rgb \
	(single_cfg){\
	.colour_value.rgb_red = 0,\
	.colour_value.rgb_green = 0,\
	.colour_value.rgb_blue = 0,\
	.onTime_ms = 0,\
	.offTime_ms = 0,\
	.repeatCount = 0,\
	.priority = CLOSE_ALL_PRIORITY,\
}

//static const single_cfg low_battery_rgb = {{255, 0, 0}, 250, 250, 0xff, LOW_POWER_PRIORITY};
//static const single_cfg ultra_low_battery_rgb = {{255, 0, 0}, 100, 100, 0xff, ULTRA_LOW_POWWR_PRIORITY};
//static const single_cfg close_all_rgb = {{0, 0, 0}, 0, 1, 1, CLOSE_ALL_PRIORITY};
#endif /* RGN_CONFIG_FILE_H_ */
