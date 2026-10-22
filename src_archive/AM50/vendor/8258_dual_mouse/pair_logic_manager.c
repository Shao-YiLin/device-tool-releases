/*
 * pair_logic_manager.c
 *
 *  Created on: 2020-7-29
 *      Author: hanxu
 */

#include "common/types.h"
// #include "AAA_app_config.h"
#include "AAA_public_Config.h"

static pair_device_info_t current_adv_device;
static pair_device_info_t paired_devices[DEVICE_CHANNEL_SIZE];

void load_paired_device();

static bool is_valid_device_index(u8 device_index)
{

    if (device_index > 0 && device_index <= DEVICE_CHANNEL_SIZE)
        return true;
    return false;
}

static u8 check_paired_device(pair_device_info_t device)
{
    for (size_t i = 0; i < DEVICE_CHANNEL_SIZE; i++)
    {
        if((memcmp((u8 *)device.host_mac, (u8 *)paired_devices[i].host_mac, 6) == 0)
        		&& (memcmp((u8 *)device.slave_mac, (u8 *)paired_devices[i].slave_mac, 6) == 0))
        	return 1;
    }
    return 0;
}

static void _get_default_mac(u8 *mac)
{
    memcpy(mac, (u8 *)&user_cfg.dev_mac, 6);
    *(mac + 4) = MACADDR_CHANNEL;
    *(mac + 5) = MACADDR_AUTH_TYPE;
}

static u8 to_assic(u8 data)
{
    if (data >= 0 && data < 0xa)
    {
        return data + 0x30;
    }
    else
    {
        return (data - 0xa) + 0x41;
    }
}

static u8 _get_device_name(u8 *mac, int size, u8 *name)
{

    //u8 name_len = sizeof(DEVICE_NAME_AAA);
    u8 name_len = DEVICE_NAME_LENGTH;
    memcpy(name, DEVICE_NAME_AAA, name_len);
    *(name + name_len) = '(';
    *(name + name_len + 1) = to_assic(*(mac) >> 4);
    *(name + name_len + 2) = to_assic((*(mac)&0x0f));
    *(name + name_len + 3) = ':';
    *(name + name_len + 4) = to_assic(*(mac + 1) >> 4);
    *(name + name_len + 5) = to_assic((*(mac + 1) & 0x0f));
    *(name + name_len + 6) = ')';
    return name_len + DEVICE_NAME_EXTEND_LENGTH;
}

pair_device_info_t get_current_adv_device()
{
    return current_adv_device;
}

void set_current_device_os_type(u8 os_type){
	current_adv_device.os_type = os_type;

}

void set_current_device_adv_type(u8 adv_type){
	current_adv_device.adv_type = adv_type;
}

void set_current_device_host_mac(u8 *mac, u8 size)
{
    if (NULL == mac)
        return;

    if (size > 6)
        memcpy(current_adv_device.host_mac, mac, 6);
    else
        memcpy(current_adv_device.host_mac, mac, size);
}

