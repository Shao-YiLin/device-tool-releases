/*
 * @Author: xuhan
 * @Date: 2020-02-07 14:47:40
 * @LastEditors  : xuhan
 * @LastEditTime : 2020-02-08 17:55:42
 * @Description: 
 */
#ifndef _LIBPROTOCOL_LOGIC_DEFINE_H_
#define _LIBPROTOCOL_LOGIC_DEFINE_H_

typedef void(*LOGIC_ACTION_CALLBACK)(void *param, unsigned char cmd, int mode, int channel, char *data, int len);

typedef void(*DEVICE_WRITE_CALLBACK)(void *param, unsigned char *data, int size);

enum ERR_CODE
{
    ERR_FAIL = 0,
    ERR_SUCCESS = 1
};

enum MouseType
{
	KEY_DOWN = 1,
	KEY_UP,
	START_RECORD,
	STOP_RECORD,
	RECORD_DATA,
	SET_DPI,
	GET_DPI,
	DPI_DATA,
	BATTER,
	BLE_STATUS,
	VERSION_MOURSE_DATA,
	VERSION_DONGLE_DATA,
	MAC_MOURSE_DATA,
	MAC_DONGLE_DATA,
	GET_BATTERY,
	HANDSHAKE,
	RECORD_CHANNEL,
	PRODUCT_NO,
	DONGLE_PRODUCT_NO,
	DEVICE_FINGER,
	CLIENT_FINGER,
	PROTOCOL_VERSION,
	AUTHENTICATE,
	IFLY_AUTHENTICATE,
	HOTPLUG
};

enum PROTOCOL_AUTHENTICATE_PHASE
{
	PHASE1 = 0x1,
	PHASE2 = 0x2,
	PHASE3 = 0x3,
	PHASE4 = 0x4
};


#endif