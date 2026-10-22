// Test.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <string.h>
#include <Windows.h>

#include <stdio.h>
#include "MadaoMessage.h"
#include <map>
#include <list>
#include "mspeech_pcm.h"
#include <time.h>
ICONINFO iconInfo = { 0 };
HBITMAP oldBitmap;
HCURSOR newCursor;
HBITMAP newBitmap;
HCURSOR cursor;
HDC memDC;
void curso_init()
{
	// 获取当前光标的句柄
	CURSORINFO cursorInfo = { 0 };
	cursorInfo.cbSize = sizeof(cursorInfo);
	GetCursorInfo(&cursorInfo);
	cursor = cursorInfo.hCursor;

	// 定义光标颜色，RGB 值为红色
	COLORREF cursorColor = RGB(255, 0, 0);

	// 创建光标信息结构体
	GetIconInfo(cursor, &iconInfo);

	// 备份原来的光标资源
	oldBitmap = iconInfo.hbmColor;

	// 创建位图信息结构体
	BITMAP bmInfo = { 0 };
	GetObject(iconInfo.hbmColor, sizeof(bmInfo), &bmInfo);

	// 创建位图 DC
	memDC = CreateCompatibleDC(NULL);

	// 创建新的位图
	newBitmap = CreateBitmap(bmInfo.bmWidth, bmInfo.bmHeight, bmInfo.bmPlanes, bmInfo.bmBitsPixel, NULL);

	// 选择新位图到位图 DC 中
	HGDIOBJ oldBitmap2 = SelectObject(memDC, newBitmap);

	// 使用指定颜色填充新位图
	RECT rect = { 0 };
	rect.right = bmInfo.bmWidth;
	rect.bottom = bmInfo.bmHeight;
	HBRUSH brush = CreateSolidBrush(cursorColor);
	SelectObject(memDC, brush);
	Ellipse(memDC, 0, 0, bmInfo.bmWidth, bmInfo.bmHeight);
	DeleteObject(brush);

	// 复制旧位图的像素到新位图中，只保留颜色位
	HBITMAP hOldBitmap = (HBITMAP)SelectObject(memDC, iconInfo.hbmColor);
	BitBlt(memDC, 0, 0, bmInfo.bmWidth, bmInfo.bmHeight, memDC, 0, 0, SRCCOPY);

	// 恢复旧位图 DC 和位图对象
	SelectObject(memDC, hOldBitmap);

	// 修改光标信息
	iconInfo.hbmColor = newBitmap;
}




static void onOtherAction(struct mspeech_framework *framework);
#define PART_OF_OPUS_DATA 40


typedef enum
{
	PCM_1_CHANNEL,
	PCM_2_CHANNEL,
	PCM_4_CHANNEL
} FILE_PCM_TYPE;

FILE *fileChannelLeft = NULL;
FILE *fileChannelRight = NULL;
FILE *fileChannel = NULL;

#define channel_left "channel_left.pcm"
#define channel_right "channel_right.pcm"
#define channel_pcm "record.pcm"

#define SIN_CHECK_PCM "sincheck.pcm"

static FILE_PCM_TYPE current_pcm_type = PCM_1_CHANNEL;


#define PCM_FILE "pcm_file.pcm"
FILE *filePcm = NULL;
static int totalSize = 0;

struct mspeech_pcm pcm[2];

static void writePcm(unsigned char *data, int len) {

	if (NULL == filePcm) {
		totalSize = 0;
		fopen_s(&filePcm, PCM_FILE, "wb");
	}

	if (NULL != filePcm)
	{
		totalSize += len;
		printf("fileSize:%d\r\n", totalSize);
		fwrite(data, 1, len, filePcm);
		fflush(filePcm);
	}
}

int getNowTime(char *nowTime)
{
	char acYear[5] = { 0 };
	char acMonth[5] = { 0 };
	char acDay[5] = { 0 };
	char acHour[5] = { 0 };
	char acMin[5] = { 0 };
	char acSec[5] = { 0 };

	time_t now;
	struct tm* timenow;

	time(&now);
	timenow = localtime(&now);

	strftime(acYear, sizeof(acYear), "%Y", timenow);
	strftime(acMonth, sizeof(acMonth), "%m", timenow);
	strftime(acDay, sizeof(acDay), "%d", timenow);
	strftime(acHour, sizeof(acHour), "%H", timenow);
	strftime(acMin, sizeof(acMin), "%M", timenow);
	strftime(acSec, sizeof(acSec), "%S", timenow);

	strncat(nowTime, acYear, 4);
	strncat(nowTime, acMonth, 2);
	strncat(nowTime, acDay, 2);
	strncat(nowTime, acHour, 2);
	strncat(nowTime, acMin, 2);
	strncat(nowTime, acSec, 2);
	strncat(nowTime, ".pcm", 4);

	return 0;
}

static void onResetRecord()
{

	if (NULL != fileChannel)
		remove(channel_pcm);
	fileChannel = NULL;
	char data[36] = { 0 };
	getNowTime(data);
	fopen_s(&fileChannel, data, "wb");

	//if (NULL != fileChannelLeft)
	//{
	//	remove(channel_left);
	//}
	//fopen_s(&fileChannelLeft, channel_left, "wb");

	//if (NULL != fileChannelRight) {
	//	remove(channel_right);
	//}

	//fileChannelLeft = NULL;
	//fileChannelRight = NULL;

	//fopen_s(&fileChannelRight, channel_right, "wb");
}

static unsigned short channelleft_buffer[320] = { 0 };
static unsigned short channelright_buffer[320] = { 0 };
static bool bChannelLeftFlag = false;

static void onSaveRecordData(int mode, unsigned short *data, int len)
{

	/*if (mode == 0 && NULL != fileChannelLeft) {

		fwrite(data, 1, len, fileChannelLeft);
		fflush(fileChannelLeft);
	}
	else if (mode == 1 && NULL != fileChannelRight) {
		fwrite(data, 1, len, fileChannelRight);
		fflush(fileChannelRight);
	}*/

	if (current_pcm_type == PCM_1_CHANNEL)
	{
		if (NULL != fileChannel && mode == 0)
		{
			fwrite(data, 1, len * 2, fileChannel);
			fflush(fileChannel);
		}
	}
	else
	{
		if (mode == 0)
		{
			memcpy(channelleft_buffer, data, sizeof(channelleft_buffer));
			bChannelLeftFlag = true;
		}
		else if (mode == 1)
		{
			memcpy(channelright_buffer, data, sizeof(channelright_buffer));
		}

		if (mode == 1 && bChannelLeftFlag)
		{
			if (current_pcm_type == PCM_2_CHANNEL)
			{
				unsigned short channel_buffer[640] = { 0 };
				for (int i = 0; i < len; i++)
				{
					channel_buffer[i * 2] = channelleft_buffer[i];
					channel_buffer[i * 2 + 1] = channelright_buffer[i];
				}
				if (NULL != fileChannel)
				{
					fwrite(channel_buffer, 1, sizeof(channel_buffer), fileChannel);
					fflush(fileChannel);
				}
			}
			else if (current_pcm_type == PCM_4_CHANNEL)
			{
				unsigned int channel_buffer[1280] = { 0 };
				for (int i = 0; i < len; i++)
				{
					channel_buffer[i * 4] = (channelleft_buffer[i] << 16) & 0xffff0000;
					channel_buffer[i * 4 + 1] = (channelright_buffer[i] << 16) & 0xffff0000;
					channel_buffer[i * 4 + 2] = 0;
					channel_buffer[i * 4 + 3] = 0;
				}
				if (NULL != fileChannel)
				{
					fwrite(channel_buffer, 1, sizeof(channel_buffer), fileChannel);
					fflush(fileChannel);
				}

				if (NULL != fileChannelLeft)
				{
					fwrite(channelleft_buffer, 1, sizeof(channelleft_buffer), fileChannelLeft);
					fflush(fileChannelLeft);
				}
			}
			bChannelLeftFlag = false;
		}
	}
}

