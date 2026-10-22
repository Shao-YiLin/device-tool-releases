/*
 * @Descripttion: 
 * @version: 
 * @Author: xdhuang
 * @Date: 2021-05-20 13:31:09
 * @LastEditors: xdhuang
 * @LastEditTime: 2021-05-28 10:24:07
 * @Copyright: Hefei Madao Information Technology Co., Ltd
 */
#include "common/types.h"
int open_the_website();
int madao_interface_report(u8 *data, u8 len);
int usb_keboard_consumer_report(u8 *data, u8 len);
int usb_keyboard_hid_report(u8 *data,u8 len);
int usb_mouse_hid_report(u8 *data, u8 len);
void usb_report_web_addr();
