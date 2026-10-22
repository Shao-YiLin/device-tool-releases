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
#include "keyboard_scan.h"
//#include "../../application/usbstd/usbkeycode.h"
#include "../../application/app/usbkb.h"
static u8 web_flag_usb = 0;
//http://feishu.xunfei.cn

typedef struct
{
	u8 ctrl_key;
	u8 keycode;
	u8 next_interval; //uint 10
} y_kbdata_usb;
//http://product_NO.madaogo.com
y_kbdata_usb web_data_usb[22+PRODUCT_NO_LENGTH];
static int web_count = 0;
void web_data_usb_init()
{
	y_kbdata_usb web_head[] = {
			{VK_MSK_LWIN, VK_R, 50},
			{0, VK_H, 0},
			{0, VK_T, 0},
			{0, VK_T, 0},
			{0, VK_P, 0},
		//	{0, VK_S, 0},
			{VK_MSK_LSHIFT, VK_SEMICOLON, 0}, //:;
			{0, VK_SLASH, 0},				  // /
			{0, VK_SLASH, 0},
	};
	y_kbdata_usb web_tail[] = {
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
			{VK_MSK_LSHIFT, 0, 20},
			{0, VK_ENTER, 0},
	};
	u8 product_NO[PRODUCT_NO_LENGTH] = DEVICE_PRODUCT_NO;
	memcpy(web_data_usb,web_head,8*sizeof(y_kbdata_usb));
	for(u8 i=0;i<PRODUCT_NO_LENGTH;i++)
	{
		if(product_NO[i]>='A'&&product_NO[i]<='Z')
			web_data_usb[8+i] = (y_kbdata_usb){0,product_NO[i]-'A'+VK_A, 0};
		else if(product_NO[i]>='1'&&product_NO[i]<='9')
			web_data_usb[8+i] = (y_kbdata_usb){0,product_NO[i]-'1'+VK_1, 0};
		else if(product_NO[i]=='0')
			web_data_usb[8+i] = (y_kbdata_usb){0,VK_0, 0};
	}
	memcpy(web_data_usb+(8+PRODUCT_NO_LENGTH),web_tail,14*sizeof(y_kbdata_usb));
	web_count = 22+PRODUCT_NO_LENGTH;
}

void usb_report_web_addr(void)
{
	if (!web_flag_usb)
		return;

	static u8 addr_cnt = 0;
	static u8 flag = 0;
	static u32 tick = 0;
	u8 buf[14] = {0};
	if (flag == 0)
	{
		buf[0] = web_data_usb[addr_cnt].ctrl_key;
		buf[2] = web_data_usb[addr_cnt].keycode;
		if (usbkb_hid_report_normal(buf[0], buf + 2))
		{
			flag = 1;
		}
	}
	else if (flag == 1)
	{
		if (usbkb_hid_report_normal(0, buf+2))
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
// 定义两个数组，用于存储上一次的键盘按键状态
static u8 last_6kro_value[8] = {0,0,0,0,0,0,0,0};
static u8 last_nkro_value[13] = {0,0,0,0,0,0,0,0,0,0,0,0,0};

int usb_keyboard_hid_report(u8 *data,u8 len)
{
	if(len != 14 || data == NULL)
		return 1;
	u8 result = 1;
	u8 result_nkro = 1;
    u8 cout = 2;
    // 定义两个数组，用于存储当前的键盘按键状态
    u8 current_6kro_value[8] = {0,0,0,0,0,0,0,0};
    u8 current_nkro_value[13] = {0,0,0,0,0,0,0,0,0,0,0,0,0};
    current_6kro_value[0] = *data;
    memcpy(current_nkro_value, data + 1, 13);

    // 处理标准6键模式下的按键状态
    for(int i = 2; i < 8; i++)
    {
        u8 value = last_6kro_value[i];
        if(value)
        {
            // 如果当前按键状态中包含上一次按键状态中不存在的按键，则将这些按键添加到current_6kro_value数组中
            if(current_nkro_value[value >> 3] & (1 << (value & 0x07)))// 如果之前的按键依旧被按下
            {
                current_6kro_value[cout++] = value;// 加入到当前的6键
                current_nkro_value[value >> 3] &= ~(1 << (value & 0x07));// 全键中删除此按键
            }
        }
        else break;
    }

    // 如果6键无冲的数组没有满，则将其填满（只填上次全键无冲没有的值）
    if(cout < 8)
    {
        for(u8 i = 0; i < 13; i++)
        {
            // 检查current_nkro_value中哪些按键是本次按下但上一次没有按下的按键，将这些按键添加到current_6kro_value数组中
            u8 key_value = current_nkro_value[i] & (current_nkro_value[i] ^ last_nkro_value[i]);//本次全键无冲有的值，但上次全键无冲没有的值
            if(key_value & 0xff)
            {
                for(u8 j = 0; j < 8; j++)
                {
                    if((key_value >> j) & 0x01)
                    {
                        current_nkro_value[i] &= ~(1 << j);
                        current_6kro_value[cout++] = i * 8 + j;
                        if(cout >= 8)
                            goto send;
                    }
                }
            }
        }
    }
send:
    // 如果current_6kro_value数组中有新的按键被添加，生成标准6键模式的HID报告并发送给主机
    if(memcmp(last_6kro_value, current_6kro_value, 8))
    {
    	result = usbkb_hid_report_normal(current_6kro_value[0], current_6kro_value+2);
        if(result)
            memcpy(last_6kro_value, current_6kro_value, 8);

    }

    // 如果current_nkro_value数组中有新的按键被添加，生成全键无冲模式的HID报告并发送给主机
    if(memcmp(last_nkro_value, current_nkro_value, 13))
    {
    	result_nkro = usbkb_hid_report_nkro(current_nkro_value, 13);
        if(result_nkro)
            memcpy(last_nkro_value, current_nkro_value, 13);
    }
	return result & result_nkro;


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

int madao_interface_report(u8 *data, u8 len)
{
	if(data == NULL)
		return 1;
	return madao_interface_to_host(data, len);
}

int open_the_website(u8 *data, u8 len)
{
	if(len == 7 && data)
		web_flag_usb=1;
	return 1;
}



