/*
 * peer_manager.h
 *
 *  Created on: 2020-7-28
 *      Author: hanxu
 */

#ifndef PEER_MANAGER_H_
#define PEER_MANAGER_H_

#include "common/types.h"

/**
 * @brief  配对信息 （64 byte）
 * @note   
 * @retval None
 */
typedef struct 
{
    u8 valid_flag; // 数据有效标志
    u8 device_index; // 设备通道索引
    u8 os_type; // 操作系统类型
    u8 host_mac[6]; // 主机MAC地址
    u8 slave_mac[6]; // 从机MAC地址
    u16 deep_sleep_time; // 无触发进行深度休眠的时间间隔
    u8 adv_type; //广播类型
    u8 reserved[46]; //保留数据
}pair_device_info_t;

typedef struct
{
	u8 valid_flag;
	u8 slave_mac[6];
	u8 reserved;
}pair_slave_mac;


int add_pair_device(pair_device_info_t *device);

int remove_pair_device_by_device_index(u8 device_index);

int get_pair_device_by_device_index(u8 device_index, pair_device_info_t *device);

void remove_all();

void set_device_adv_by_reset(u8 device_index, bool reset_flag);

extern void  reset_all_index_pairs();

int update_os_type_by_slave_mac(u8 *slave_mac, int size, u8 os_type);

int update_deep_sleep_time_by_slave_mac(u8 *slave_mac, int size, u16 sleep_time);

int update_device_by_slave_mac(u8 *slave_mac, int size, u8 os_type, u16 sleep_time);

bool check_slave_mac_is_exist(u8 *slave_mac, int size);

void save_last_pair_device(pair_device_info_t *device);

int get_last_pair_device(pair_device_info_t *device);

void clear_last_pair_device();

void save_latest_mac_addr(u8 *mac_addr);

int get_latest_mac_addr(pair_slave_mac *slave_addr);

void clear_latest_mac_addr();

#endif /* PEER_MANAGER_H_ */