static void onCloseRecord()
{
	/*
	if (NULL != fileChannelRight) {
		fclose(fileChannelRight);
	}*/

	if (NULL != fileChannel)
	{
		fclose(fileChannel);
		fileChannel = NULL;
	}

	//if (NULL != fileChannelLeft)
	//{
	//	fclose(fileChannelLeft);
	//	fileChannelLeft = NULL;
	//}
}

static void __SleepSec(int sec)
{
	Sleep(sec * 1000);
}

static void printDex(unsigned char *data, int len)
{
	if (len == 0)
		return;

	for (int i = 0; i < len; i++)
	{
		printf("%02x ", *(data + i) & 0xff);
	}
	printf("\n");
}

#include <time.h>
#include <chrono>
#include <iostream> // std::cout
using namespace std;

static time_t getTimeStamp()
{
	std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> tp = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()); //获取当前时间点
	time_t timestamp = tp.time_since_epoch().count();																															  //计算距离1970-1-1,00:00的时间长度
	return timestamp;
}

/////////////////////////////////////////////////////////
#include "include/mspeech_usb.h"
#include "include/mspeech_framework.h"
#include "include/mspeech_pcm.h"
#include "include/mspeech_spp.h"
#include "include/mspeech_usb_manager.h"

static struct mspeech_spp mSppHandle;

// static struct mspeech_usb_manager multi_usb;

struct pcm_engine
{
	struct mspeech_pcm left;
	struct mspeech_pcm right;
};

#define USB_VID 0x2480
#define USB_PRODUCT_ID 0x6698

#define MAX_DEVICE_SIZE 4 
#define MAX_SCAN_DEVICE_SIZE 16

static struct mspeech_device_info device_array[MAX_DEVICE_SIZE] = { 0 };
static int device_size = 0;
//static struct mspeech_usb usb_array[MAX_DEVICE_SIZE] = {0};
//static struct mspeech_framework framework_array[MAX_DEVICE_SIZE] = {0};
//static bool framework_flag = false;
//static bool multi_usb_framework_flag = false;
static struct pcm_engine engine_array[MAX_DEVICE_SIZE] = { 0 };
static void *pLoopThread = NULL;
static MadaoMessage *mMadaoMsgHandle;

struct DeviceBindManager {

	struct mspeech_usb usb;
	struct mspeech_usb_manager usb_manager;
	struct mspeech_framework framework_array[MAX_DEVICE_SIZE];
	unsigned char device_type;
	unsigned char device_addr;
	bool framework_flag;
};

static struct DeviceBindManager deviceBindManagerArray[MAX_DEVICE_SIZE];

static struct DeviceBindManager *getDeviceBindFramework(struct mspeech_framework *framework) {
	for (int i = 0; i < MAX_DEVICE_SIZE; i++) {
		for (int j = 0; j < MAX_DEVICE_SIZE; j++) {
			struct mspeech_framework *p = &deviceBindManagerArray[i].framework_array[j];
			if (p == framework)
				return &deviceBindManagerArray[i];
		}
	}
	return NULL;
}

static struct DeviceBindManager *getDeviceBindByUsbManager(struct mspeech_usb_manager *usb_manager) {
	for (int i = 0; i < MAX_DEVICE_SIZE; i++) {
		struct mspeech_usb_manager *p = &deviceBindManagerArray[i].usb_manager;
		if (p == usb_manager)
			return &deviceBindManagerArray[i];
	}
	return NULL;
}

static struct DeviceBindManager *getDeviceBindByDeviceAddr(unsigned char device_addr) {
	for (int i = 0; i < MAX_DEVICE_SIZE; i++) {
		if (deviceBindManagerArray[i].device_addr == device_addr) {
			return &deviceBindManagerArray[i];
		}
	}
	return NULL;
}

struct scan_device_info
{
	unsigned char rssi;
	unsigned char mac[6];
	unsigned char name[24];
	unsigned short size;
};

static struct scan_device_info scan_array[MAX_SCAN_DEVICE_SIZE] = { 0 };

static bool is_have_in_scan_array(unsigned char *mac)
{
	for (int i = 0; i < MAX_SCAN_DEVICE_SIZE; i++)
	{
		if (memcmp(scan_array[i].mac, mac, 6) == 0)
			return true;
	}

	return false;
}

static bool push_to_scan_array(unsigned char rssi, unsigned char *mac, unsigned char *name, unsigned int len) {

	if (NULL == mac)
		return false;

	if (is_have_in_scan_array(mac)) {
		//	printf("scan device is exist \n");
		return false;
	}


	for (int i = 0; i < MAX_SCAN_DEVICE_SIZE; i++)
	{
		if (scan_array[i].size == 0) {
			scan_array[i].rssi = rssi;
			memcpy(scan_array[i].mac, mac, 6);
			memcpy(scan_array[i].name, name, len);
			scan_array[i].size = len;
			return true;
		}
	}
	return false;
}

//static void onDeviceScanThread();

static void _addFrameworkMsg(unsigned char msgId)
{
	mMadaoMsgHandle->send(&msgId, 1);
}

static void _MSPEECH_RECEIVE_CALLBACK(void *param, unsigned char *data, unsigned int len)
{
	printf("receiver:");
	printDex(data, len);

	//printf("receiver_data: %d \n", len);
	/*if(len == 63 || len == 10)*/
	//if (len == 40) {
	//	writePcm(data, len);
	//	return;
	//}
	//
	//if (len == 53) {
	//	writePcm(data + 9, 40);
	//	return;
	//}


	// if(framework_flag)

	struct mspeech_device_info *device = (struct mspeech_device_info *)param;
	if (NULL == device)
		return;
	//	printf("receiver_data: %d, %d \n", device->device_type, device->device_address);
	struct DeviceBindManager *bindManager = getDeviceBindByDeviceAddr(device->device_address);
	if (NULL != bindManager && bindManager->framework_flag) {
		if (device->device_type == 2) {
			bindManager->usb_manager.append(&bindManager->usb_manager, data, len);
		}
		else /*if (device->device_type == 1) */ {
			//bindManager->framework_array[0].append_data(&bindManager->framework_array[0], data, len);
			bindManager->usb_manager.append(&bindManager->usb_manager, data, len);
		}
	}
}

static void _mspeech_framework_write_cb_fn(void *param, unsigned char *data, int size)
{
	printf("write_data:");
	printDex(data, size);
	if (NULL != param) {
		struct mspeech_framework *framework = (struct mspeech_framework *)param;
		struct DeviceBindManager *bindManager = getDeviceBindFramework(framework);
		if (NULL != bindManager) {

			//	printf("write_data: %d,%d\n", bindManager->device_type, bindManager->device_addr);

			if (bindManager->device_type == 1 || bindManager->device_type == 3) {
				//	printf("singleDongle \r\n");
				int ret = bindManager->usb.write(&bindManager->usb, data, size);
				//	printf("usb_write_state:%d\r\n", ret);
			}
			else if (bindManager->device_type == 2) {
				if (framework->mac != NULL) {
					bindManager->usb_manager.convert_to_usb_data(&bindManager->usb_manager, framework->mac, data, size);
					/*if (len > 0) {
						printf("write_data:");
						printDex(buffer, len);
						bindManager->usb.write(&bindManager->usb, buffer, len);
					}*/
				}
			}
		}
	}
	else {
		printf("param is null \n");
	}
}

static void _mspeech_framework_audio_cb_fn(void *param, unsigned char type, unsigned char channel, unsigned short *buffer, unsigned int len)
{
	struct mspeech_framework *framework = (struct mspeech_framework *)param;
	if (NULL == framework)
		return;
	unsigned short out[640] = { 0 };
	unsigned short out_size = 0;
	if ((len % PART_OF_OPUS_DATA) == 0) {
		unsigned int i = 0;
		while (i < (len / PART_OF_OPUS_DATA)) {
			if (pcm[type - 2].convert2pcm(&pcm[type - 2], buffer + (20 * i), out, &out_size) > 0) {
				printf("decode:%d,%d\r\n", out_size, type);
				onSaveRecordData(0, out, out_size);
				i++;
			}
		}

	}
}

static void onFrameworkHandShake(struct mspeech_framework *framework);

static time_t timeTemp = 0;
static int pc_down = 0;
static int pc_up = 0;
static int pc_temp = 0;
static char start_ota_flag = 0;
static char start_record_flag = 0;

