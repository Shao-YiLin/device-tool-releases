/*
 * @Author: xuhan
 * @Date: 2020-02-08 11:32:11
 * @LastEditors  : xuhan
 * @LastEditTime : 2020-02-08 18:28:31
 * @Description:
 */
#ifndef _LIB_DEVICE_DEFINE_H_
#define _LIB_DEVICE_DEFINE_H_

#include <stdint.h>

typedef enum
{
	LIBMOUSE_EVENT_LEFT = 0x1,
	LIBMOUSE_EVENT_ARRIVED,
	LIBMOUSE_EVENT_BLE_CONNECTED,
	LIBMOUSE_EVENT_BLE_DISCONNECTED,
	LIBMOUSE_EVENT_SINCHECK,
	LIBMOUSE_EVENT_KEY_DOWN,
	LIBMOUSE_EVENT_KEY_UP,
	LIBMOUSE_EVENT_LASER,
	LIBMOUSE_EVENT_HEART_HEAT = 0x09
} libmouse_event_type;

typedef enum
{
	LIBMOUSE_ACTION_BATTERY = 0x11,
	LIBMOUSE_ACTION_DPI,
	LIBMOUSE_ACTION_SET_DPI,
	LIBMOUSE_ACTION_HANDSHAKE,
	LIBMOUSE_ACTION_START_RECORD,
	LIBMOUSE_ACTION_STOP_RECORD,
	LIBMOUSE_ACTION_AUTHENTICATION,
	LIBMOUSE_ACTION_LED_RGB,
	LIBMOUSE_ACTION_VIBRATION,
	LIBMOUSE_ACTION_CUSTOM_KEY,
	LIBMOUSE_ACTION_SINCHECK,
	LIBMOUSE_ACTION_START_OTA,
	LIBMOUSE_ACTION_UPDATE_OTA,
	LIBMOUSE_ACTION_END_OTA,
	LIBMOUSE_ACTION_SET_POLLING_RATE,
	LIBMOUSE_ACTION_GET_POLLING_RATE = 0x40,
	LIBMOUSE_ACTION_SET_LED_INTENCITY,
	LIBMOUSE_ACTION_SET_RGB_BACKLIGHT,
	LIBMOUSE_ACTION_SET_LASER,
	LIBMOUSE_ACTION_SET_WHEEL = 0x44,
	LIBMOUSE_ACTION_GET_WHEEL = 0x45,
	LIBMOUSE_ACTION_PlAYER_PLAY = 0x46,
	LIBMOUSE_ACTION_SET_PLAYER_VOLUME = 0x47,
	LIBMOUSE_ACTION_SET_TIME_HEART_BEAT = 0x48,
	LIBMOUSE_ACTION_SET_KEYBOARD_CUSTOM_RGB = 0x49
} libmouse_action_type;

typedef enum
{
	LIBMOUSE_INFO_MOUSE_VERSION = 0x21,
	LIBMOUSE_INFO_MOUSE_MACADDR,
	LIBMOUSE_INFO_DONGLE_VERSION,
	LIBMOUSE_INFO_DONGLE_MACADDR,
	LIBMOUSE_INFO_MOUSE_PRODUCTNO,
	LIBMOUSE_INFO_PROTOCOL_VERSION,
	LIBMOUSE_INFO_DONGLE_PRODUCTNO,
	LIBMOUSE_INFO_MOUSE_THEME,
	LIBMOUSE_INFO_MOUSE_FLIP_PAGE,
	LIBMOUSE_INFO_MOUSE_SUBTITLE_STATUS,
	LIBMOUSE_INFO_MOUSE_WORK_MODE,
	LIBMOUSE_INFO_MOUSE_CUSTOM_KEY,
	LIBMOUSE_INFO_MOUSE_CONNECTION_TYPE,
	LIBMOUSE_INFO_GET_LED_INTENCITY,
	LIBMOUSE_INFO_GET_RGB_BACKLIGHT,
	LIBMOUSE_INFO_SLAVE_DEVICE_VERSION = 0x30,
	LIBMOUSE_INFO_GET_PLAYER_VOLUME,
	LIBMOUSE_INFO_GET_TIME_HEART_BEAT
} libmouse_info_type;

typedef enum
{
	LIBMOUSE_DEVICE_TYPE_MOUSE = 0x31,
	LIBMOUSE_DEVICE_TYPE_KEYBOARD = 0x32
}libmouse_device_type;

struct libmouse_device_info
{
	unsigned short vendor_id;
	unsigned short product_id;
	unsigned char device_address;
	unsigned char device_type;
};

struct libmouse_init_param {
	struct libmouse_device_info device;
	void(*usb_cb)(uint8_t* data, uint32_t len);
};

typedef void(*libmouse_write_cb_fn)(void * param, unsigned char *data, int size);

/**
 * @brief  音频数据回调函数
 * @buffer: pcm音频数据
 * @len: 音频数据长度
 */
typedef void(*libmouse_audio_cb_fn)(void * param, unsigned char type, unsigned char channel, unsigned short *buffer, unsigned int len);

/**
 * @brief  鼠标状态回调
 * @state:  1:鼠标dongle拔出
 *          2:鼠标dongle插入　
 *          3:鼠标和dongle连接　
 *          4:鼠标和dongle断开　
 *          5:语音按键按下
 *          6:语音按键释放
 * @retval
 */
typedef void(*libmouse_event_cb_fn)(void * param, libmouse_event_type state, unsigned int value);

/**
 * @brief  鼠标的命令反馈回调
 * @action: 0x11:鼠标电量，value：鼠标电量的值
 *          0x12:鼠标dpi, value: 鼠标dpi等级（０～４）
 *          0x13:握手反馈，value: 1 成功，０ 失败
 *          0x14:开始录音，value: 1 成功，０ 失败
 *          0x15:停止录音，value: 1 成功，０ 失败
 *          0x16:鉴权，value: 1 成功，０ 失败
 */
typedef void(*libmouse_action_cb_fn)(void * param, libmouse_action_type action, unsigned int value);

/**
 * @brief  鼠标扩展命令反馈回调
 * @info: 0x21:鼠标版本号 （4字节的版本号）
 *          0x22:鼠标mac地址（4字节的版本号）
 *          0x23:dongle版本号（4字节的版本号）
 *          0x24:dongle的mac地址（4字节的版本号）
 *          0X25:设备产品系列（4字节的版本号）
 *              1) 0x22:复刻版
*				2) 0x23:Pro版
*				3) 0x24:Lite版
*           0x26:设备协议版本号 （4字节的版本号）
*           0x27:dongle产品系列
*               1) 0x10:通用
*				2) 0x11:键鼠
*				3) 0x12:鼠标
*				4) 0x13:键盘
*				5) 0x14:无线麦克风
*				6) 0x15:录音笔
 * @retval
 */
typedef void(*libmouse_info_cb_fn)(void * param, libmouse_info_type info, unsigned char *buffer, unsigned int len);

#endif