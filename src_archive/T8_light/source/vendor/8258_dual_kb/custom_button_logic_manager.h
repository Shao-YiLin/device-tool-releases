/*
 * custom_keys_logic_manager.h
 *
 *  Created on: 2022-2-11
 *      Author: xdhuang
 */

#ifndef CUSTOM_BUTTON_LOGIC_MANAGER_H_
#define CUSTOM_BUTTON_LOGIC_MANAGER_H_

typedef struct
{
	/**
	 * 0:无效   1：标准按键 2：自定义按键
	 */
	unsigned char flag;

	unsigned char index;
	/**
	 * 0: normal key 1: consumer key
	*/
	unsigned char type;
	/**
	 * normal key模式下：8字节的标准键盘协议数据格式;  consume key模式下：2字节的标准键盘
	*/
	unsigned char value[8];

	unsigned char reserved;

	unsigned int offset_addr;
} custom_button_info;

custom_button_info custom_button_param[120];

void init_button_value_flash();
void set_custom_button_param(u8 *data, u8 data_len);

#endif /* CUSTOM_KEYS_LOGIC_MANAGER_H_ */