static void _mspeech_framework_event_cb_fn(void *param, mspeech_framework_event_type state, unsigned int value)
{
	struct mspeech_framework *framework = (struct mspeech_framework *)param;
	if (NULL == framework)
		return;

	//printf("event_cb : %d \n", state);
	switch (state)
	{
	case MSPEECH_FRAMEWORK_EVENT_KEY_DOWN:
		//printf("------------------------------\n");
		//timeTemp = getTimeStamp();
		//printf("key down: %d \n", ++pc_down);
		printf("key down index: %d \n", value);
		//_addFrameworkMsg(state);

		if (value == 0 && start_ota_flag == 0) {
			framework->start_record(framework);
			start_record_flag = 1;
		}
		else if (value == 1) {
			//	printf("keydown value:%d\r\n", value);

		}
		else if (value == 2) {
			//framework->get_dpi(framework);
		}
		break;
	case MSPEECH_FRAMEWORK_EVENT_KEY_UP:
	{
		//pc_temp++;
		//if (pc_temp % 2 == 0) {

		//	}
		//
		//_addFrameworkMsg(state);
		if (value == 0) {
			framework->stop_record(framework);
			start_record_flag = 0;
		}
		else if (value == 1) {
			framework->get_theme(framework);
			//	printf("keyup value:%d\r\n", value);
		}
		else if (value == 2) {
			//framework->get_dpi(framework);
		}
		//printf("key up: %d \n", ++pc_up);
		printf("key up index: %d \n", value);
		printf("------------------------------\n");
	}
	break;
	case MSPEECH_FRAMEWORK_EVENT_BLE_CONNECTED:
		printf("ble connected \n");
		onFrameworkHandShake(framework);
		break;
	case MSPEECH_FRAMEWORK_EVENT_BLE_DISCONNECTED:
		printf("ble disconnected \n");
		break;
	case MSPEECH_FRAMEWORK_EVENT_SINCHECK:
		printf("sincheck:%d \n", value);
		break;	
	case MSPEECH_FRAMEWORK_EVENT_LASER:
		if (value < 3)
		{
			printf("激光模式:%d \n", value);

		}

		else if (value == 4)
		{
			printf("数字激光工作\n");

			curso_init();
			// 创建新的光标
			newCursor = CreateIconIndirect(&iconInfo);	

			// 设置新的光标
			SetSystemCursor(newCursor, 32512);
	

		}
	
		else if (value == 5)
		{

			printf("数字激光停止工作\n");
			// 将光标恢复
			iconInfo.hbmColor = oldBitmap;
			newCursor = CreateIconIndirect(&iconInfo);
			SetSystemCursor(newCursor, 32512);

			// 释放资源
			DeleteObject(iconInfo.hbmMask);
			DeleteObject(oldBitmap);
			DeleteObject(newBitmap);
			DestroyCursor(cursor);
			DestroyCursor(newCursor);
			DeleteDC(memDC);

		}

		break;
	case MSPEECH_FRAMEWORK_EVENT_EVENT_HEART:
		printf("heart:%d \n", value);
	default:
		break;
	}
}


struct frimware_data_param {
	unsigned char *total_firmware_data;
	unsigned long total_firmware_size;
	unsigned long firmware_index;
};
static frimware_data_param frimware_data_info;

static void __printf_firmware()
{
	printf("firmware:");
	for (unsigned int i = 0; i < frimware_data_info.total_firmware_size; i++)
	{
		printf("%2x ", *(frimware_data_info.total_firmware_data + i));
		if (((i + 1) % 16) == 0) {
			printf("\n");
		}
	}
}

static unsigned char __read_part_firmware(unsigned char *out_data, unsigned long size) {
	if (frimware_data_info.total_firmware_data == NULL || size != 32 || out_data == NULL || frimware_data_info.total_firmware_size < frimware_data_info.firmware_index) {
		if (frimware_data_info.total_firmware_data != NULL) {
			free(frimware_data_info.total_firmware_data);
			frimware_data_info.total_firmware_data = NULL;
		}
		frimware_data_info.total_firmware_size = 0;
		return 0;
	}
	if (frimware_data_info.total_firmware_size == frimware_data_info.firmware_index) {
		if (frimware_data_info.total_firmware_data != NULL) {
			free(frimware_data_info.total_firmware_data);
			frimware_data_info.total_firmware_data = NULL;
		}
		frimware_data_info.total_firmware_size = 0;
		return 2;
	}
	if (frimware_data_info.total_firmware_size < (frimware_data_info.firmware_index + size)) {
		memcpy(out_data, frimware_data_info.total_firmware_data + frimware_data_info.firmware_index, frimware_data_info.total_firmware_size - frimware_data_info.firmware_index);
		memset(out_data + (frimware_data_info.total_firmware_size - frimware_data_info.firmware_index), 0xff, size + frimware_data_info.firmware_index - frimware_data_info.total_firmware_size);
		frimware_data_info.firmware_index = frimware_data_info.total_firmware_size;
	}
	else if (frimware_data_info.total_firmware_size >= (frimware_data_info.firmware_index + size)) {
		memcpy(out_data, frimware_data_info.total_firmware_data + frimware_data_info.firmware_index, size);
		frimware_data_info.firmware_index += size;
	}

	return 1;
}

static unsigned char __read_byte_stream_file()
{
	printf("读取文件\n");
	FILE *fid;
	unsigned long num = 0;
	unsigned char read_file_en = 0;
	frimware_data_info.total_firmware_size = 0;
	frimware_data_info.firmware_index = 0;
	if (frimware_data_info.total_firmware_data != NULL)
	{
		free(frimware_data_info.total_firmware_data);
		frimware_data_info.total_firmware_data = NULL;
	}
	fopen_s(&fid, "PDongle_1.0.1.11_20230904_Ed.bin", "rb");
	if (fid == NULL)
	{
		printf("读取文件出错\n");
		return 0;
	}
	//获取文件大小
	fseek(fid, 0, SEEK_END);
	frimware_data_info.total_firmware_size = ftell(fid);
	rewind(fid);
	//开辟存储空间
	frimware_data_info.total_firmware_data = (unsigned char*)malloc(sizeof(unsigned char) * frimware_data_info.total_firmware_size);
	if (frimware_data_info.total_firmware_data == NULL)
	{
		printf("开辟空间出错\n");
		goto end_read;
	}
	fread(frimware_data_info.total_firmware_data, frimware_data_info.total_firmware_size, 1, fid);
	//读取firmware 0x18-0x1b 四字节确定firmware size
	//__printf_firmware();

	/*memcpy(&num, frimware_data_info.total_firmware_data + 0x18, 4);
	if (num != frimware_data_info.total_firmware_size)
	{
		free(frimware_data_info.total_firmware_data);
		frimware_data_info.total_firmware_data = NULL;
		printf("文件长度出错\n");
		goto end_read;
	}*/
	printf("firmware size:%d\n", frimware_data_info.total_firmware_size);
	fclose(fid);
	return 1;
end_read: fclose(fid);
	return 0;
}

static int pc_start = 0;
static int pc_stop = 0;

