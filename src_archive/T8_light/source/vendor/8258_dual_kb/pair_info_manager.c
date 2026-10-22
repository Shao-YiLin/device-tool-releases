/*
 * peer_manager.c
 *
 *  Created on: 2020-7-28
 *      Author: hanxu
 */

#include "pair_info_manager.h"
#include "drivers.h"
#include "../../common/string.h"

#define PAIR_DEVICE_DATA_ADDR (0x7b000)
#define LAST_PAIR_DEVICE_DATA_ADDR (0x7c000)
#define PAIR_SLAVE_MAC_DATA_ADDR  (0x7d000)
#define MAX_PAIR_DEVICE_SIZE (64)
#define MAX_PAIR_MAC_SIZE (64)
#define NO_WRITE_FLAG (0xff)
#define PAIR_DEVICE_DATA_PAGE_SIZE (4096)

/**
 * @brief  检测当前写入的flash地址的数据和当前的设备信息是否相同
 * @note   
 * @param  addr: 
 * @param  *device: 
 * @retval 
 */
static bool is_write_success(u64 addr, pair_device_info_t *device)
{
    pair_device_info_t device_info;
    flash_read_page(addr, sizeof(pair_device_info_t), (u8 *)&device_info);
    if (memcmp(&device_info, device, sizeof(pair_device_info_t)) == 0)
        return true;

    return false;
}

/**
 * @brief  获取搬迁之后的可写入的flash的开始地址
 * @note   
 * @retval 
 */
static u64 get_removal_write_addr()
{
    pair_device_info_t devices[MAX_PAIR_DEVICE_SIZE];
    u8 valid_device_size = 0;
    u32 offset_addr = 0;
    pair_device_info_t device_info;
    for (size_t i = 0; i < MAX_PAIR_DEVICE_SIZE; i++)
    {
        offset_addr = i * sizeof(pair_device_info_t);
        flash_read_page(PAIR_DEVICE_DATA_ADDR + offset_addr, sizeof(pair_device_info_t), (u8 *)&device_info);
        if (device_info.valid_flag == 1)
        {
            memcpy((u8 *)&devices[valid_device_size++], &device_info, sizeof(pair_device_info_t));
        }
    }

    flash_erase_sector(PAIR_DEVICE_DATA_ADDR);

    for (size_t i = 0; i < valid_device_size; i++)
    {
        flash_write_page(PAIR_DEVICE_DATA_ADDR + i * sizeof(pair_device_info_t), sizeof(pair_device_info_t), (u8 *)&devices[i]);
    }

    return PAIR_DEVICE_DATA_ADDR + valid_device_size * sizeof(pair_device_info_t);
}

/**
 * @brief  获取当前可写入数据的flash的开始地址
 * @note   
 * @retval 
 */
static u64 get_pair_write_addr()
{
    pair_device_info_t device_info;
    u32 offset_addr = 0;
    for (size_t i = 0; i < MAX_PAIR_DEVICE_SIZE; i++)
    {
        offset_addr = i * sizeof(pair_device_info_t);
        flash_read_page(PAIR_DEVICE_DATA_ADDR + offset_addr, sizeof(pair_device_info_t), (u8 *)&device_info);
        if (device_info.valid_flag == NO_WRITE_FLAG)
        {
            return PAIR_DEVICE_DATA_ADDR + offset_addr;
        }
    }

    return get_removal_write_addr();
}

int add_pair_device(pair_device_info_t *device)
{
    device->valid_flag = 1;
    u64 write_addr = get_pair_write_addr();
    flash_write_page(write_addr, sizeof(pair_device_info_t), (u8 *)device);
    if (is_write_success(write_addr, device) == true)
        return 1;
    else
        return 0;
}

int remove_pair_device_by_device_index(u8 device_index)
{
    pair_device_info_t device_info;
    u32 offset_addr = 0;
    for (size_t i = 0; i < MAX_PAIR_DEVICE_SIZE; i++)
    {
        offset_addr = i * sizeof(pair_device_info_t);
        flash_read_page(PAIR_DEVICE_DATA_ADDR + offset_addr, sizeof(pair_device_info_t), (u8 *)&device_info);
        if (device_info.valid_flag == 1 && device_info.device_index == device_index)
        {
            device_info.valid_flag = 0;
            flash_write_page(PAIR_DEVICE_DATA_ADDR + offset_addr, 1, &device_info.valid_flag);
            return 1;
        }
    }
    return 0;
}

