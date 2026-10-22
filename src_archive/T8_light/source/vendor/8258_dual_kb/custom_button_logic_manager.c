/*
 * custom_button_logic_manager.c
 *
 *  Created on: 2022-2-11
 *      Author: xdhuang
 */
#include "../../drivers/8258/flash.h"
#include "../../common/string.h"
#include "common/types.h"
#include "custom_button_logic_manager.h"
#include "keyboard_scan.h"
#define NULL_BYTE_FLAG (0xff)
#define CUSTOM_BUTTON_VALUE_ADDR (0x7e000)
#define MAX_BUTTON_DATA_SIZE (256)

/**
 * @brief  获取搬迁之后的可写入的flash的开始地址
 * @note
 * @retval
 */
static u32 get_button_removal_write_addr()
{
	custom_button_info custom_button_arr[MAX_BUTTON_DATA_SIZE];
    u8 valid_button_size = 0;
    u32 offset_addr = 0;
    custom_button_info custom_button;
    for (size_t i = 0; i < MAX_BUTTON_DATA_SIZE; i++)
    {
        offset_addr = i * sizeof(custom_button_info);
        flash_read_page(CUSTOM_BUTTON_VALUE_ADDR + offset_addr, sizeof(custom_button_info), (u8 *)&custom_button);
        if (custom_button.flag != 0)
        {
            memcpy((u8 *)&custom_button_arr[valid_button_size++], &custom_button, sizeof(custom_button_info));
        }
    }

    flash_erase_sector(CUSTOM_BUTTON_VALUE_ADDR);

    for (size_t i = 0; i < valid_button_size; i++)
    {
        flash_write_page(CUSTOM_BUTTON_VALUE_ADDR + i * sizeof(custom_button_info), sizeof(custom_button_info), (u8 *)&custom_button_arr[i]);
    }
    return CUSTOM_BUTTON_VALUE_ADDR + valid_button_size * sizeof(custom_button_info);
}

/**
 * @brief  获取当前可写入数据的flash的开始偏移量地址
 * @note
 * @retval
 */
static u32 get_button_writable_addr()
{
	custom_button_info custom_button;
	u32 offset_addr = 0;
	for (u32 i = 0; i < MAX_BUTTON_DATA_SIZE; i++)
	{
		offset_addr = i * sizeof(custom_button_info);
		flash_read_page(CUSTOM_BUTTON_VALUE_ADDR + offset_addr, 1, (u8 *)&custom_button);
		if (custom_button.flag == NULL_BYTE_FLAG)
		{
			return CUSTOM_BUTTON_VALUE_ADDR + offset_addr;
		}
	}
	//当前扇区已写满
	return get_button_removal_write_addr();
}

/**
 * @brief  flash中写入新数据
 * @note
 * @retval
 */

static void set_valid_info_to_flash(custom_button_info *custom_button)
{
	custom_button_info button_param;
	if(custom_button->offset_addr)
	{
		flash_read_page(custom_button->offset_addr, 2, (u8 *)&button_param);
		if((button_param.flag != 0) && (button_param.index == custom_button->index))
		{
			button_param.flag = 0;
			flash_write_page(custom_button->offset_addr, 1, (u8 *)(&button_param.flag));
		}
	}
	if(custom_button->flag)
	{
		custom_button->offset_addr = get_button_writable_addr();
		flash_write_page(custom_button->offset_addr, sizeof(custom_button_info), (u8 *)custom_button);
	}
}

static u32 get_button_value_from_flash(custom_button_info *custom_button)
{
	custom_button_info button_param;
	u32 i ;
	for(i = 0; i< MAX_BUTTON_DATA_SIZE; i++)
	{
		flash_read_page(CUSTOM_BUTTON_VALUE_ADDR + i * sizeof(custom_button_info), 1, (u8 *)&button_param);
		if(button_param.flag == 0xff)
		{
			return i *sizeof(custom_button_info);
		}
		else if(button_param.flag != 0)
		{
			flash_read_page(CUSTOM_BUTTON_VALUE_ADDR + i *sizeof(custom_button_info), sizeof(custom_button_info), (u8 *)&button_param);
			memcpy((u8 *)(custom_button + button_param.index), (u8 *)&button_param, sizeof(custom_button_info));
		}
	}
	return i *sizeof(custom_button_info);
}

void init_button_value_flash()
{
//	if(!get_button_value_from_flash(custom_button_param))
//	{
//		custom_button_param[MS_Voice -1].flag = 1;
//		custom_button_param[MS_Voice -1].type = 0;
//		custom_button_param[MS_Voice -1].index = MS_Voice -1;
//		custom_button_param[MS_Voice -1].value[2] = 0x65;	//app
//		set_valid_info_to_flash(custom_button_param + MS_Voice -1);
//	}
	get_button_value_from_flash(custom_button_param);
}

void set_custom_button_param(u8 *data, u8 data_len)
{
	u8 key_value_differences_flag = 0;
	if(custom_button_param[*data].flag != *(data + 1))
	{
		custom_button_param[*data].flag = *(data + 1);
		key_value_differences_flag = 1;
	}
	if(custom_button_param[*data].type != *(data + 2))
	{
		custom_button_param[*data].type = *(data + 2);
		key_value_differences_flag = 1;
	}
	if(memcmp(custom_button_param[*data].value, data + 3, 8))
	{
		memcpy(custom_button_param[*data].value, data + 3, 8);
		key_value_differences_flag = 1;
	}
	if(key_value_differences_flag){
		custom_button_param[*data].index = *data;
		set_valid_info_to_flash(custom_button_param + (*data));
	}
}