static int authentication_pc = 0;
static int handshakeFlag = 0;
static int success_flag = 0;
static int fail_flag = 0;
static void _mspeech_framework_action_cb_fn(void *param, mspeech_framework_action_type action, unsigned int value)
{
	struct mspeech_framework *framework = (struct mspeech_framework *)param;
	if (NULL == framework)
		return;

	//printf("action_event: %d \n", action);
	switch (action)
	{
	case MSPEECH_FRAMEWORK_ACTION_START_RECORD:
	{
		printf("开始录音: %d \n", ++pc_start);
		onResetRecord();
		//	time_t temp = getTimeStamp() - timeTemp;
		//	cout << "time:" << temp << endl;
	}
	break;
	case MSPEECH_FRAMEWORK_ACTION_STOP_RECORD:
		Sleep(2000);
		onCloseRecord();
		printf("结束录音:%d \n", ++pc_stop);
		break;
	case MSPEECH_FRAMEWORK_ACTION_BATTERY:
		//framework->get_battery(framework);
		printf("设备电量: %d \n", value);
		break;
	case MSPEECH_FRAMEWORK_ACTION_DPI:
		printf("设备dpi: %d \n", value);
		break;
	case MSPEECH_FRAMEWORK_ACTION_HANDSHAKE:
		printf("握手反馈：%d \n", value);
		handshakeFlag = value;
		if (handshakeFlag == 0)
		{
			fail_flag++;
			printf("fail_flag : %d\n\n", fail_flag);
			Sleep(500);
			//onFrameworkHandShake(framework);

		}

		break;
	case MSPEECH_FRAMEWORK_ACTION_AUTHENTICATION:
	{
		time_t temp = getTimeStamp() - timeTemp;
		cout << "time:" << temp << endl;
		printf("授权状态: %d \n", value);

		if (handshakeFlag)
		{
			onOtherAction(framework);
			//success_flag++;
			//printf("success_flag %d\n\n", success_flag);
			
		}
		else {
			//onFrameworkHandShake(framework);
		}
		if (value == 0)
		{
			authentication_pc++;
			//framework->set_handshake_info(framework, 1);
			printf("授权失败：%d \n", authentication_pc);
		}
		//Sleep(3000);
	}

	break;
	case MSPEECH_FRAMEWORK_ACTION_VIBRATION:
		printf("设置震动 \n");
		break;
	case MSPEECH_FRAMEWORK_ACTION_LED_RGB:
		printf("设置RGB \n");
		break;
	case MSPEECH_FRAMEWORK_ACTION_CUSTOM_KEY:
		printf("设置自定义按键 \n");
		break;
	case MSPEECH_FRAMEWORK_ACTION_SINCHECK:
		printf("产线指令: %d \n", value);
		break;
	case MSPEECH_FRAMEWORK_ACTION_START_OTA:
		printf("start ota:%d\n", value);
		break;
	case MSPEECH_FRAMEWORK_ACTION_UPDATE_OTA:
		printf("update ota:%d\n", value * 100 / frimware_data_info.total_firmware_size);
		break;
	case MSPEECH_FRAMEWORK_ACTION_END_OTA:
		printf("end ota_result:%d\n", value);
		break;
	case MSPEECH_FRAMEWORK_ACTION_SET_POLLING_DATE:
		printf("set polling:%d\n", value);
		break;
	case MSPEECH_FRAMEWORK_ACTION_GET_POLLING_DATE:
		printf("get polling:%d\n", value);
		break;
	case MSPEECH_FRAMEWORK_ACTION_SET_LED_INTENSITY:
		printf("set_led_intensity:%d\n", value); 
		break;
	case MSPEECH_FRAMEWORK_ACTION_SET_RGB_BACKLIHGHT:
		printf("set_rgb_backlihght:%d\n", value);
		break;
	case MSPEECH_FRAMEWORK_ACTION_SET_LASER:
		printf("set_laser:%d\n", value);
		break;
	case  MSPEECH_FRAMEWORK_ACTION_SET_WHEEL:
		if (value == 1)
		{
			printf("set wheel mode success   \n");
		}
		else
		{
			printf("set wheel mode fail   \n");
		}
		break;
	case MSPEECH_FRAMEWORK_ACTION_GET_WHEEL:

		if (value == 0)
		{
			printf("wheel mode ：音量\n");
		}
		else if (value == 1)
		{
			printf("wheel mode ：灯光\n");
		}
		
		break;
	case MSPEECH_FRAMEWORK_ACTION_SET_PLAYER_PLAY:
		printf("set player play : %d \n", value);
		break;
	case MSPEECH_FRAMEWORK_ACTION_SET_PLAYER_VOLUME:
		printf("set player volume : %d \n", value);
		break;
	case MSPEECH_FRAMEWORK_ACTION_SET_HEART_RATE_TIME:
		printf("set heart time : %d \n", value);
		break;
	case MSPEECH_FRAMEWORK_ACTION_SET_ADAPTIVE_MODULATION:
		printf("set adaptive modulation : %d \n", value);
		break;
	case MSPEECH_FRAMEWORK_ACTION_GET_ADAPTIVE_MODULATION:
		printf("get adaptive modulation: %d \n", value);
		break;
	default:
		break;
	}
}

static void _mspeech_framework_info_cb_fn(void *param, mspeech_framework_info_type info, unsigned char *data, unsigned int len)
{
	struct mspeech_framework *framework = (struct mspeech_framework *)param;
	if (NULL == framework)
		return;

	switch (info)
	{
	case MSPEECH_FRAMEWORK_INFO_MOUSE_PRODUCTNO:
	{
		//framework->get_device_productNo(framework);
		int productNo = 0;
		memcpy(&productNo, data, len);
		printf("productNO : %c%c%c%c\n", *(data), *(data + 1), *(data + 2), *(data + 3));
		//Sleep(20);
	}
	break;
	case MSPEECH_FRAMEWORK_INFO_MOUSE_VERSION:
	{
		int mouseMode = ((*data) & 0xf0) >> 4;
		int mouseHardMode = (*data) & 0x0f;
		//	printf("鼠标类型：%d ", mouseMode);
		printf("鼠标硬件版本：%d", mouseHardMode);
		printf("鼠标版本号：%d.%d.%d\n", *(data + 1), *(data + 2), *(data + 3));
	}
	break;
	case MSPEECH_FRAMEWORK_INFO_MOUSE_MACADDR:
		printf("鼠标MAC地址：%.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n", *(data + 5) & 0xff, *(data + 4) & 0xff, *(data + 3) & 0xff, *(data + 2) & 0xff, *(data + 1) & 0xff, *(data) & 0xff);
		break;
	case MSPEECH_FRAMEWORK_INFO_DONGLE_VERSION:
	{
		int dongleMode = ((*data) & 0xf0) >> 4;
		int dongleHardMode = (*data) & 0x0f;
		//	printf("DONGLE类型：%d ", dongleMode);
		printf("DONGLE硬件版本：%d", dongleHardMode);
		printf("DONGLE版本号：%d.%d.%d\n", *(data + 1), *(data + 2), *(data + 3));
	}
	break;
	case MSPEECH_FRAMEWORK_INFO_DONGLE_MACADDR:
		printf("DONGLEMAC地址：%.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n", *(data + 5) & 0xff, *(data + 4) & 0xff, *(data + 3) & 0xff, *(data + 2) & 0xff, *(data + 1) & 0xff, *(data) & 0xff);
		break;
	case MSPEECH_FRAMEWORK_INFO_PROTOCOL_VERSION:
		printf("协议版本号:");
		printDex(data, len);
		break;
	case MSPEECH_FRAMEWORK_INFO_DONGLE_PRODUCTNO:
		printf("Dongle系列:");
		printf("%s\n", data);
		break;
	case MSPEECH_FRAMEWORK_INFO_MOUSE_THEME:
		printf("theme:");
		printDex(data, len);
		break;
	case MSPEECH_FRAMEWORK_INFO_MOUSE_FLIP_PAGE:
		printf("flip page:");
		//printDex(data, len);
		unsigned int data_int;
		memcpy(&data_int, data, len);
		printf("%x\n", data_int);
		break;
	case MSPEECH_FRAMEWORK_INFO_MOUSE_SUBTITLE_STATUS:
		printf("subtitle:");
		printDex(data, len);
		break;
	case MSPEECH_FRAMEWORK_INFO_MOUSE_WORK_MODE:
		printf("work mode:");
		printDex(data, len);
		break;
	case MSPEECH_FRAMEWORK_INFO_MOUSE_CUSTOM_KEY:
		printf("custom key:");
		printDex(data, len);
		break;
	case MSPEECH_FRAMEWORK_INFO_GET_LED_INTENSITY:
	{
		int value = 0;
		memcpy(&value, data + 1, 1);
		printf("index: %d   value :%d", *data, value);
		break;
	}
	//case MSPEECH_FRAMEWORK_INFO_GET_RGB_BACKLIHGHT:
	//{
	//	int value = 0;
	//	memcpy(&value, data + 1, 1);
	//	if (*data == 0)
	//		printf("灯光类型 :%d\n", value);
	//	else if (*data == 1)
	//		printf("灯光亮度 :%d\n", value);
	//	else if (*data == 2)
	//		printf("灯光速度 :%d\n", value);
	//	break;
	//}
	case MSPEECH_FRAMEWORK_INFO_RGB_BACKLIGHT:
	{
		int value = 0;
		memcpy(&value, data + 1, 1);
		printf("灯光属性消息\n");
		if (*data == 0)
			printf("灯光类型 :%d\n",  value);
		else if (*data == 1)
			printf("灯光亮度 :%d\n", value);
		else if (*data == 2)
			printf("灯光速度 :%d\n", value);
		break;
	}
	case MSPEECH_FRAMEWORK_INFO_MOUSE_CONNECTION_TYPE:
		printf("连接类型:");
		printDex(data, len);
		break;
	case MSPEECH_FRAMEWORK_INFO_SLAVE_DEVICE_VERSION:
		printf("从机版本号：%d.%d\n", *(data), *(data + 1));
		break;

	case MSPEECH_FRAMEWORK_INFO_PLAYER_VOLUME:
	{
		int volume = 0;
		memcpy(&volume, data, len);
		cout << "音量大小:" << volume << endl;

	}

	break;
	case MSPEECH_FRAMEWORK_INFO_TIME_HEART_BEAT:
	{
		int time = 0;
		memcpy(&time, data, len);
		cout << "心跳时间:" << time << endl;
	}

	break;
	case MSPEECH_FRAMEWORK_INFO_LS_DEVICE_ID:
	{
		printf("DEVICE_ID：\n");
		printDex(data, len);
	}

	break;
	case MSPEECH_FRAMEWORK_INFO_LS_CHECK_SUM:
	{
		printf("CHECK_SUM：\n");
		printDex(data, len);
	}

	break;
	default:
		break;
	}
}