void force_save_current_device(bool en)
{
	if(en)
	{
		remove_pair_device_by_device_index(current_adv_device.device_index);
	    add_pair_device(&current_adv_device);
		load_paired_device();
	}
	pair_device_info_t device;
	if(get_last_pair_device(&device))
	{
//		if(memcmp(&current_adv_device,&device,sizeof(pair_device_info_t)))
		if(current_adv_device.device_index != device.device_index)
			save_last_pair_device(&current_adv_device);
	}

}
static void _pair_smp_manager_by_device_index(u8 device_index)
{
    const u8 null_own_ltk[16] = {0};
    smp_param_save_t new_smp_param;
    smp_param_save_t flash_smp_param;
    bls_smp_param_loadByIndex(blc_smp_param_getCurrentBondingDeviceNumber() - 1, &new_smp_param);

    if (memcmp(paired_devices[device_index-1].own_ltk, null_own_ltk, 16)) // 如果此通道配对过,则删除配对的SMP
    {
        u32 effective_address = BONDING_FLASH_ADDR;
        u8 effective_bit;
        flash_read_page(BONDING_FLASH_ADDR + 0xFF0, 1, (u8 *)&effective_bit);
        if(effective_bit == 0x3c)
            effective_address += 0;
        else
            effective_address += 0x1000;
        u32 offset_addr = 0;
        for (int i = 0, j = 0; i < 64; i++, j++)
        {
            offset_addr = i * sizeof(smp_param_save_t);
            flash_read_page(effective_address + offset_addr, sizeof(smp_param_save_t), (u8 *)&flash_smp_param);
            if (flash_smp_param.flag == 0x00)
            {
            	j--;
            	continue;
            }
            else if (flash_smp_param.flag == 0x99)
            {

                if (memcmp(paired_devices[device_index-1].own_ltk, flash_smp_param.own_ltk, 16) == 0) // 找到
                {
                    printf("del smp\n");
                    flash_smp_param.flag = 0;
                    bls_smp_param_deleteByIndex(j);//flash_write_page(effective_address + offset_addr, sizeof(u8), (u8 *)&flash_smp_param.flag);
                    break;
                }
                continue;
            }
            else if (flash_smp_param.flag == 0xff)
                break;
        }
    }
    // else
    //     printf("no pairing:%d\n", device_index);
    memcpy(current_adv_device.own_ltk, new_smp_param.own_ltk, 16); // 直接存储

}

void save_new_connect_device()
{
    _pair_smp_manager_by_device_index(current_adv_device.device_index);
    remove_pair_device_by_device_index(current_adv_device.device_index);

    add_pair_device(&current_adv_device);
    save_last_pair_device(&current_adv_device);
    save_latest_mac_addr(current_adv_device.slave_mac);
    printf("save device: %d \n", current_adv_device.device_index);

    load_paired_device();
}

u8 save_current_device(u8 *slave_mac, u8 *master_mac, u8 adv_type, u8 os_type)
{
	if(memcmp(slave_mac, current_adv_device.slave_mac, 6))
		return 0;
	current_adv_device.adv_type = adv_type;
	set_current_device_host_mac(master_mac, 6);
	current_adv_device.os_type = os_type;
	if (check_paired_device(current_adv_device))
	{
		pair_device_info_t device;
		if(get_last_pair_device(&device))
		{
			if(current_adv_device.device_index != device.device_index)
				save_last_pair_device(&current_adv_device);
		}
		return 1;
	}
//    remove_pair_device_by_device_index(current_adv_device.device_index);
//    add_pair_device(&current_adv_device);
//    save_last_pair_device(&current_adv_device);
//    save_latest_mac_addr(current_adv_device.slave_mac);
//
//    load_paired_device();
    return 1;
}

void set_device_adv_info()
{
    bls_ll_setAdvData((u8 *)tbl_advData_aaa, DEVICE_NAME_LENGTH + DEVICE_NAME_EXTEND_LENGTH + DEV_POSITION);
    bls_ll_setScanRspData(&tbl_advData_aaa[DEV_POSITION - 2], DEVICE_NAME_LENGTH + DEVICE_NAME_EXTEND_LENGTH + 2);
}

static void _set_device_adv(pair_device_info_t *device)
{
    if (NULL == device)
        return;

    memset(&current_adv_device, 0x0, sizeof(pair_device_info_t));
    memcpy(&current_adv_device, device, sizeof(pair_device_info_t));

    // 设置广播数据
    tbl_advData_aaa[14] = device_protocol_version;
    u8 device_name[MAX_DEV_NAME_LEN] = {0};
    u8 name_len = _get_device_name(device->slave_mac, 6, device_name);

    memset(user_cfg.device_name, 0x0, sizeof(user_cfg.device_name));
    memcpy(user_cfg.device_name, device_name, name_len);

    memcpy(&tbl_advData_aaa[DEV_POSITION], device_name, name_len);
    tbl_advData_aaa[DEV_POSITION - 2] = name_len + 1;

    blc_ll_setRandomAddr(device->slave_mac);
    blc_ll_initStandby_module(device->slave_mac);     //mandatory
    blc_ll_initAdvertising_module(device->slave_mac); //adv module: 		 mandatory for BLE slave,

    set_device_adv_info();
    // if (device->valid_flag == 1)
    // {
    //     // 回连模式
    // }
    // else
    // {
    //     // 新设备
    // }
}

