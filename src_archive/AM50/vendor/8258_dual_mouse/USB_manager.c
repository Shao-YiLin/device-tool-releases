/*
 * @Descripttion: 
 * @version: 
 * @Author: xdhuang
 * @Date: 2021-05-20 13:31:09
 * @LastEditors: xdhuang
 * @LastEditTime: 2021-05-28 10:24:02
 * @Copyright: Hefei Madao Information Technology Co., Ltd
 */
#include "USB_manager.h"
#include "../../application/usbstd/usbkeycode.h"
#include "../../application/app/usbkb.h"
#include "../../application/app/usbmouse.h"
static u8 web_flag_usb = 0;
//http://feishu.xunfei.cn

typedef struct
{
	u8 ctrl_key;
	u8 keycode;
	u8 next_interval; //uint 10
} y_kbdata_usb;
const y_kbdata_usb web_data_usb[] = {
	{VK_MSK_LWIN, VK_R, 50},
	{0, VK_H, 0},
	{0, VK_T, 0},
	{0, VK_T, 0},
	{0, VK_P, 0},
	{VK_MSK_SHIFT, VK_SEMICOLON, 0}, //:;
	{0, VK_SLASH, 0},				  // /
	{0, VK_SLASH, 0},
	{0, VK_1, 0},
	{0, VK_0, 0},
	{0, VK_2, 0},
	{0, VK_3, 0},
	{0, VK_PERIOD, 0}, //.>
	{0, VK_M, 0},
	{0, VK_A, 0},
	{0, VK_D, 0},
	{0, VK_A, 0},
	{0, VK_O, 0},
	{0, VK_G, 0},
	{0, VK_O, 0},
	{0, VK_PERIOD, 0},
	{0, VK_C, 0},
	{0, VK_O, 0},
	{0, VK_M, 0},
	{VK_MSK_SHIFT, 0, 20},
	{0, VK_ENTER, 0},
};
//bool o_sendor_flag;

void usb_report_web_addr(void)
{
	if (!web_flag_usb)
		return;

	static u8 addr_cnt = 0;
	static u8 flag = 0;
	static u32 tick = 0;
	u8 buf[6] = {0};
	if (flag == 0)
	{
		buf[0] = web_data_usb[addr_cnt].keycode;
		if (usbkb_hid_report_normal(web_data_usb[addr_cnt].ctrl_key, buf))
		{
			flag = 1;
		}
	}
	else if (flag == 1)
	{
		if (usbkb_hid_report_normal(0, buf))
		{
			flag = 2;
		}
	}
	else if (flag == 2)
	{
		if (web_data_usb[addr_cnt].next_interval)
		{
			tick = clock_time();
			flag = 3;
		}
		else
		{
			flag = 4;
		}
	}
	else if ((flag == 3) && clock_time_exceed(tick, web_data_usb[addr_cnt].next_interval * 10000))
	{
		flag = 4;
	}
	else if (flag == 4)
	{
		flag = 0;
		addr_cnt++;
		if (addr_cnt >= (sizeof(web_data_usb) / sizeof(y_kbdata_usb)))
		{
			addr_cnt = 0;
			web_flag_usb = 0;
		}
	}
}

int usb_keyboard_hid_report(u8 *data,u8 len)
{
	if(len != 8 || data == NULL || web_flag_usb)
		return 1;
	u8 ctrl;
	ctrl = *(data);
	return usbkb_hid_report_normal(ctrl, data + 2);
}
int usb_keboard_consumer_report(u8 *data, u8 len)
{
	if(len != 2 || data == NULL)
		return 1;
//	printf("keboard consumer:");
//	for(int i=0;i<len;i++)
//	{
//		printf("%3x",*(data+i));
//	}
//	printf("\n");
	return usbkb_report_consumer_keycode(data, 2);
}

int usb_mouse_hid_report(u8 *data, u8 len)
{
	if(len != 6 || data == NULL)
		return 1;
	return usbmouse_hid_report(HID_REPORT_ID_MOUSE_INPUT, data, len);
}

int madao_interface_report(u8 *data, u8 len)
{
	if(data == NULL)
		return 1;
	return madao_interface_to_host(data, len);
}

int open_the_website()
{
	web_flag_usb=1;
	return 1;
}