static void *pthread = NULL;

static DWORD WINAPI __usb_thread_func(LPVOID param)
{

	struct mspeech_usb *lm = (struct mspeech_usb *)param;
	if (NULL != lm && lm->handle)
	{
		lm->read(lm);
	}
	return 0;
}

static void _create_thread_usb_read(struct mspeech_usb *usb)
{
	if (NULL == usb)
		return;

	LPDWORD dwThreadId = NULL;
	pthread = CreateThread(NULL, 0, __usb_thread_func, usb, 0, dwThreadId);
}


static DWORD WINAPI __usbmanager_thread_func(LPVOID param)
{

	struct mspeech_usb_manager *lm = (struct mspeech_usb_manager *)param;
	if (NULL != lm)
	{
		lm->start_service(lm);
	}
	return 0;
}

static void _create_thread_usb_manager_read(struct mspeech_usb_manager *usb)
{
	if (NULL == usb)
		return;

	LPDWORD dwThreadId = NULL;
	void * pthread = CreateThread(NULL, 0, __usbmanager_thread_func, usb, 0, dwThreadId);
}

static DWORD WINAPI __usb_thread_func1(LPVOID param)
{
	struct mspeech_usb *lm = (struct mspeech_usb *)param;
	if (NULL != lm && lm->handle)
	{
		lm->read(lm);
	}
	return 0;
}

static void _create_thread_usb_read1(struct mspeech_usb *usb)
{
	if (NULL == usb)
		return;

	LPDWORD dwThreadId = NULL;
	CreateThread(NULL, 0, __usb_thread_func1, usb, 0, dwThreadId);
}

//static void _destory_thread_usb_read() {
//	if (pthread) {
//		TerminateThread(pthread);
//		pthread = NULL;
//	}
//}

static void onDestroy()
{
	//framework_flag = false;
	//usb_array[0].stop(&usb_array[0]);
	////_destory_thread_usb_read();
	//usb_array[0].uninit(&usb_array[0]);
	//engine_array[0].left.uninit(&engine_array[0].left);
	//engine_array[0].right.uninit(&engine_array[0].right);
	//framework_array[0].uninit(&framework_array[0]);
}

static void _MSPEECH_USB_REMOVE_CALLBACK(void *param)
{
	printf("usb removed \n");
	// onDestroy();

	//onDeviceScanThread();
}

static void setPcmType(FILE_PCM_TYPE type)
{
}

static void onOtherAction(struct mspeech_framework *framework)
{
	if (NULL == framework)
		return;

	int i = 0;
	framework->get_battery(framework);
	Sleep(100);
	framework->get_device_productNo(framework);
	Sleep(100);
	framework->get_device_version(framework);
	Sleep(20);
	framework->get_dongle_version(framework);
	Sleep(100);
	//framework->get_dpi(framework);
									
	Sleep(100);
		
		
		
		//framework->get_device_macaddr(framework);
		
		//framework->set_rgb_backlight_param(framework, 0, 16);//类型
		//Sleep(20);
		//framework->set_rgb_backlight_param(framework, 1,170 );//亮度
		//Sleep(20);
		//framework->set_rgb_backlight_param(framework, 2,3);//速度

		//framework->set_sleep_time(framework,600);//
		//Sleep(100);
		//framework->set_laser_mode(framework,1);
		//framework->get_laser_mode(framework);
		//framework->get_slave_device_version(framework);
		
	
		
	
	

	//framework->set_wheel_mode(framework,0);
	//framework->get_wheel_mode(framework);
	//framework->get_dpi(framework);
	//framework->start_sincheck(framework);
	//Sleep(20);

	//printf("version:%x\r\n", framework->get_version(framework));

	//framework->get_theme(framework);
	Sleep(20);
}

/**
 * @brief  查询是否已经存在
 * @note
 * @param  *mac:
 * @retval
 */
static int _is_have_create(struct DeviceBindManager *bindManager, unsigned char *mac)
{
	if (NULL == mac)
		return -1;

	for (int i = 0; i < MAX_DEVICE_SIZE; i++) {
		if (NULL != bindManager->framework_array[i].mac) {
			if (memcmp(bindManager->framework_array[i].mac, mac, 6) == 0) {
				return i;
			}
		}
	}
	return -1;
}

static DWORD WINAPI __framework_thread_func(void *param)
{

	if (NULL == param)
		return 0;

	struct mspeech_framework *framework = (struct mspeech_framework *)(param);
	framework->handshake(framework);
	Sleep(3000);
	
	
	
	
	
	



	

	return 1;
}

static void onFrameworkHandShake(struct mspeech_framework *framework)
{

	LPDWORD dwThreadId = NULL;
	pLoopThread = CreateThread(NULL, 0, __framework_thread_func, framework, 0, dwThreadId);
}

static bool __createCurrentFramework(struct mspeech_framework *framework, unsigned char *mac, char version) {

	if (NULL == framework)
		return true;

	printf("version:%d\n", version);

	if (framework->mac == NULL) {
		mspeech_framework_construct(framework);
		framework->init(framework,
			_mspeech_framework_write_cb_fn,
			_mspeech_framework_event_cb_fn,
			_mspeech_framework_action_cb_fn,
			_mspeech_framework_audio_cb_fn,
			_mspeech_framework_info_cb_fn, framework);
		framework->bind_mac(framework, mac);
		framework->set_protocol_version(framework, version);

		__SleepSec(2);
		onFrameworkHandShake(framework);
		return true;
	}
	return false;
}

static void ___createOneFramework(struct mspeech_usb_manager *usb, unsigned char *mac, char version) {
	if (NULL == mac)
		return;

	struct DeviceBindManager *bindManager = getDeviceBindByUsbManager(usb);
	if (NULL == bindManager)
		return;

	int dev_index = _is_have_create(bindManager, mac);
	if (dev_index != -1) {
		//onFrameworkHandShake(&bindManager->framework_array[dev_index]);
		printf("framework is exit \n");
		return;
	}

	for (int i = 0; i < MAX_DEVICE_SIZE; i++)
	{
		if (__createCurrentFramework(&(bindManager->framework_array[i]), mac, version))
			return;
	}
}