int get_pair_device_by_device_index(u8 device_index, pair_device_info_t *device)
{
    pair_device_info_t device_info;
    u32 offset_addr = 0;
    for (size_t i = 0; i < MAX_PAIR_DEVICE_SIZE; i++)
    {
        offset_addr = i * sizeof(pair_device_info_t);
        flash_read_page(PAIR_DEVICE_DATA_ADDR + offset_addr, sizeof(pair_device_info_t), (u8 *)&device_info);
        if (device_info.valid_flag == 1 && device_info.device_index == device_index)
        {
            memcpy((u8 *)device, &device_info, sizeof(pair_device_info_t));
            return 1;
        }
    }
    return 0;
}

void remove_all()
{
    flash_erase_sector(PAIR_DEVICE_DATA_ADDR);
}

int update_os_type_by_slave_mac(u8 *slave_mac, int size, u8 os_type)
{
    pair_device_info_t device_info;
    u32 offset_addr = 0;
    for (size_t i = 0; i < MAX_PAIR_DEVICE_SIZE; i++)
    {
        offset_addr = i * sizeof(pair_device_info_t);
        flash_read_page(PAIR_DEVICE_DATA_ADDR + offset_addr, sizeof(pair_device_info_t), (u8 *)&device_info);
        if (device_info.valid_flag == 1 && memcmp(slave_mac, device_info.slave_mac, size) == 0)
        {
        	// 写设置当前为无效，然后在追加一个配对设备
        	device_info.valid_flag = 0;
        	flash_write_page(PAIR_DEVICE_DATA_ADDR + offset_addr, 1, &device_info.valid_flag);
            device_info.os_type = os_type;
            device_info.valid_flag = 1;
            add_pair_device(&device_info);
            return 1;
        }
    }
    return 0;
}

int update_deep_sleep_time_by_slave_mac(u8 *slave_mac, int size, u16 sleep_time)
{
    pair_device_info_t device_info;
    u32 offset_addr = 0;
    for (size_t i = 0; i < MAX_PAIR_DEVICE_SIZE; i++)
    {
        offset_addr = i * sizeof(pair_device_info_t);
        flash_read_page(PAIR_DEVICE_DATA_ADDR + offset_addr, sizeof(pair_device_info_t), (u8 *)&device_info);
        if (device_info.valid_flag == 1 && memcmp(slave_mac, device_info.slave_mac, size) == 0)
        {
            // 写设置当前为无效，然后在追加一个配对设备
            device_info.valid_flag = 0;
            flash_write_page(PAIR_DEVICE_DATA_ADDR + offset_addr, 1, &device_info.valid_flag);
            device_info.valid_flag = 1;
            device_info.deep_sleep_time = sleep_time;
            add_pair_device(&device_info);
            return 1;
        }
    }
    return 0;
}

int update_device_by_slave_mac(u8 *slave_mac, int size, u8 os_type, u16 sleep_time)
{
    pair_device_info_t device_info;
    u32 offset_addr = 0;
    for (size_t i = 0; i < MAX_PAIR_DEVICE_SIZE; i++)
    {
        offset_addr = i * sizeof(pair_device_info_t);
        flash_read_page(PAIR_DEVICE_DATA_ADDR + offset_addr, sizeof(pair_device_info_t), (u8 *)&device_info);
        if (device_info.valid_flag == 1 && memcmp(slave_mac, device_info.slave_mac, size) == 0)
        {
            // 写设置当前为无效，然后在追加一个配对设备
            device_info.valid_flag = 0;
            flash_write_page(PAIR_DEVICE_DATA_ADDR + offset_addr, 1, &device_info.valid_flag);
            device_info.valid_flag = 1;
            device_info.deep_sleep_time = sleep_time;
            device_info.os_type = os_type;
            add_pair_device(&device_info);
            return 1;
        }
    }
    return 0;
}

/**
 * @brief  检测当前的slave地址是否已经存在
 * @note   
 * @param  *slave_mac: 
 * @param  size: 
 * @retval 
 */
