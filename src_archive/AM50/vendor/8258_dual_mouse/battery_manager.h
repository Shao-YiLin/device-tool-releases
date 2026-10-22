#ifndef BATTERY_MANAGER_H_
#define BATTERY_MANAGER_H_

#include "common/types.h"

/**
 * batter state change will call this function
 * @ state  正常：0， 低电：1， 充满：2 低电量通知:3
 */
typedef void(*battery_status_change_cb_fn)(u8 status);

void battery_manager_init(int check_interval, battery_status_change_cb_fn cb);

void battery_manager_check_proc();

bool battery_is_charging();

void reset_battery_detect_time();

bool battery_is_full();

#endif