static void ___removeOneFramework(unsigned char *mac)
{
	if (NULL == mac)
		return;
	for (int j = 0; j < MAX_DEVICE_SIZE; j++)
	{
		for (int i = 0; i < MAX_DEVICE_SIZE; i++)
		{
			if (deviceBindManagerArray[j].framework_array[i].mac != NULL) {//feishu.xuei.cn
				if (memcmp(deviceBindManagerArray[j].framework_array[i].mac, mac, 6) == 0) {
					deviceBindManagerArray[j].framework_array[i].uninit(&deviceBindManagerArray[j].framework_array[i]);
					deviceBindManagerArray[j].framework_array[i].bind_mac(&deviceBindManagerArray[j].framework_array[i], NULL);
					printf("j=%d,i=%d", j, i);
					// remove from UI show
					return;
				}
			}
		}
	}
}

static struct mspeech_framework *get_framework_by_mac(struct DeviceBindManager *bindManager, unsigned char *mac) {

	if (NULL == bindManager)
		return NULL;

	for (int i = 0; i < MAX_DEVICE_SIZE; i++)
	{
		if (bindManager->framework_array[i].mac != NULL) {
			if (memcmp(bindManager->framework_array[i].mac, mac, 6) == 0)
				return &bindManager->framework_array[i];
			// add to UI show
		}
	}

	return NULL;
}

static void __write_data_resp(struct mspeech_usb_manager *usb, unsigned char *data, unsigned int size)
{
	if (size < 7)
		return;

	struct mspeech_usb_write_data write;
	usb->get_usb_write_data(usb, data, size, &write);
	struct DeviceBindManager *bindManager = getDeviceBindByUsbManager(usb);
	if (NULL == bindManager)
		return;
	struct mspeech_framework *framework = get_framework_by_mac(bindManager, write.mac);
	if (NULL != framework) {
		framework->append_data(framework, write.data, write.len);
	}
}

static void _append_framework(struct mspeech_usb_manager *usb, unsigned char *data, unsigned int size)
{
	struct DeviceBindManager *bindManager = getDeviceBindByUsbManager(usb);
	if (NULL == bindManager)
		return;
	bindManager->framework_array[0].append_data(bindManager->framework_array, data, size);
}

void _protocol_callback(void *param, unsigned char type, unsigned char *data, unsigned int size)
{
	if (param == NULL)
		return;
	struct mspeech_usb_manager * manager = (struct mspeech_usb_manager *)param;
	unsigned char name[128] = { 0 };
	struct mspeech_usb_event_data event;
	switch (type)
	{
	case MSPEECH_USB_MANAGERE_EVENT_TRANSPOND:
		__write_data_resp(manager, data, size);
		break;
	case MSPEECH_USB_MANAGERE_EVENT_SCAN:
		struct mspeech_usb_scan_data scan;
		manager->get_usb_scan_data(manager, data, size, &scan);
		memcpy(name, scan.data, scan.len);
		printf("scanDevice: %d, %s\n", scan.rssi, name);
		//manager->connect_device(manager, scan.mac);
		break;
	case MSPEECH_USB_MANAGERE_EVENT_CONNECTED:
		manager->get_usb_event_data(manager, data, size, &event);
		memcpy(name, event.data, event.len);
		if (event.state == 1) {
			printf("connectedDevice: %s\n", name);
			//	printDex(data, size);
			___createOneFramework(manager, event.mac, event.version);
		}
		else if (event.state == 0) {
			printf("bondedDevice: %s\n", name);
		}
		else if (event.state == 2) {
			printf("con_error: %s\n", name);
		}
		else
		{
			printf("con_unknown: %s\n", name);
		}
		break;
	case MSPEECH_USB_MANAGERE_EVENT_DISCONNECT:
		//struct mspeech_usb_event_data event;
		manager->get_usb_event_data(manager, data, size, &event);
		memcpy(name, event.data, event.len);
		//___removeOneFramework(event.mac);
		printf("disconnectedDevice: %s\n", name);
		break;
	case MSPEECH_USB_MANAGERE_EVENT_START_OTA:
		printf("start_usb_ota:%d\n", *data);
		break;
	case MSPEECH_USB_MANAGERE_EVENT_UPDATE_OTA:
	{
		unsigned long value = 0;
		memcpy(&value, data, size);
		printf("update ota:%d\n", value * 100 / frimware_data_info.total_firmware_size);
		break;
	}
	case MSPEECH_USB_MANAGERE_EVENT_END_OTA:
		printf("end_ota:%d\n", *data);
		break;
	case MSPEECH_USB_MANAGERE_EVENT_PRODUCT_NO:
		printf("master_product_no:%s\n", data);
		printDex(data, size);
		break;
	case MSPEECH_USB_MANAGERE_EVENT_APPEND:
		_append_framework(manager, data, size);
		break;
	default:
		break;
	}
}

void _write_callback(void *param, unsigned char *data, unsigned int size)
{
	if (NULL == data || size == 0)
		return;

	//printf("usb write data:");
	//printDex(data, size);

	if (NULL == param)
		return;

	struct DeviceBindManager *bindManager = getDeviceBindByUsbManager((struct mspeech_usb_manager *)param);
	if (NULL == bindManager)
		return;

	int ret = bindManager->usb.write(&bindManager->usb, data, size);
	//	printf("usbmanager write: %d \n", ret);
}

static void _MSPEECH_USB_SCAN_CALLBACK(struct mspeech_device_info *data, int size)
{

	printf("scan device size: %d \n", size);

	device_size = size;
	int i;
	for (i = 0; i < size; i++)
	{
		memcpy(&device_array[i], data + i, sizeof(struct mspeech_device_info));
		printf("device info: pid=%04x vid=%04x address= %d type= %d\n", device_array[i].product_id, device_array[i].vendor_id, device_array[i].device_address, device_array[i].device_type);
	}
}


static void onTestMspeechUsb()
{

	device_size = 0;
	//framework_flag = false;
	mspeech_usb_scan(_MSPEECH_USB_SCAN_CALLBACK, USB_VID, USB_PRODUCT_ID);
	//mspeech_usb_scan(_MSPEECH_USB_SCAN_CALLBACK, 0X635, 0X1000);

	mspeech_pcm_construct(&pcm[0]);
	pcm[0].init(&pcm[0], MSPEECH_ENCODE_ICO, 1);
	mspeech_pcm_construct(&pcm[1]);
	pcm[1].init(&pcm[1], MSPEECH_ENCODE_OPUS, 1);
	//onResetRecord();
	curso_init();
	for (int i = 0; i < device_size; i++)
	{
		mspeech_usb_construct(&(deviceBindManagerArray[i].usb));
		int ret = deviceBindManagerArray[i].usb.init(&deviceBindManagerArray[i].usb, &device_array[i], _MSPEECH_RECEIVE_CALLBACK, _MSPEECH_USB_REMOVE_CALLBACK);
		deviceBindManagerArray[i].device_addr = device_array[i].device_address;
		deviceBindManagerArray[i].device_type = device_array[i].device_type;
		printf("usb init : %d \n", ret);
		_create_thread_usb_read(&deviceBindManagerArray[i].usb);
		__SleepSec(2);
		printf("deviceType: %d \r\n", device_array[i].device_type);
		if (device_array[i].device_type == 1 || device_array[i].device_type == 3) {
			mspeech_framework_construct(&deviceBindManagerArray[i].framework_array[0]);
			deviceBindManagerArray[i].framework_array[0].init(&deviceBindManagerArray[i].framework_array[0],
				_mspeech_framework_write_cb_fn,
				_mspeech_framework_event_cb_fn,
				_mspeech_framework_action_cb_fn,
				_mspeech_framework_audio_cb_fn,
				_mspeech_framework_info_cb_fn, &deviceBindManagerArray[i].framework_array[0]);
			__SleepSec(1);
			timeTemp = getTimeStamp();
			printf("start:%lld\r\n", timeTemp);
			onFrameworkHandShake(&deviceBindManagerArray[i].framework_array[0]);
			deviceBindManagerArray[i].framework_flag = true;
			mspeech_usb_manager_construct(&deviceBindManagerArray[i].usb_manager);
			deviceBindManagerArray[i].usb_manager.init(&deviceBindManagerArray[i].usb_manager, SINGLE_DONGLE, _protocol_callback, _write_callback, &deviceBindManagerArray[i].usb_manager);
			_create_thread_usb_manager_read(&deviceBindManagerArray[i].usb_manager);
		}
		else if (device_array[i].device_type == 2) {
			mspeech_usb_manager_construct(&deviceBindManagerArray[i].usb_manager);
			deviceBindManagerArray[i].usb_manager.init(&deviceBindManagerArray[i].usb_manager, MULTI_DONGLE, _protocol_callback, _write_callback, &deviceBindManagerArray[i].usb_manager);
			_create_thread_usb_manager_read(&deviceBindManagerArray[i].usb_manager);
			__SleepSec(1);
			deviceBindManagerArray[i].framework_flag = true;
			deviceBindManagerArray[i].usb_manager.get_connected_device(&deviceBindManagerArray[i].usb_manager);
			deviceBindManagerArray[i].usb_manager.scan_device(&deviceBindManagerArray[i].usb_manager, 600, 60);
		}
	}
}