static void _get_valid_slave_mac(u8 *org_mac, u8 *dest_mac)
{
    if (NULL == org_mac || NULL == dest_mac)
        return;

    memcpy(dest_mac, org_mac, 6);
    u8 change_value = *(dest_mac + 3);
    u8 high_value = (((((change_value & 0xf0) >> 4) + 1) & 0xf) << 4);
    u8 low_value = ((change_value & 0xf));
    // u8 high_value = (change_value & 0xf0);
    // u8 low_value = ((change_value & 0xf) + 1) & 0xf;
    *(dest_mac + 3) = high_value + low_value;

    if (check_slave_mac_is_exist(dest_mac, 6))
    {
        *(org_mac + 3) = *(dest_mac + 3);
        _get_valid_slave_mac(org_mac, dest_mac);
    }
}

void add_device_adv(u8 device_index)
{
    pair_device_info_t device;
    pair_slave_mac slave_addr;
    int ret = get_latest_mac_addr(&slave_addr);
    u8 slave_mac[6] = {0}; // 默认的mac地址
    _get_default_mac(slave_mac);
    if (ret == 1)
    {
    	if(!memcmp(slave_mac, slave_addr.slave_mac, 3) && (slave_addr.slave_mac[5] == MACADDR_AUTH_TYPE) && (slave_addr.slave_mac[4] == MACADDR_CHANNEL))
    		_get_valid_slave_mac(slave_addr.slave_mac, slave_mac);
    	else
    		reset_all_index_pairs();
    }
    memset(&device, 0x0, sizeof(pair_device_info_t));
    device.valid_flag = 0;
    device.device_index = device_index;
    memcpy(device.slave_mac, slave_mac, sizeof(slave_mac));
   // printf("add new device \n");

    _set_device_adv(&device);
}

void set_device_adv(u8 device_index)
{
    pair_device_info_t device;
    int ret = get_pair_device_by_device_index(device_index, &device);
    if (ret)
    {
        _set_device_adv(&device);
    }
    else
    {
        if (current_adv_device.valid_flag == 0 && strlen((char *)current_adv_device.slave_mac) > 0)
        {
            device.device_index = device_index;
        }
        else
        {
            add_device_adv(device_index);
        }
    }
}

void set_device_adv_by_reset(u8 device_index, bool reset_flag)
{
    if (!is_valid_device_index(device_index))
        return;
    super_mouse_flag = 0;
    if (reset_flag)
    {
    	//pair_device_info_t device = paired_devices[device_index - 1];
        add_device_adv(device_index);
    }
    else
    {
        pair_device_info_t device = paired_devices[device_index - 1];
        if (device.device_index == device_index && device.valid_flag == 1)
        {
        //	printf("set adv device \n");
            _set_device_adv(&device);
        }
        else
        {
        //	printf("add one device \n");
            add_device_adv(device_index);
        }
    }
}

//void set_last_device_adv()
//{
//    pair_device_info_t device;
//
//    int ret = get_last_pair_device(&device);
//    if (ret == 1)
//    {
//        _set_device_adv(&device);
//    }
//    else
//    {
//        add_device_adv(CHANNEL_1);
//    }
//}
void set_last_device_adv()
{
    pair_device_info_t device_last;

    int ret = get_last_pair_device(&device_last);
    if (ret == 1)
    {
    	pair_device_info_t device = paired_devices[device_last.device_index - 1];
    	if (device.device_index == device_last.device_index && device.valid_flag == 1)
    	{
    		_set_device_adv(&device);
    		return;
    	}
    }
    add_device_adv(CHANNEL_1);
}
/**
 * @brief  获取已经配对的历史设备列表
 * @note   
 * @retval None
 */
void load_paired_device()
{
    memset(paired_devices, 0x0, sizeof(pair_device_info_t) * DEVICE_CHANNEL_SIZE);
    for (size_t i = 1; i <= DEVICE_CHANNEL_SIZE; i++)
    {
        get_pair_device_by_device_index(i, &paired_devices[i - 1]);
    }
}