bool check_slave_mac_is_exist(u8 *slave_mac, int size)
{
    pair_device_info_t device_info;
    u32 offset_addr = 0;
    for (size_t i = 0; i < MAX_PAIR_DEVICE_SIZE; i++)
    {
        offset_addr = i * sizeof(pair_device_info_t);
        flash_read_page(PAIR_DEVICE_DATA_ADDR + offset_addr, sizeof(pair_device_info_t), (u8 *)&device_info);
        if (device_info.valid_flag == 1 && memcmp(slave_mac, device_info.slave_mac, size) == 0)
        {
            return true;
        }
    }
    return false;
}

static u32 get_last_pair_write_addr()
{
    pair_device_info_t device_info;
    u32 offset_addr = 0;
    for (size_t i = 0; i < MAX_PAIR_DEVICE_SIZE; i++)
    {
        offset_addr = i * sizeof(pair_device_info_t);
        flash_read_page(LAST_PAIR_DEVICE_DATA_ADDR + offset_addr, sizeof(pair_device_info_t), (u8 *)&device_info);
        if (device_info.valid_flag == NO_WRITE_FLAG)
        {
            return LAST_PAIR_DEVICE_DATA_ADDR + offset_addr;
        }
    }
    flash_erase_sector(LAST_PAIR_DEVICE_DATA_ADDR);
    flash_write_page(LAST_PAIR_DEVICE_DATA_ADDR, sizeof(pair_device_info_t), (u8 *)&device_info);
    return LAST_PAIR_DEVICE_DATA_ADDR + sizeof(pair_device_info_t);
}

void save_last_pair_device(pair_device_info_t *device)
{
    // pair_device_info_t device_info;
    // flash_read_page(LAST_PAIR_DEVICE_DATA_ADDR, sizeof(pair_device_info_t), (u8 *)&device_info);
    // if (is_write_success(LAST_PAIR_DEVICE_DATA_ADDR, device) == true)
    //     return;
	u32 offset_addr = get_last_pair_write_addr();

    flash_write_page(offset_addr, sizeof(pair_device_info_t), (u8 *)device);
}

int get_last_pair_device(pair_device_info_t *device)
{
    flash_read_page(get_last_pair_write_addr() - sizeof(pair_device_info_t) , sizeof(pair_device_info_t), (u8 *)device);
    if (device->valid_flag == NO_WRITE_FLAG)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

void clear_last_pair_device()
{
    flash_erase_sector(LAST_PAIR_DEVICE_DATA_ADDR);
}

static u32 get_slave_mac_write_addr()
{
	pair_slave_mac device_info;
    u32 offset_addr = 0;
    for (size_t i = 0; i < MAX_PAIR_MAC_SIZE; i++)
    {
        offset_addr = i * sizeof(pair_slave_mac);
        flash_read_page(PAIR_SLAVE_MAC_DATA_ADDR + offset_addr, sizeof(pair_slave_mac), (u8 *)&device_info);
        if (device_info.valid_flag == NO_WRITE_FLAG)
        {
            return PAIR_SLAVE_MAC_DATA_ADDR + offset_addr;
        }
    }
    flash_erase_sector(PAIR_SLAVE_MAC_DATA_ADDR);
    flash_write_page(PAIR_SLAVE_MAC_DATA_ADDR, sizeof(pair_slave_mac), (u8 *)&device_info);
    return PAIR_SLAVE_MAC_DATA_ADDR + sizeof(pair_slave_mac);
}

void save_latest_mac_addr(u8 *mac_addr)
{
	pair_slave_mac device_info;
	device_info.valid_flag = 0x01;
	memcpy(device_info.slave_mac, mac_addr, 6);
	u32 offset_addr = get_slave_mac_write_addr();
	flash_write_page(offset_addr, sizeof(pair_slave_mac), (u8 *)&device_info);
}

int get_latest_mac_addr(pair_slave_mac *slave_addr)
{
    flash_read_page(get_slave_mac_write_addr() - sizeof(pair_slave_mac) , sizeof(pair_slave_mac), (u8 *)slave_addr);
    if (slave_addr->valid_flag == 1)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

void clear_latest_mac_addr()
{
	flash_erase_sector(PAIR_SLAVE_MAC_DATA_ADDR);
}