static void __printConnectedDevice() {
	//for (int i = 0; i < MAX_DEVICE_SIZE; i++)
	//{
	//	if (NULL != deviceBindManagerArray[0].framework_array[i].mac) {
	//		printf("%d:", i + 1);
	//		printDex(deviceBindManagerArray[0].framework_array[i].mac, 6);
	//	}
	//}
	for (int i = 0; i < MAX_DEVICE_SIZE; i++)
	{
		if (NULL != deviceBindManagerArray[i].usb_manager.get_connected_device)
		{
			deviceBindManagerArray[i].usb_manager.get_connected_device(&deviceBindManagerArray[i].usb_manager);
		}
	}
}

static void __removeCurrentDevice(int index) {

	if (NULL != deviceBindManagerArray[0].framework_array[index - 1].mac) {
		deviceBindManagerArray[0].usb_manager.remove_device(&deviceBindManagerArray[0].usb_manager, deviceBindManagerArray[0].framework_array[index - 1].mac);
	}
}

static void _reset_multidongle()
{
	deviceBindManagerArray[0].usb_manager.reset_device(&deviceBindManagerArray[0].usb_manager);
}

static void _start_sincheck()
{
	deviceBindManagerArray[0].framework_array[0].start_sincheck(&deviceBindManagerArray[0].framework_array[0]);
}

static void _set_flip_page_1()
{
	deviceBindManagerArray[0].framework_array[0].set_flip_page(&deviceBindManagerArray[0].framework_array[0], 1);
}
static void _set_flip_page_0()
{
	deviceBindManagerArray[0].framework_array[0].set_flip_page(&deviceBindManagerArray[0].framework_array[0], 0);
}

static void _get_flip_page()
{
	deviceBindManagerArray[0].framework_array[0].get_flip_page(&deviceBindManagerArray[0].framework_array[0]);
}

static void _get_subtitle_status()
{
	deviceBindManagerArray[0].framework_array[0].get_subtitle_status(&deviceBindManagerArray[0].framework_array[0]);
}

static void _get_work_mode()
{
	deviceBindManagerArray[0].framework_array[0].get_work_mode(&deviceBindManagerArray[0].framework_array[0]);
}

static void _start_ota()
{
	unsigned char session_data[16] = { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 };
	if (__read_byte_stream_file())
		deviceBindManagerArray[0].framework_array[0].start_ota(&deviceBindManagerArray[0].framework_array[0], session_data, frimware_data_info.total_firmware_data, frimware_data_info.total_firmware_size);
}

static void _stop_ota()
{
	deviceBindManagerArray[0].framework_array[0].end_ota(&deviceBindManagerArray[0].framework_array[0], 0);
}

static void _start_usb_ota()
{
	unsigned char session_data[16] = { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 };
	if (__read_byte_stream_file())
		deviceBindManagerArray[0].usb_manager.start_ota(&deviceBindManagerArray[0].usb_manager, session_data, frimware_data_info.total_firmware_data, frimware_data_info.total_firmware_size);
}

static void _get_master_product_no()
{
	deviceBindManagerArray[0].usb_manager.get_dongle_product_no(&deviceBindManagerArray[0].usb_manager);
}

static void _start_scan()
{
	deviceBindManagerArray[0].usb_manager.scan_device(&deviceBindManagerArray[0].usb_manager, 600, 60);
}

static void _stop_usb_ota()
{
	deviceBindManagerArray[0].usb_manager.end_ota(&deviceBindManagerArray[0].usb_manager, 0);
}

static void _set_cpi(unsigned int value) {
	deviceBindManagerArray[0].framework_array[0].set_dpi(&deviceBindManagerArray[0].framework_array[0], value);
}
static void _get_cpi() {
	//deviceBindManagerArray[0].framework_array[0].get_dpi(&deviceBindManagerArray[0].framework_array[0]);
	deviceBindManagerArray[0].framework_array[0].get_dongle_version(&deviceBindManagerArray[0].framework_array[0]);
}

static void _start_record() {
	deviceBindManagerArray[0].framework_array[0].start_record(&deviceBindManagerArray[0].framework_array[0]);
}

static void _stop_record() {
	deviceBindManagerArray[0].framework_array[0].stop_record(&deviceBindManagerArray[0].framework_array[0]);
}

static void _set_custom_key() {
	unsigned char data_value[8] = { 0x0, 0, 0x4, 0, 0, 0, 0, 0 };
	deviceBindManagerArray[0].framework_array[0].set_custom_key(&deviceBindManagerArray[0].framework_array[0], 1, 1, 0, data_value, 8);
}

static void _get_custom_key(unsigned char index)
{
	deviceBindManagerArray[0].framework_array[0].get_custom_key(&deviceBindManagerArray[0].framework_array[0], index);
}

static void _set_theme()
{
	rgb_param rgb_value[] = { {0,255,255},{255,0,0},{0,255,0},{255,0,255},{255,255,0} };
	deviceBindManagerArray[0].framework_array[0].set_led_rgb(&deviceBindManagerArray[0].framework_array[0], 1, 1000, rgb_value, 1);
}

static void _get_theme()
{
	deviceBindManagerArray[0].framework_array[0].get_theme(&deviceBindManagerArray[0].framework_array[0]);
}

static void _set_sleep_time(unsigned int time_s)
{
	deviceBindManagerArray[0].framework_array[0].set_sleep_time(&deviceBindManagerArray[0].framework_array[0], time_s);
}

static void _get_connection_type()
{
	deviceBindManagerArray[0].framework_array[0].get_connection_type(&deviceBindManagerArray[0].framework_array[0]);
}

static void _set_polling_rate()
{
	deviceBindManagerArray[0].framework_array[0].set_polling_rate(&deviceBindManagerArray[0].framework_array[0], 500);
}

static void _get_polling_rate()
{
	deviceBindManagerArray[0].framework_array[0].get_polling_rate(&deviceBindManagerArray[0].framework_array[0]);
}

static void _get_dongle_product_no()
{
	deviceBindManagerArray[0].framework_array[0].get_dongle_product_no(&deviceBindManagerArray[0].framework_array[0]);
}

static void _set_laser_mode(unsigned char index)
{
	deviceBindManagerArray[0].framework_array[0].set_laser_mode(&deviceBindManagerArray[0].framework_array[0], index);
}
static void _get_laser_mode()
{
	deviceBindManagerArray[0].framework_array[0].get_laser_mode(&deviceBindManagerArray[0].framework_array[0]);
}

static void heart_beat(unsigned char index)
{
	deviceBindManagerArray[0].framework_array[0].heart_beat(&deviceBindManagerArray[0].framework_array[0], index);
}
static void player_play(unsigned char index)
{
	deviceBindManagerArray[0].framework_array[0].player_play(&deviceBindManagerArray[0].framework_array[0], index);
}
static void set_player_volume(unsigned char index)
{
	deviceBindManagerArray[0].framework_array[0].set_player_volume(&deviceBindManagerArray[0].framework_array[0], index);
}

static void set_time_heart_beat(unsigned int time)
{
	deviceBindManagerArray[0].framework_array[0].set_time_heart_beat(&deviceBindManagerArray[0].framework_array[0], time);
}
static void get_time_heart_beat()
{
	deviceBindManagerArray[0].framework_array[0].get_time_heart_beat(&deviceBindManagerArray[0].framework_array[0]);
}

static void get_player_volume()
{
	deviceBindManagerArray[0].framework_array[0].get_player_volume(&deviceBindManagerArray[0].framework_array[0]);
}
static void set_adaptive_modulation(unsigned int value)
{
	deviceBindManagerArray[0].framework_array[0].set_adaptive_modulation(&deviceBindManagerArray[0].framework_array[0], value);
}
static void get_adaptive_modulation()
{
	deviceBindManagerArray[0].framework_array[0].get_adaptive_modulation(&deviceBindManagerArray[0].framework_array[0]);
}

static void get_ls_device_id()
{
	deviceBindManagerArray[0].framework_array[0].get_ls_device_id(&deviceBindManagerArray[0].framework_array[0]);
}
static void get_ls_check_sum(unsigned int time)
{
	deviceBindManagerArray[0].framework_array[0].get_ls_check_sum(&deviceBindManagerArray[0].framework_array[0], time);
}

#define TEST_REMOVE_DEVICE 1

static void onTestFrameworkProject()
{

	//	mMadaoMsgHandle = new MadaoMessage();
	onTestMspeechUsb();

	if (TEST_REMOVE_DEVICE) {
		while (true)
		{
			printf("请设置操作\n");

			printf("1. 开始心跳 \n");
			printf("2. 开始语音播报\n");
			printf("3. 请输入音量大小\n");
			printf("4. 请输入下发时间戳\n");
			printf("5. 获取LS DEVEICE ID\n");
			printf("6. 请获取心跳时间\n");
			char arr[64];

			int i = 0;
			char ch;
			scanf_s("%c", &arr[0], 1);
			ch = arr[0];
			while (ch != '\n')
			{
				i++;
				scanf_s("%c", &arr[i], 1);
				ch = arr[i];
			}
			arr[i] = '\0';
			if (i == 1)
			{
				if (arr[0] == '1')
				{
					_get_flip_page();
					//heart_beat(0);
					//__printConnectedDevice();
				}
				else if (arr[0] == '2')
				{

					//_start_pcbcheck();
					//set_backlight(0, 2);
					printf("开始语音播报\n");
					int n;
					cin >> n;
					player_play(n);


				}
				else if (arr[0] == '3')
				{
					printf("设置自适应调频\n");
					int n;
					cin >> n;
					_set_cpi(n);
					//set_adaptive_modulation(n);
					//_start_sincheck();
					//get_slave_version();
				}
				else if (arr[0] == '4')
				{
					printf("请输入下发时间戳,获取ls check sum\n");
					int n;
					cin >> n;
					get_ls_check_sum(n);
					//_start_sincheck();
					//get_slave_version();
				}
				else if (arr[0] == '5')
				{
					printf("获取LS DEVEICE ID\n");
					get_ls_device_id();

				}
				else if (arr[0] == '6')
				{

					printf("请获取心跳时间\n");
					get_time_heart_beat();

				}
			}
			//printf("请设置操作\n");

			//printf("1. 开启录音 \n");
			//printf("2. 关闭录音\n");
			//printf("3. 开始OTA\n");
			//printf("4. 结束OTA\n");
			//printf("5. 设置自定义按键\n");
			//printf("6. 获取自定义按键\n");
			//char arr[64];

			//int i = 0;
			//char ch;
			//scanf_s("%c", &arr[0], 1);
			//ch = arr[0];
			//while (ch != '\n')
			//{
			//	i++;
			//	scanf_s("%c", &arr[i], 1);
			//	ch = arr[i];
			//}
			//arr[i] = '\0';
			//if (i == 1)
			//{
			//	if (arr[0] == '1')
			//	{
			//		//__printConnectedDevice();
			//		_start_record();
			//	}
			//	else if (arr[0] == '2')
			//	{
			//		
			//		printf("请继续输入要设置的cpi值:0-5 \n");
			//		int input;
			//		scanf_s("%d", &input);

			//		if (input >= 0 && input <= MAX_DEVICE_SIZE + 1) {
			//		_set_cpi(input);
			//		_get_cpi();
			//		}
			//		//_stop_record();
			//		//_start_sincheck();
			//	}
			//	else if (arr[0] == '3') {
			//		//_get_subtitle_status();
			//		//_get_cpi();
			//		//_start_ota();
			//		//_get_cpi();
			//		_start_usb_ota();

			//	}
			//	else if (arr[0] == '4') {
			//		//_stop_ota();
			//		_stop_usb_ota();
			//	}
			//	else if (arr[0] == '5') {
			//		//_reset_multidongle();
			//		//_set_custom_key();
			//		_set_polling_rate();
			//		//_set_theme();
			//		//_get_master_product_no();
			//		//_set_sleep_time(20); 
			//		//_get_connection_type();
			//		//return;
			//	}
			//	else if (arr[0] == '6') {
			//		//_get_dongle_product_no();
			//		//_get_theme();
			//		//_get_custom_key(1);
			//		//_set_custom_key();
			//		//_start_scan();
			//		//return;
			//	}
			//}
			__SleepSec(1);
		}
	}
}

static void onTestPcmEngie() {

	struct mspeech_pcm pcm;
	mspeech_pcm_construct(&pcm);
	int channles = 1;
	int error = pcm.init(&pcm, MSPEECH_ENCODE_OPUS, channles);
	if (error == 0) {
		printf("pcm init error\r\n");
		return;
	}

	unsigned char input[80]; // 16KHZ, T=20ms
	unsigned short out[640] = { 0 };
	unsigned short out_samples = 0;


	FILE *fileOpus = NULL;
	/*int fileCode = fopen_s(&fileOpus, "opus_test.opus", "rb");*/
	//int fileCode = fopen_s(&fileOpus, "mic-0.bin", "rb");
	//int fileCode = fopen_s(&fileOpus, "mic-16_1.opus", "rb");
	if (channles == 2) {
		fopen_s(&fileOpus, "mic-0.opus", "rb");
	}
	else {
		fopen_s(&fileOpus, "mic-1.opus", "rb");
	}


	//printf("opus open:%d\r\n", fileCode);

	FILE * filePcm = NULL;
	fopen_s(&filePcm, "pcm_test.pcm", "wb");

	int readCount;
	while ((readCount = fread(input, 1, 40 * channles, fileOpus)) > 0) {
		printf("opus read:%d\r\n", readCount);
		int ret = pcm.convert2pcm(&pcm, (unsigned short *)input, out, &out_samples);
		if (ret > 0) {
			printf("decode:%d \r\n", out_samples);
			if (channles == 2) {
				unsigned short temp[320] = { 0 };
				unsigned short pos = 0;
				for (int i = 0; i < out_samples; i = i + 2) {
					temp[pos++] = out[i];
				}
				fwrite(temp, 1, out_samples, filePcm);
			}
			else {
				fwrite(out, 1, out_samples * 2, filePcm);
			}

			/*		fwrite(out, 1, out_samples * 2, filepcm);*/
			fflush(filePcm);
		}
		else {
			printf("decode error:%d\r\n", out_samples);
		}
	}

	fclose(filePcm);
	fclose(fileOpus);

	printf("read finish\r\n");
}

//int pAuth_path(void *data, int len, int tag) {
//	printf("pAuth_data:%s\n", (char *)data);
//	printf("pAuth_len:%d\n", len);
//	printf("pAuth_tag:%d\n", tag);
//	typedef int(*FunDLL2)(void *pData, int dataLen, int tag);
//	HMODULE hdll = LoadLibrary(_T("iflysmlogic.dll"));
//	FunDLL2 sm_active = (FunDLL2)GetProcAddress(hdll, "sm_active");
//	int result = sm_active(data, len, tag);
//	printf("result:%d\n", result);
//	return result;
//}

int main()
{
	/*typedef void(*FunDLL1)(SM_Authencation pAuthT);

	HMODULE hdll = LoadLibrary("../release/iflysmlogic.dll");
	FunDLL1 sm_init = (FunDLL1)GetProcAddress(hdll, "sm_init");
	sm_init(pAuth_path);*/
	onTestFrameworkProject();
	//onTestPcmEngie();
//while (1)
//{
//	__SleepSec(1);
//}
//   std::cout << "Hello World!\n";
}
