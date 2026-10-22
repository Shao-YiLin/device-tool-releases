/*
 * @Author: xuhan
 * @Date: 2020-02-08 15:12:53
 * @LastEditors  : xuhan
 * @LastEditTime : 2020-02-10 20:09:47
 * @Description:
 */

#include <string.h>
#include <time.h>

#include "lib_device_manager.h"
#include "protocol_2_logic_manager.h"
#include "protocol_1_logic_manager.h"
#include "mblue_heap.h"
#include "libprotocol_logic_define.h"
#include "include/commondefine.h"
#include "MadaoMessage.h"

static void _open_device(struct lib_device_manager *dm);

#define MAX_COMMAND_SIZE 128
#define PART_FIRMWARE_SIZE 32

enum authenticateStep {
	setClientFingerStep = 0x1,
	getDeviceFingerStep = 0x2,
	getDeviceMacStep = 0x4,
	getDeviceVersionStep = 0x8,
	deviceDisconnect = 0x10
};

static char *find_char(char *source, char *chare)
{
	char *ret = 0;
	if (*source == '\0' || *chare == '\0') {
		return ret;
	}
	char temp;
	char *p;
	p = 0;
	while ((temp = *chare++) != '\0') { /*����chare�ַ���*/
		p = source;
		while (*p != '\0')
		{
			if (*p == temp)
				return p;
			else
				p++;
		}
	}
	return ret;
}

static BOOL get_board_serialnumber(char *lpszBaseBoard, int len/*=128*/)
{
	WCHAR szFetCmd[] = L"wmic BaseBoard get SerialNumber"; // ��ȡ�������к�������	
	char strEnSearch[] = { "SerialNumber" }; // �������кŵ�ǰ����Ϣ

	BOOL   bret = FALSE;
	HANDLE hReadPipe = NULL; //��ȡ�ܵ�
	HANDLE hWritePipe = NULL; //д��ܵ�	
	PROCESS_INFORMATION pi;   //������Ϣ	
	STARTUPINFO			si;	  //���������д�����Ϣ
	SECURITY_ATTRIBUTES sa;   //��ȫ����

	char			szBuffer[MAX_COMMAND_SIZE + 1] = { 0 }; // ���������н�������������
	char			strBuffer[MAX_COMMAND_SIZE] = { 0 };
	unsigned long	count = 0;
	long			ipos = 0;
	char *p;
	int index = 0;

	memset(&pi, 0, sizeof(pi));
	memset(&si, 0, sizeof(si));
	memset(&sa, 0, sizeof(sa));

	pi.hProcess = NULL;
	pi.hThread = NULL;
	si.cb = sizeof(STARTUPINFO);
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = TRUE;

	int j = 0;

	//1.0 �����ܵ�
	bret = CreatePipe(&hReadPipe, &hWritePipe, &sa, 0);
	if (!bret)
	{
		goto END;
	}

	//2.0 ���������д��ڵ���ϢΪָ���Ķ�д�ܵ�
	GetStartupInfo(&si);
	si.hStdError = hWritePipe;
	si.hStdOutput = hWritePipe;
	si.wShowWindow = SW_HIDE; //���������д���
	si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;

	//3.0 ������ȡ�����еĽ���
	bret = CreateProcess(NULL, szFetCmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
	if (!bret)
	{
		goto END;
	}

	//4.0 ��ȡ���ص�����
	WaitForSingleObject(pi.hProcess, 500/*INFINITE*/);
	bret = ReadFile(hReadPipe, szBuffer, MAX_COMMAND_SIZE, &count, 0);
	if (!bret)
	{
		goto END;
	}

	//5.0 �����������к�
	bret = FALSE;
	p = find_char(szBuffer, strEnSearch);
	if (!p)
		goto END;
	else {
		p = p + strlen(strEnSearch);
		while (*p != '\0') {
			strBuffer[index++] = *p;
			p++;
		}
	}

	//ȥ���м�Ŀո� \r \n
	size_t i;
	for (i = 0; i < strlen(strBuffer); i++)
	{
		if (strBuffer[i] != ' ' && strBuffer[i] != '\n' && strBuffer[i] != '\r')
		{
			lpszBaseBoard[j] = strBuffer[i];
			j++;
		}
	}

	bret = TRUE;

END:
	//�ر����еľ��
	CloseHandle(hWritePipe);
	CloseHandle(hReadPipe);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return(bret);
}

static bool _is_valid_data(char *data, int len) {

	if (NULL == data || len == 0)
		return false;


	for (int i = 0; i < len; i++) {
		char value = *(data + i);
		if (value != 0)
			return true;
	}

	return false;
}

static void _get_client_finger(struct lib_device_manager *dm, unsigned char *finger) {


	if (_is_valid_data(dm->client_finger, 16) == true) {
		memcpy(finger, dm->client_finger, 16);
		return;
	}

	char board_uuid[128] = { 0 };
	if (get_board_serialnumber(board_uuid, sizeof(board_uuid)))
	{
		md5_ctx md5ctx;
		dm->md5.init(&dm->md5, &md5ctx);
		dm->md5.update(&dm->md5, &md5ctx, (unsigned char*)board_uuid, sizeof(board_uuid));
		dm->md5.final(&dm->md5, &md5ctx, finger);
		memcpy(dm->client_finger, finger, 16);
		return;
	}


	srand(time(NULL));

	for (int i = 0; i < 16; i++) {
		finger[i] = rand();
	}
	memcpy(dm->client_finger, finger, 16);
}

static void _get_common_sessionkey(struct lib_device_manager *dm, unsigned char* key, char *mac, char *version) {
	memcpy(key, mac, 6);
	*(key + 6) = '{';
	memcpy(key + 7, version, 4);
	*(key + 11) = '}';
	u32 hash_key = dm->hash.sdbm_mem(&dm->hash, key, 12);
	memcpy(key + 12, &hash_key, 4);
}

static void _generate_random(unsigned char* random) {

	srand((unsigned)time(NULL));
	for (int i = 0; i < 4; i++)
	{
		unsigned int random1 = rand();
		memcpy(random + i * 4, &random1, 4);
	}
}



static void printDex(unsigned char *data, int len)
{
	if (len == 0)
		return;

	for (int i = 0; i < len; i++)
	{
		printf("%02x", *(data + i) & 0xff);
	}
	printf("\n");
}

static void ___authentication_action(struct lib_device_manager *dm, int phase, char *data, int len)
{
	if (!dm || !data)
		return;

	//	printf("phase: %d \r\n", phase);

	if (phase == PHASE1)
	{
		/*if (strlen(dm->device_mac) == 0 || strlen(dm->device_finger) == 0 || strlen(dm->device_version) == 0) {
			return;
		}*/

		if (_is_valid_data(dm->device_mac, 6) == false || _is_valid_data(dm->device_finger, 16) == false || _is_valid_data(dm->device_version, 4) == false) {
			printf("authentication invalid param \r\n");
			printf("mac:");
			printDex(dm->device_mac, 6);
			printf("device_finger:");
			printDex(dm->device_finger, 16);
			printf("device_version:");
			printDex(dm->device_version, 4);
			if (dm->libmouse_action_cb)
				dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_AUTHENTICATION, 0);
			return;
		}

		//printf("authenticateion phase1\n");
		unsigned char client_finger[16] = { 0 };
		_get_client_finger(dm, client_finger);
		unsigned char session_key[16] = { 0 };
		_get_common_sessionkey(dm, session_key, dm->device_mac, dm->device_version);
		memset(dm->client_random, 0x0, sizeof(dm->client_random));
		_generate_random(dm->client_random);

		if (dm->protocol_type == 2) {
			if (dm->protocol_handle2)
				((struct protocol_logic_manager *)dm->protocol_handle2)->set_authenticate(dm->protocol_handle2, data, len, session_key, dm->device_finger, client_finger, dm->client_random);
		}
		else {
			if (dm->protocol_handle1)
				((struct protocol_1_logic_manager *)dm->protocol_handle1)->set_authenticate(dm->protocol_handle1, data, len, NULL, NULL, NULL, NULL);
		}
	}
	else if (phase == PHASE3)
	{
		if (len != 16) {
			//	printf("phase len != 16\r\n");
			if (dm->libmouse_action_cb)
				dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_AUTHENTICATION, 0);
			return;
		}


		/*if (strlen(dm->client_random) == 0) */

		if (_is_valid_data(dm->client_random, 16) == false)
		{
			//	printf("client_random fail: ");
			//	printDex(dm->client_random, 16);
			if (dm->libmouse_action_cb)
				dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_AUTHENTICATION, 0);
			return;
		}


		unsigned char client_finger[16] = { 0 };
		_get_client_finger(dm, client_finger);

		u32 hash_finger = dm->hash.sdbm_mem(&dm->hash, client_finger, sizeof(client_finger));
		unsigned char temp[4 + 16] = { 0 };
		memcpy(temp, &hash_finger, 4);
		memcpy(temp + 4, dm->client_random, 16);

		unsigned char salt[16] = { 0 };

		md5_ctx md5ctx;
		dm->md5.init(&dm->md5, &md5ctx);
		dm->md5.update(&dm->md5, &md5ctx, temp, sizeof(temp));
		dm->md5.final(&dm->md5, &md5ctx, salt);

		/* printf("mouse data : ");
		 printDex(data, 16);
		 printf("client data : ");
		 printDex((char *)salt, 16);*/

		if (dm->libmouse_action_cb) {
			if (!memcmp(data, salt, 16)) {
				dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_AUTHENTICATION, 1);
			}
			else {
				dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_AUTHENTICATION, 0);
			}
		}
	}
}

static void __data_action_cb_fn(void *param, unsigned char cmd, int mode, int channel, char *data, int len) {
	struct lib_device_manager *dm = param;
	if (!dm)
		return;

	MessageInfo info;
	memset(&info, 0, sizeof(info));
	info.cmd = cmd;
	info.mode = mode;
	info.channel = channel;
	info.len = len;
	memcpy(info.data, data, len);
	dm->msgManager.send(&dm->msgManager, &info, 1);
	/*if (cmd == START_RECORD) {
		printf("__data_action_cb_fn \n");
	}*/
}

static unsigned char __read_part_firmware(struct lib_device_manager *dm, unsigned char *out_data, unsigned long size) {
	if (dm->ota_param.firmware == NULL || size != 32 || out_data == NULL || dm->ota_param.firmware_len < dm->ota_param.firmware_index) {
		if (dm->ota_param.firmware != NULL) {
			free(dm->ota_param.firmware);
			dm->ota_param.firmware = NULL;
		}
		dm->ota_param.firmware_len = 0;
		return 0;
	}
	if (dm->ota_param.firmware_len == dm->ota_param.firmware_index) {
		if (dm->ota_param.firmware != NULL) {
			free(dm->ota_param.firmware);
			dm->ota_param.firmware = NULL;
		}
		dm->ota_param.firmware_len = 0;
		return 2;
	}
	if (dm->ota_param.firmware_len < (dm->ota_param.firmware_index + size)) {
		memcpy(out_data, dm->ota_param.firmware + dm->ota_param.firmware_index, dm->ota_param.firmware_len - dm->ota_param.firmware_index);
		memset(out_data + (dm->ota_param.firmware_len - dm->ota_param.firmware_index), 0xff, size + dm->ota_param.firmware_index - dm->ota_param.firmware_len);
		dm->ota_param.firmware_index = dm->ota_param.firmware_len;
	}
	else if (dm->ota_param.firmware_len >= (dm->ota_param.firmware_index + size)) {
		memcpy(out_data, dm->ota_param.firmware + dm->ota_param.firmware_index, size);
		dm->ota_param.firmware_index += size;
	}
	return 1;
}

static void __action_cb_fn(void *param, unsigned char cmd, int mode, int channel, char *data, int len)
{
	struct lib_device_manager *dm = param;

	

	switch (cmd)
	{
	case KEY_DOWN:
		dm->libmouse_event_cb(dm->base, LIBMOUSE_EVENT_KEY_DOWN, mode);
		break;
	case KEY_UP:
		dm->libmouse_event_cb(dm->base, LIBMOUSE_EVENT_KEY_UP, mode);
		break;
	case RECORD_DATA:
	{
		dm->libmouse_audio_cb(dm->base, mode, channel, (unsigned short *)data, len);
	}
	break;
	case DPI_DATA:
	case GET_DPI:
	{
		if (len == 4 && data != NULL)
		{
			int cur_dpi_level = 0;
			memcpy(&cur_dpi_level, data, len);
			dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_DPI, cur_dpi_level);
		}
		else {
			dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_DPI, mode);
		}
	}
	break;
	case BATTER:
	{
		int cur_batter = 0;
		memcpy(&cur_batter, data, len);
		dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_BATTERY, cur_batter);
	}
	break;
	case HOTPLUG:
		dm->libmouse_event_cb(dm->base, LIBMOUSE_EVENT_LEFT, 0);
		break;
	case BLE_STATUS:
	{
		if (len == 4)
		{
			int cur_ble_status = 0;
			memcpy(&cur_ble_status, data, len);
			if (cur_ble_status == 1) {
				dm->libmouse_event_cb(dm->base, LIBMOUSE_EVENT_BLE_CONNECTED, 0);
				//_open_device(dm);
			}
			else
				dm->libmouse_event_cb(dm->base, LIBMOUSE_EVENT_BLE_DISCONNECTED, 0);
		}
	}
	break;
	case VERSION_MOURSE_DATA:
	{
		if (len == 4)
		{
			dm->libmouse_info_cb(dm->base, LIBMOUSE_INFO_MOUSE_VERSION, data, len);
			memset(dm->device_version, 0x0, sizeof(dm->device_version));
			memcpy(dm->device_version, data, 4);
			dm->authentication_step |= getDeviceVersionStep;
			//	printf("VERSION_MOURSE_DATA \n");
		}
	}
	break;
	case VERSION_DONGLE_DATA:
	{
		if (len == 4)
		{
			dm->libmouse_info_cb(dm->base, LIBMOUSE_INFO_DONGLE_VERSION, data, len);
		}
	}
	break;
	case MAC_MOURSE_DATA:
	{
		if (len == 6)
		{
			dm->libmouse_info_cb(dm->base, LIBMOUSE_INFO_MOUSE_MACADDR, data, len);
			memset(dm->device_mac, 0x0, sizeof(dm->device_mac));
			memcpy(dm->device_mac, data, 6);
			dm->authentication_step |= getDeviceMacStep;
			//	printf("MAC_MOURSE_DATA \n");
		}
	}
	break;
	case MAC_DONGLE_DATA:
	{
		if (len == 6)
		{
			dm->libmouse_info_cb(dm->base, LIBMOUSE_INFO_DONGLE_MACADDR, data, len);
		}
	}
	break;
	case START_RECORD:
		//printf("START_RECORD \n");
		dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_START_RECORD, mode);
		break;
	case STOP_RECORD:
		dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_STOP_RECORD, mode);
		break;
	case SET_DPI:
		dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_SET_DPI, mode);
		break;
	case LED_RGB:
		dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_LED_RGB, mode);
		break;
	case VIBRATION:
		dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_VIBRATION, mode);
		break;
	case SET_CUSTOM_BTN:
		dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_CUSTOM_KEY, mode);
		break;
	case HANDSHAKE: {
		int handshakeFlag = mode & 0xffff;
		int authenticationFlag = (mode & 0xffff0000) >> 16;
		dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_HANDSHAKE, handshakeFlag);
		Sleep(20);
		if (dm->protocol_type == 1) {
			if (NULL != dm->libmouse_action_cb)
				dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_AUTHENTICATION, 0xFE);
		}
		else {
			if (authenticationFlag == 1)
				dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_AUTHENTICATION, 1);
		}
	}


					//printf("HANDSHAKE \n");
					break;
	case PRODUCT_NO:
		if (len == 4)
		{
			memcpy(dm->ota_param.product_NO, data, 4);
			dm->libmouse_info_cb(dm->base, LIBMOUSE_INFO_MOUSE_PRODUCTNO, data, len);
		}
		break;
	case PROTOCOL_VERSION:
		if (len == 4)
		{
			int protocolVersion = 0;
			memcpy(&protocolVersion, data, 4);
			if (protocolVersion > 0) {
				dm->is_timeout = 0;
			}
			dm->libmouse_info_cb(dm->base, LIBMOUSE_INFO_PROTOCOL_VERSION, data, len);
		}
		break;
	case AUTHENTICATE:
		___authentication_action(dm, mode, data, len);
		break;
	case DEVICE_FINGER:
	{
		if (len == 16) {
			memset(dm->device_finger, 0x0, sizeof(dm->device_finger));
			memcpy(dm->device_finger, data, 16);
			dm->authentication_step |= getDeviceFingerStep;
			//printf("DEVICE_FINGER \n");
		}
	}
	break;
	case CLIENT_FINGER: {

		if (mode == 1) {
			dm->authentication_step |= setClientFingerStep;
		}
		else if (mode == ERR_DISCONNECT) {
			dm->authentication_step |= deviceDisconnect;
		}
		else {
			dm->authentication_step &= ~setClientFingerStep;
		}
	}
						break;
	case IFLY_AUTHENTICATE:
	{

	}
	break;
	case SIN_CHECK:
	{
		printf("action sincheck\r\n");
		dm->libmouse_event_cb(dm->base, LIBMOUSE_EVENT_SINCHECK, mode);
	}
	break;
	case MANUFACTURE_INSTRUCTION:
		dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_SINCHECK, mode);
		break;
	case THEME:
	{
		if (len != 0)
		{
			dm->libmouse_info_cb(dm->base, LIBMOUSE_INFO_MOUSE_THEME, data, len);
		}
	}
	break;
	case GET_FLIP_PAGE:
		if (len == 4)
		{
			dm->libmouse_info_cb(dm->base, LIBMOUSE_INFO_MOUSE_FLIP_PAGE, data, len);
		}
		break;
	case GET_SUBTITLE_STATUS:
		if (len != 0)
		{
			dm->libmouse_info_cb(dm->base, LIBMOUSE_INFO_MOUSE_SUBTITLE_STATUS, data, len);
		}
		break;
	case GET_MOUSE_WOEK_MODE:
		if (len != 0)
		{
			dm->libmouse_info_cb(dm->base, LIBMOUSE_INFO_MOUSE_WORK_MODE, data, len);
		}
		break;
	case GET_MOUSE_CONNECTION_TYPE:
		if (len != 0)
		{
			dm->libmouse_info_cb(dm->base, LIBMOUSE_INFO_MOUSE_CONNECTION_TYPE, data, len);
		}
		break;
	case GET_CUSTOM_BTN:
		if (len > 0 && mode == ERR_SUCCESS)
		{
			dm->libmouse_info_cb(dm->base, LIBMOUSE_INFO_MOUSE_CUSTOM_KEY, data, len);
		}
		break;
	case START_OTA:
	{
		if (len == 16 && mode == ERR_SUCCESS)
		{
			unsigned char salt[16] = { 0 };
			md5_ctx md5ctx;
			dm->md5.init(&dm->md5, &md5ctx);
			dm->md5.update(&dm->md5, &md5ctx, dm->ota_param.session, 16);
			dm->md5.final(&dm->md5, &md5ctx, salt);
			if (memcmp(salt, data, len) == 0) {
				unsigned char firmware_data[32] = { 0 };
				unsigned char read_result = __read_part_firmware(dm, firmware_data, PART_FIRMWARE_SIZE);
				if (read_result == 1)
				{
					dm->update_ota_data(dm, firmware_data, PART_FIRMWARE_SIZE);
				}
				else
				{
					dm->end_ota(dm, 0);
					dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_START_OTA, 0);
					break;
				}
				dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_START_OTA, 1);
				break;
			}
		}
		dm->end_ota(dm, 0);
		dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_START_OTA, 0);
	}break;
	case UPDATE_OTA:
	{
		if (len == 4 && mode == ERR_SUCCESS) {
			unsigned int firmware_hash;
			memcpy(&firmware_hash, data, len);
			if (dm->ota_param.firmware_hash_sum == firmware_hash)
			{
				unsigned char firmware_data[32] = { 0 };
				unsigned char read_result = __read_part_firmware(dm, firmware_data, PART_FIRMWARE_SIZE);
				if (read_result == 1)
				{
					dm->update_ota_data(dm, firmware_data, PART_FIRMWARE_SIZE);
				}
				else if (read_result == 2)
				{
					dm->end_ota(dm, 1);
				}
				else
				{
					dm->end_ota(dm, 0);
				}
			}
			else
				dm->end_ota(dm, 0);
			dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_UPDATE_OTA, dm->ota_param.firmware_index);
		}
		else
			dm->end_ota(dm, 0);
	}break;
	case END_OTA:
		if (dm->ota_param.firmware)
		{
			free(dm->ota_param.firmware);
			dm->ota_param.firmware = NULL;
		}
		dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_END_OTA, mode);
		break;
	case GET_POLLING_RATE:
		if (len == 4 && data != NULL)
		{
			int device_polling_rate = 0;
			memcpy(&device_polling_rate, data, len);
			dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_GET_POLLING_RATE, device_polling_rate);
		}
		else {
			dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_GET_POLLING_RATE, mode);
		}
		break;
	case SET_POLLING_RATE:
		dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_SET_POLLING_RATE, mode);
		break;
	case DONGLE_PRODUCT_NO:
		dm->libmouse_info_cb(dm->base, LIBMOUSE_INFO_DONGLE_PRODUCTNO, data, len);
	case GET_LED_INTENSITY:
		if (len == 2 && data != NULL)
		{
			
			//dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_GET_LED_INTENCITY, indexandvalue);
			dm->libmouse_info_cb(dm->base, LIBMOUSE_INFO_GET_LED_INTENCITY, data, 2);
		}
		
		break;
	case SET_LED_INTENSITY:
		dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_SET_LED_INTENCITY, mode);
		break;
	//case GET_BACHLIGHT:
	//	if (len == 2 && data != NULL)
	//	{
	//		dm->libmouse_info_cb(dm->base, LIBMOUSE_INFO_GET_RGB_BACKLIGHT, data, 2);
	//	}
	//	break;
	case SET_BACKHLIGHT:
		dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_SET_RGB_BACKLIGHT, mode);
		break;
	case SET_LASER:
		dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_SET_LASER, mode);
		break;
	case SLAVE_DEVICE_VERSION:
		//printf(" SLAVE_DEVICE_VERSION\n");
	//	if (len == 2)
		{
			dm->libmouse_info_cb(dm->base, LIBMOUSE_INFO_SLAVE_DEVICE_VERSION, data, len);

		}
		break;
	case SET_WHEEL_MODE:
		dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_SET_WHEEL, mode);
		break;
	case GET_WHEEL_MODE:
	{
		//int cur_dpi_level = 0;
		//memcpy(&cur_dpi_level, data, len);
		if (len == 4 && data != NULL)
		{
			dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_GET_WHEEL, *data);
		}
		else
		dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_GET_WHEEL,mode );
	}

		break;
	case START_HEART:
		if (mode == 1)
		{
			dm->libmouse_event_cb(dm->base, LIBMOUSE_EVENT_HEART_HEAT, channel);
		}
		break;
	case PlAYER_PLAY:
		dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_PlAYER_PLAY, mode);
		break;
	case SET_PLAYER_VOLUME:
		dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_SET_PLAYER_VOLUME, mode);
		break;
	case GET_PLAYER_VOLUME:
		if (len == 4)
		{
			dm->libmouse_info_cb(dm->base, LIBMOUSE_INFO_GET_PLAYER_VOLUME, data, len);
		}
		break;
	case SET_TIME_HEART_BEAT:
		dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_SET_TIME_HEART_BEAT, mode);
		break;
	case GET_TIME_HEART_BEAT:
		if (len == 4)
		{
			dm->libmouse_info_cb(dm->base, LIBMOUSE_INFO_GET_TIME_HEART_BEAT, data, len);
		}
		break;
	case SET_KEYBOARD_CUSTOM_RGB:
		dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_SET_KEYBOARD_CUSTOM_RGB, mode);
		break;
	case SET_ADAPTIVE_MODULATION:
		dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_SET_ADAPTIVE_MODULATION, mode);
		break;
	case GET_ADAPTIVE_MODULATION:
	{
		if (len == 4 && data != NULL)
		{
			int adaptive_modulation = 0;
			memcpy(&adaptive_modulation, data, len);
			dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_GET_ADAPTIVE_MODULATION, adaptive_modulation);
		}
		else {
			dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_GET_ADAPTIVE_MODULATION, mode);
		}
	}
	break;
	case GET_LS_DEVICE_ID:
	{
		dm->libmouse_info_cb(dm->base, LIBMOUSE_INFO_GET_LS_DEVICE_ID, data, len);
	}
	break;

	case GET_LS_CHECK_SUM:
	{
		dm->libmouse_info_cb(dm->base, LIBMOUSE_INFO_GET_LS_CHECK_SUM, data, len);
	}
	break;
		
	



	default:
		break;
	}
}

static void __common_action_cb_fn(void *param, unsigned char cmd, int mode, int channel, char *data, int len)
{
	struct lib_device_manager *dm = param;

	switch (cmd)
	{
	case KEY_DOWN:
		dm->libmouse_event_cb(dm->base, LIBMOUSE_EVENT_KEY_DOWN, mode);
		break;
	case KEY_UP:
		dm->libmouse_event_cb(dm->base, LIBMOUSE_EVENT_KEY_UP, mode);
		break;
	case RECORD_DATA:
	{
		dm->libmouse_audio_cb(dm->base, mode, channel, (unsigned short *)data, len);
	}
	break;
	case DPI_DATA:
	case GET_DPI:
	{
		if (len == 4 && data != NULL)
		{
			int cur_dpi_level = 0;
			memcpy(&cur_dpi_level, data, len);
			dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_DPI, cur_dpi_level);
		}
		else {
			dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_DPI, mode);
		}
	}
	break;
	case BATTER:
	{
		int cur_batter = 0;
		memcpy(&cur_batter, data, len);
		dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_BATTERY, cur_batter);
	}
	break;
	case HOTPLUG:
		dm->libmouse_event_cb(dm->base, LIBMOUSE_EVENT_LEFT, 0);
		break;
	case BLE_STATUS:
	{
		if (len == 4 && data != NULL)
		{
			int cur_ble_status = 0;
			memcpy(&cur_ble_status, data, len);
			if (cur_ble_status == 1) {
				dm->libmouse_event_cb(dm->base, LIBMOUSE_EVENT_BLE_CONNECTED, 0);
			}
			else
				dm->libmouse_event_cb(dm->base, LIBMOUSE_EVENT_BLE_DISCONNECTED, 0);
		}
	}
	break;
	case SIN_CHECK:
	{
		//	printf("common action sincheck\r\n");
		int sincheck_value = 0;
		memcpy(&sincheck_value, data, len);
		dm->libmouse_event_cb(dm->base, LIBMOUSE_EVENT_SINCHECK, sincheck_value);
	}break;

	case GET_BACKHLIGHT:
	{	
		dm->libmouse_info_cb(dm->base, LIBMOUSE_INFO_GET_RGB_BACKLIGHT, (data+1), 4);

	}break;
	case GET_LASER:
	{
		int laser = 0;
		memcpy(&laser, data, len);
		dm->libmouse_event_cb(dm->base, LIBMOUSE_EVENT_LASER, laser);
		

	}break;
	
		
	default:
		break;
	}
}

static void _usb_write_cb_fn(void *param, unsigned char *data, int size)
{
	struct lib_device_manager *dm = param;
	if (dm->libmouse_write_cb)
		dm->libmouse_write_cb(dm->base, data, size);
}

static  void __init_protocol_handle(struct lib_device_manager *dm)
{
	if (!dm)
		return;

	if (!dm->protocol_handle2)
		return;
	protocol_logic_manager_construct(dm->protocol_handle2);
	((struct protocol_logic_manager *)dm->protocol_handle2)->register_action_callback(dm->protocol_handle2, __data_action_cb_fn, _usb_write_cb_fn, dm);
	((struct protocol_logic_manager *)dm->protocol_handle2)->init(dm->protocol_handle2);

	if (!dm->protocol_handle1)
		return;

	protocol_1_logic_manager_construct(dm->protocol_handle1);
	((struct protocol_1_logic_manager *)dm->protocol_handle1)->register_action_callback(dm->protocol_handle1, __data_action_cb_fn, _usb_write_cb_fn, dm);
	((struct protocol_1_logic_manager *)dm->protocol_handle1)->init(dm->protocol_handle1);
}

static void __uninit_protocol_handle(struct lib_device_manager *dm)
{
	if (!dm)
		return;

	if (dm->protocol_handle2)
		((struct protocol_logic_manager *)dm->protocol_handle2)->uninit(dm->protocol_handle2);

	if (dm->protocol_handle1)
		((struct protocol_1_logic_manager *)dm->protocol_handle1)->uninit(dm->protocol_handle1);
}

//static int __init_protocol_handle(struct lib_device_manager *dm)
//{
//    if (!dm)
//        return 0;
//
//	if (dm->protocol_type == 2) {
//		if (!dm->protocol_handle2)
//			return 0;
//		protocol_logic_manager_construct(dm->protocol_handle2);
//		((struct protocol_logic_manager *)dm->protocol_handle2)->register_action_callback(dm->protocol_handle2, __action_cb_fn, _usb_write_cb_fn, dm);
//		return ((struct protocol_logic_manager *)dm->protocol_handle2)->init(dm->protocol_handle2);
//	}else {
//		if (!dm->protocol_handle1)
//			return 0;
//		protocol_1_logic_manager_construct(dm->protocol_handle1);
//		((struct protocol_1_logic_manager *)dm->protocol_handle1)->register_action_callback(dm->protocol_handle1, __action_cb_fn, _usb_write_cb_fn, dm);
//		return ((struct protocol_1_logic_manager *)dm->protocol_handle1)->init(dm->protocol_handle1);
//	}
//}
//
//static void __uninit_protocol_handle(struct lib_device_manager *dm)
//{
//    if (!dm)
//        return;
//
//	if (dm->protocol_type == 2) {
//		if (dm->protocol_handle2) {
//			((struct protocol_logic_manager *)dm->protocol_handle2)->uninit(dm->protocol_handle2);
//		}
//	}
//	else {
//		if (dm->protocol_handle1)
//		((struct protocol_1_logic_manager *)dm->protocol_handle1)->uninit(dm->protocol_handle1);
//	}
//}

static void __check_protocol_version(struct lib_device_manager *dm)
{
	if (dm->protocol_type == 2)
	{
		dm->is_timeout = 1;
		//log_d("__check_protocol_version \n");
		dm->get_protocol_version(dm);
		int loopPc = 0;
		while (loopPc++ < 60) {
			Sleep(100);
			/*Sleep(50);*/
			if (0 == dm->is_timeout)
				return;
		}

		if (dm->is_timeout == 1)
		{
			log_d("read protocolVersion timeout \n");
			//	 dm->protocol_handler_valid_flag = 0;
			//	__uninit_protocol_handle(dm);
			//	dm->is_new_protocol = false;
			dm->protocol_type = 1;
			//	__init_protocol_handle(dm);
			//	dm->protocol_handler_valid_flag = 1;
		}
	}
}

static void _open_device(struct lib_device_manager *dm)
{
	// __init_protocol_handle(dm);
//	dm->protocol_handler_valid_flag = 1;
	dm->protocol_type = 2;

	if (dm->known_protocol_type == -1)
		__check_protocol_version(dm);
	else if (dm->known_protocol_type == 0)
		dm->protocol_type = 1;
}

static void common_protocol_parse_cb_fn(void* param, unsigned char cmd, unsigned char sequence, unsigned char *data, unsigned int size) {

	//printf("common_protocol_parse_cb_fn: %d \n", sequence);

	struct lib_device_manager *dm = param;
	// if (dm->protocol_handler_valid_flag) 
	{
		if (dm->protocol_type == 2)
		{
			if (dm->protocol_handle2)
				((struct protocol_logic_manager *)dm->protocol_handle2)->add_data(dm->protocol_handle2, cmd, sequence, data, size);
		}
		else {
			if (dm->protocol_handle1)
				((struct protocol_1_logic_manager *)dm->protocol_handle1)->add_data(dm->protocol_handle1, cmd, sequence, data, size);
		}
	}
}

static DWORD WINAPI __msgManager_thread_func(LPVOID param) {

	struct lib_device_manager *dm = param;
	if (!dm)
		return 0;
	MessageInfo info;
	while (1) {
		memset(&info, 0, sizeof(info));
		if (dm->msgManager.recv(&dm->msgManager, &info)) {
			__action_cb_fn(dm, info.cmd, info.mode, info.channel, info.data, info.len);
		}
		else {
			Sleep(1);
		}
	}
	return 0;
}

static void _create_thread_msgManager(struct lib_device_manager *dm) {
	if (!dm)
		return;

	LPDWORD dwThreadId = NULL;
	dm->pthread_handle = CreateThread(NULL, 0, __msgManager_thread_func, dm, 0, dwThreadId);
}

static void _destory_thread_msgManager(struct lib_device_manager *dm) {

	if (!dm)
		return;

	if (dm->pthread_handle) {
		TerminateThread(dm->pthread_handle, 0);
	}
}

static void _init(struct lib_device_manager *dm,
	libmouse_write_cb_fn write_cb,
	libmouse_event_cb_fn event_cb,
	libmouse_action_cb_fn action_cb,
	libmouse_audio_cb_fn audio_cb,
	libmouse_info_cb_fn info_cb, void *param)
{
	dm->base = param;
	dm->libmouse_write_cb = write_cb;
	dm->libmouse_event_cb = event_cb;
	dm->libmouse_action_cb = action_cb;
	dm->libmouse_audio_cb = audio_cb;
	dm->libmouse_info_cb = info_cb;

	//dm->protocol_handler_valid_flag = 0;

	if (!dm->protocol_handle1)
		dm->protocol_handle1 = mblue_malloc(sizeof(struct protocol_1_logic_manager));

	if (!dm->protocol_handle2)
		dm->protocol_handle2 = mblue_malloc(sizeof(struct protocol_logic_manager));

	dm->msgManager.init(&dm->msgManager);

	__init_protocol_handle(dm);

	_create_thread_msgManager(dm);
	dm->commonProtocol.init(&dm->commonProtocol, __common_action_cb_fn, common_protocol_parse_cb_fn, dm);
	//	_open_device(dm);
}

static void _uninit(struct lib_device_manager *dm)
{
	__uninit_protocol_handle(dm);

	dm->commonProtocol.uninit(&dm->commonProtocol);

	if (dm->protocol_handle1)
	{
		mblue_free(dm->protocol_handle1);
		dm->protocol_handle1 = 0;
	}

	if (dm->protocol_handle2)
	{
		mblue_free(dm->protocol_handle2);
		dm->protocol_handle2 = 0;
	}
	if (dm->ota_param.firmware)
	{
		free(dm->ota_param.firmware);
		dm->ota_param.firmware = NULL;
	}

	_destory_thread_msgManager(dm);

	dm->msgManager.uninit(&dm->msgManager);

}

static void _set_dpi(struct lib_device_manager *dm, unsigned int dpi_level)
{
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2)
			((struct protocol_logic_manager *)dm->protocol_handle2)->set_dpi(dm->protocol_handle2, dpi_level);
	}
	else {
		if (dm->protocol_handle1)
			((struct protocol_1_logic_manager *)dm->protocol_handle1)->set_dpi(dm->protocol_handle1, dpi_level);
	}
}

static void _get_dpi(struct lib_device_manager *dm)
{
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2)
			((struct protocol_logic_manager *)dm->protocol_handle2)->get_dpi(dm->protocol_handle2);
	}
	else {
		if (dm->protocol_handle1)
			((struct protocol_1_logic_manager *)dm->protocol_handle1)->get_dpi(dm->protocol_handle1);
	}
}

static void _start_record(struct lib_device_manager *dm)
{
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2)
			((struct protocol_logic_manager *)dm->protocol_handle2)->start_record(dm->protocol_handle2);
	}
	else {
		if (dm->protocol_handle1)
			((struct protocol_1_logic_manager *)dm->protocol_handle1)->start_record(dm->protocol_handle1);
	}
}

static void _stop_record(struct lib_device_manager *dm)
{
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2)
			((struct protocol_logic_manager *)dm->protocol_handle2)->stop_record(dm->protocol_handle2);
	}
	else {
		if (dm->protocol_handle1)
			((struct protocol_1_logic_manager *)dm->protocol_handle1)->stop_record(dm->protocol_handle1);
	}
}

static void _get_battery(struct lib_device_manager *dm)
{
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2)
			((struct protocol_logic_manager *)dm->protocol_handle2)->get_battery(dm->protocol_handle2);
	}
	else {
		if (dm->protocol_handle1)
			((struct protocol_1_logic_manager *)dm->protocol_handle1)->get_battery(dm->protocol_handle1);
	}
}

static void __get_version(struct lib_device_manager *dm, char cmd)
{
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2)
			((struct protocol_logic_manager *)dm->protocol_handle2)->get_version(dm->protocol_handle2, cmd);
	}
	else {
		if (dm->protocol_handle1)
			((struct protocol_1_logic_manager *)dm->protocol_handle1)->get_version(dm->protocol_handle1, cmd);
	}
}

static void __get_mac(struct lib_device_manager *dm, char cmd)
{
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2)
			((struct protocol_logic_manager *)dm->protocol_handle2)->get_mac_address(dm->protocol_handle2, cmd);
	}
	else {
		if (dm->protocol_handle1)
			((struct protocol_1_logic_manager *)dm->protocol_handle1)->get_mac_address(dm->protocol_handle1, cmd);
	}
}

static void _get_device_version(struct lib_device_manager *dm)
{
	__get_version(dm, VERSION_MOURSE_DATA);
}

static void _get_slave_device_version(struct lib_device_manager *dm)
{
	__get_version(dm, SLAVE_DEVICE_VERSION);
}
static void _get_device_macaddr(struct lib_device_manager *dm)
{
	__get_mac(dm, MAC_MOURSE_DATA);
}

static void _get_dongle_version(struct lib_device_manager *dm)
{
	__get_version(dm, VERSION_DONGLE_DATA);
}

static void _get_dongle_macaddr(struct lib_device_manager *dm)
{
	__get_mac(dm, MAC_DONGLE_DATA);
}

static void _handshake(struct lib_device_manager *dm)
{
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2)
			((struct protocol_logic_manager *)dm->protocol_handle2)->handshake(dm->protocol_handle2);
	}
	else {
		if (dm->protocol_handle1)
			((struct protocol_1_logic_manager *)dm->protocol_handle1)->handshake(dm->protocol_handle1);
	}
}

static void _get_device_productNo(struct lib_device_manager *dm)
{
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2)
			((struct protocol_logic_manager *)dm->protocol_handle2)->get_productNo(dm->protocol_handle2);
	}
	else {
		if (dm->protocol_handle1)
			((struct protocol_1_logic_manager *)dm->protocol_handle1)->get_productNo(dm->protocol_handle1);
	}
}

static void _get_protocol_version(struct lib_device_manager *dm)
{
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2)
			((struct protocol_logic_manager *)dm->protocol_handle2)->get_protocol_version(dm->protocol_handle2);
	}
	else {
		if (dm->protocol_handle1)
			((struct protocol_1_logic_manager *)dm->protocol_handle1)->get_protocol_version(dm->protocol_handle1);
	}
}

static void _set_record_channel_size(struct lib_device_manager *dm, int size)
{
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2)
			((struct protocol_logic_manager *)dm->protocol_handle2)->set_record_channel(dm->protocol_handle2, size);
	}
	else {
		if (dm->protocol_handle1)
			((struct protocol_1_logic_manager *)dm->protocol_handle1)->set_record_channel(dm->protocol_handle1, size);
	}
}

static void _set_sleep_time(struct lib_device_manager *dm, int time) {
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2)
			((struct protocol_logic_manager *)dm->protocol_handle2)->set_sleep_time(dm->protocol_handle2, time);
	}
}

static void _set_client_finger(struct lib_device_manager *dm) {

	unsigned char finger[16] = { 0 };
	_get_client_finger(dm, finger);

	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2)
			((struct protocol_logic_manager *)dm->protocol_handle2)->set_client_finger(dm->protocol_handle2, finger, sizeof(finger));
	}
	else {
		if (dm->protocol_handle1)
			((struct protocol_1_logic_manager *)dm->protocol_handle1)->set_client_finger(dm->protocol_handle1, finger, sizeof(finger));
	}
}

static void _set_os_type(struct lib_device_manager *dm, unsigned int type) {
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2)
			((struct protocol_logic_manager *)dm->protocol_handle2)->set_os_type(dm->protocol_handle2, type);
	}
}

//#define AUTHENTICATION_SLEEP_TIME			(100)

#define AUTHENTICATION_SLEEP_TIME			(300)
#define SLEEP_TIME							(200)
#define RETRY_MAX_COUNT						(5)


static void _start_authentication(struct lib_device_manager *dm)
{
	((struct protocol_logic_manager *)dm->protocol_handle2)->clear_command_map(dm->protocol_handle2);
	_open_device(dm);

	if (dm->protocol_type == 1) {
		_handshake(dm);
		/*if (NULL != dm->libmouse_action_cb)
			dm->libmouse_action_cb(LIBMOUSE_ACTION_AUTHENTICATION,0xFE);*/
		return;
	}

	struct protocol_logic_manager *lm = dm->protocol_handle2;
	if (NULL == lm)
		return;

	dm->authentication_step = 0;
	//_set_os_type(dm, OS_TYPE_WINDOWS);
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	_set_client_finger(dm);
	Sleep(AUTHENTICATION_SLEEP_TIME);
	int retry_count = 0;
	int step_retry_count = 0;
	step_retry_count++;

	while (!(dm->authentication_step & setClientFingerStep)) {
		if (step_retry_count > RETRY_MAX_COUNT)
			goto EXIT_LABEL;

		if (dm->authentication_step & deviceDisconnect) {
			if (NULL != dm->libmouse_action_cb) {
				printf("deviceDisconnect \n");
				dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_HANDSHAKE, 0);
			}
			return;
		}

		//printf("set client finger step timeout \n");
		retry_count++;
		Sleep(AUTHENTICATION_SLEEP_TIME);
		if (retry_count > RETRY_MAX_COUNT) {
			//((struct protocol_logic_manager *)dm->protocol_handle2)->clear_command_map(dm->protocol_handle2);
			_set_client_finger(dm);
			step_retry_count++;
			retry_count = 0;
		}
	}

	if (dm->authentication_step & deviceDisconnect) {
		if (NULL != dm->libmouse_action_cb) {
			printf("deviceDisconnect \n");
			dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_HANDSHAKE, 0);
		}
		return;
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

		//printf("get_device_finger \n");
	lm->get_device_finger(lm);
	Sleep(AUTHENTICATION_SLEEP_TIME);
	retry_count = 0;
	step_retry_count = 1;
	while (!(dm->authentication_step & getDeviceFingerStep)) {
		if (step_retry_count > RETRY_MAX_COUNT)
			goto EXIT_LABEL;

		if (dm->authentication_step & deviceDisconnect) {
			if (NULL != dm->libmouse_action_cb) {
				printf("deviceDisconnect \n");
				dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_HANDSHAKE, 0);
			}
			return;
		}

		//printf("get client finger step timeout \n");
		retry_count++;
		Sleep(AUTHENTICATION_SLEEP_TIME);
		if (retry_count >= RETRY_MAX_COUNT) {
			//((struct protocol_logic_manager *)dm->protocol_handle2)->clear_command_map(dm->protocol_handle2);
			lm->get_device_finger(lm);
			step_retry_count++;
			retry_count = 0;
		}
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	dm->get_device_macaddr(dm);
	Sleep(AUTHENTICATION_SLEEP_TIME);
	retry_count = 0;
	step_retry_count = 1;
	while (!(dm->authentication_step & getDeviceMacStep)) {
		if (step_retry_count >= RETRY_MAX_COUNT)
			goto EXIT_LABEL;

		if (dm->authentication_step & deviceDisconnect) {
			if (NULL != dm->libmouse_action_cb) {
				printf("deviceDisconnect \n");
				dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_HANDSHAKE, 0);
			}
			return;
		}

		//printf("get mac step timeout \n");
		retry_count++;
		Sleep(AUTHENTICATION_SLEEP_TIME);
		if (retry_count > RETRY_MAX_COUNT) {
			//((struct protocol_logic_manager *)dm->protocol_handle2)->clear_command_map(dm->protocol_handle2);
			dm->get_device_macaddr(dm);
			step_retry_count++;
			retry_count = 0;
		}
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	dm->get_device_version(dm);
	Sleep(AUTHENTICATION_SLEEP_TIME);
	retry_count = 0;
	step_retry_count = 1;
	while (!(dm->authentication_step & getDeviceVersionStep)) {
		if (step_retry_count >= RETRY_MAX_COUNT)
			goto EXIT_LABEL;
		//printf("get version step timeout \n");

		if (dm->authentication_step & deviceDisconnect) {
			if (NULL != dm->libmouse_action_cb) {
				printf("deviceDisconnect \n");
				dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_HANDSHAKE, 0);
			}
			return;
		}

		retry_count++;
		Sleep(AUTHENTICATION_SLEEP_TIME);
		if (retry_count >= RETRY_MAX_COUNT) {
			//((struct protocol_logic_manager *)dm->protocol_handle2)->clear_command_map(dm->protocol_handle2);
			dm->get_device_version(dm);
			step_retry_count++;
			retry_count = 0;
		}
	}

	if (dm->authentication_step & deviceDisconnect) {
		if (NULL != dm->libmouse_action_cb) {
			printf("deviceDisconnect \n");
			dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_HANDSHAKE, 0);
		}
		return;
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	step_retry_count = 1;
	//printf("get handshake \n");
	_handshake(dm);

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

EXIT_LABEL:
	if (step_retry_count > RETRY_MAX_COUNT) {
		if (NULL != dm->libmouse_action_cb)
			dm->libmouse_action_cb(dm->base, LIBMOUSE_ACTION_AUTHENTICATION, 0);
	}
	return;
}

static void _append_data(struct lib_device_manager *dm, unsigned char *data, unsigned int size) {

	//	printf("append_data: %d \n", size);
	dm->commonProtocol.append(&dm->commonProtocol, data, size);

	/*if (!dm->protocol_handler_valid_flag)
		return;

	if (dm->protocol_type == 2)
	{
		if (dm->protocol_handle2)
			((struct protocol_logic_manager *)dm->protocol_handle2)->add_data(dm->protocol_handle2, data, size);
	}
	else {
		if (dm->protocol_handle1)
			((struct protocol_1_logic_manager *)dm->protocol_handle1)->add_data(dm->protocol_handle1, data, size);
	}*/
}

static void _set_led_rgb(struct lib_device_manager *dm, unsigned char type, unsigned int duration, unsigned char *data, int size)
{
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2)
			((struct protocol_logic_manager *)dm->protocol_handle2)->set_led_rgb(dm->protocol_handle2, type, duration, data, size);
	}
}

static void _set_vibration(struct lib_device_manager *dm, unsigned char status, unsigned char type, unsigned char level, unsigned int duration) {
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2)
			((struct protocol_logic_manager *)dm->protocol_handle2)->set_vibration(dm->protocol_handle2, status, type, level, duration);
	}
}

static void _set_custom_key(struct lib_device_manager *dm, unsigned char type, unsigned char key_index, unsigned char key_type, unsigned char *data, unsigned int size) {
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2)
			((struct protocol_logic_manager *)dm->protocol_handle2)->set_custom_key(dm->protocol_handle2, type, key_index, key_type, data, size);
	}
}

static 	void _start_sincheck(struct lib_device_manager *dm) {
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2)
			((struct protocol_logic_manager *)dm->protocol_handle2)->start_sincheck(dm->protocol_handle2);
	}
}

static void _get_theme(struct lib_device_manager *dm) {
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2)
			((struct protocol_logic_manager *)dm->protocol_handle2)->get_theme(dm->protocol_handle2);
	}
}

static void _set_handshake_info(struct lib_device_manager *dm, unsigned int flag) {
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2)
			((struct protocol_logic_manager *)dm->protocol_handle2)->set_handshake_info(dm->protocol_handle2, flag);
	}
}

static void _set_flip_page(struct lib_device_manager *dm, unsigned int value) {
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2)
			((struct protocol_logic_manager *)dm->protocol_handle2)->set_flip_page(dm->protocol_handle2, value);
	}
}

static void _get_flip_page(struct lib_device_manager *dm) {
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2)
			((struct protocol_logic_manager *)dm->protocol_handle2)->get_flip_page(dm->protocol_handle2);
	}
}

static void _get_subtitle_status(struct lib_device_manager *dm) {
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2)
			((struct protocol_logic_manager *)dm->protocol_handle2)->get_subtitle_status(dm->protocol_handle2);
	}
}

static void _get_custom_key(struct lib_device_manager *dm, unsigned char index) {
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2)
			((struct protocol_logic_manager *)dm->protocol_handle2)->get_custom_key(dm->protocol_handle2, index);
	}
}

static void _get_work_mode(struct lib_device_manager *dm) {
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2)
			((struct protocol_logic_manager *)dm->protocol_handle2)->get_mouse_work_mode(dm->protocol_handle2);
	}
}

static void _get_connection_type(struct lib_device_manager *dm)
{
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2)
			((struct protocol_logic_manager *)dm->protocol_handle2)->get_mouse_connection_type(dm->protocol_handle2);
	}
}

static void get_firmware_key(struct lib_device_manager *dm, unsigned char* key, unsigned char *product_no, unsigned int len)
{
	*key = '{';
	memcpy(key + 1, (u8 *)&len, 4);
	*(key + 5) = '{';
	memcpy(key + 6, product_no, 4);
	*(key + 10) = '}';
	u32 hash_key = dm->hash.sdbm_mem(&dm->hash, key + 1, 10);
	memcpy(key + 11, (u8 *)&hash_key, 4);
	*(key + 15) = '}';
}

static unsigned char _check_the_firmware(struct lib_device_manager *dm, unsigned char *firmware, unsigned int size)
{
	if (firmware == NULL || size < 32 || ((size - 8) % 16) != 0)
		return 0;
	unsigned int file_size = 0;
	unsigned char product_NO[4] = { 0 };
	unsigned char session_key[16] = { 0 };
	memcpy(product_NO, firmware, 4);
	memcpy((u8 *)&file_size, firmware + 4, 4);
	get_firmware_key(dm, session_key, product_NO, file_size);
	struct aes_ctx aesctx;
	struct mblue_aes aes;
	mblue_aes_construct(&aes);
	aes.init_ctx(&aes, &aesctx, session_key);

	dm->ota_param.firmware_len = size - 8;
	if (file_size != dm->ota_param.firmware_len || memcmp(product_NO, dm->ota_param.product_NO, 4))
		return 0;
	dm->ota_param.firmware = (unsigned char *)malloc(sizeof(unsigned char) * dm->ota_param.firmware_len);
	for (unsigned int i = 0; i < dm->ota_param.firmware_len; i += 16)
	{
		memcpy(dm->ota_param.firmware + i, firmware + 8 + i, 16);
		aes.ecb_decrypt(&aes, &aesctx, dm->ota_param.firmware + i);
	}
	return 1;
}

static void _start_ota(struct lib_device_manager *dm, unsigned char *session, unsigned char *firmware, unsigned int size) {
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2) {
			dm->ota_param.firmware_hash_sum = 0;
			dm->ota_param.firmware_crc32_sum = 0;
			dm->ota_param.firmware_index = 0;
			if (dm->ota_param.firmware)
			{
				free(dm->ota_param.firmware);
				dm->ota_param.firmware = NULL;
			}
			_get_common_sessionkey(dm, dm->ota_param.session_key, dm->device_mac, dm->device_version);
			unsigned char decrypt_buf[16] = { 0 };
			memcpy(decrypt_buf, session, 16);
			struct aes_ctx aesctx;
			struct mblue_aes aes;
			mblue_aes_construct(&aes);
			aes.init_ctx(&aes, &aesctx, dm->ota_param.session_key);
			aes.ecb_decrypt(&aes, &aesctx, decrypt_buf);
			memcpy(dm->ota_param.session, decrypt_buf, 16);
			if (_check_the_firmware(dm, firmware, size))
				((struct protocol_logic_manager *)dm->protocol_handle2)->start_ota(dm->protocol_handle2, session);
			else dm->end_ota(dm, 0);
		}
	}
}

static void _update_ota_data(struct lib_device_manager *dm, unsigned char *data, unsigned int size) {
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2 && size == PART_FIRMWARE_SIZE) {
			unsigned char temp_buf[32] = { 0 };
			for (int i = 0; i < 16; i++) {
				temp_buf[i] = *(data + i) ^ dm->ota_param.session_key[i];
			}
			struct aes_ctx aesctx;
			struct mblue_aes aes;
			mblue_aes_construct(&aes);
			aes.init_ctx(&aes, &aesctx, dm->ota_param.session);
			aes.ecb_encrypt(&aes, &aesctx, temp_buf);

			for (int i = 0; i < 16; i++) {
				temp_buf[i + 16] = *(data + i + 16) ^ dm->ota_param.session_key[i];
			}
			dm->ota_param.firmware_hash_sum += dm->hash.sdbm_mem(&dm->hash, data, PART_FIRMWARE_SIZE);
			dm->ota_param.firmware_crc32_sum += calculate_crc32(data, PART_FIRMWARE_SIZE);
			aes.ecb_encrypt(&aes, &aesctx, temp_buf + 16);
			((struct protocol_logic_manager *)dm->protocol_handle2)->update_ota_data(dm->protocol_handle2, temp_buf, size);
		}

	}
}

static void _end_ota(struct lib_device_manager *dm, unsigned int value) {
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2) {
			if (value == 1)
				((struct protocol_logic_manager *)dm->protocol_handle2)->end_ota(dm->protocol_handle2, 1, dm->ota_param.firmware_crc32_sum);
			else
				((struct protocol_logic_manager *)dm->protocol_handle2)->end_ota(dm->protocol_handle2, 0, 0);
		}
	}
}

static void _set_polling_rate(struct lib_device_manager *dm, unsigned int value)
{
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2) {
			((struct protocol_logic_manager *)dm->protocol_handle2)->set_polling_rate(dm->protocol_handle2, value);
		}
	}
}

static void _get_polling_rate(struct lib_device_manager *dm)
{
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2) {
			((struct protocol_logic_manager *)dm->protocol_handle2)->get_polling_rate(dm->protocol_handle2);
		}
	}
}

static void _get_dongle_product_no(struct lib_device_manager *dm)
{
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2) {
			((struct protocol_logic_manager *)dm->protocol_handle2)->get_dongle_product_no(dm->protocol_handle2);
		}
	}
}

static void _get_led_intensity(struct lib_device_manager *dm, uint8_t index)
{
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2) {
			((struct protocol_logic_manager *)dm->protocol_handle2)->get_led_intensity(dm->protocol_handle2,index);
		}
	}
}

static void _set_led_intensity(struct lib_device_manager *dm, uint8_t index, uint8_t value)
{
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2) {
			((struct protocol_logic_manager *)dm->protocol_handle2)->set_led_intensity(dm->protocol_handle2, index,value);
		}
	}
}

static void _get_rgb_backlight_param(struct lib_device_manager *dm, uint8_t index)
{
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2) {
			((struct protocol_logic_manager *)dm->protocol_handle2)->get_rgb_backlight_param(dm->protocol_handle2, index);
		}
	}
}

static void _set_rgb_backlight_param(struct lib_device_manager *dm, uint8_t index, uint8_t value)
{
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2) {
			((struct protocol_logic_manager *)dm->protocol_handle2)->set_rgb_backlight_param(dm->protocol_handle2, index, value);
		}
	}
}

static void _get_laser_mode(struct lib_device_manager *dm)
{
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2) {
			((struct protocol_logic_manager *)dm->protocol_handle2)->get_laser_mode(dm->protocol_handle2);
		}
	}
}

static void _set_laser_mode(struct lib_device_manager *dm, unsigned char value)
{
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2) {
			((struct protocol_logic_manager *)dm->protocol_handle2)->set_laser_mode(dm->protocol_handle2,value);
		}
	}
}

static void _get_wheel_mode(struct lib_device_manager *dm)
{
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2) {
			((struct protocol_logic_manager *)dm->protocol_handle2)->get_wheel_mode(dm->protocol_handle2);
		}
	}
}

static void _set_wheel_mode(struct lib_device_manager *dm, unsigned char value)
{
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2) {
			((struct protocol_logic_manager *)dm->protocol_handle2)->set_wheel_mode(dm->protocol_handle2, value);
		}
	}
}

static void _heart_beat(struct lib_device_manager *dm, unsigned char connect_status)
{
	if (dm->protocol_handle2) {
		((struct protocol_logic_manager *)dm->protocol_handle2)->heart_beat(dm->protocol_handle2, connect_status);
	}
}

static void _player_play(struct lib_device_manager *dm, unsigned int tone_index)
{
	if (dm->protocol_handle2) {
		((struct protocol_logic_manager *)dm->protocol_handle2)->player_play(dm->protocol_handle2, tone_index);
	}
}

static void _set_player_volume(struct lib_device_manager *dm, unsigned int volume)
{
	if (dm->protocol_handle2) {
		((struct protocol_logic_manager *)dm->protocol_handle2)->set_player_volume(dm->protocol_handle2, volume);
	}
}

static void _get_player_volume(struct lib_device_manager *dm)
{
	if (dm->protocol_handle2) {
		((struct protocol_logic_manager *)dm->protocol_handle2)->get_player_volume(dm->protocol_handle2);
	}
}

static void _set_time_heart_beat(struct lib_device_manager *dm, unsigned int time)
{
	if (dm->protocol_handle2) {
		((struct protocol_logic_manager *)dm->protocol_handle2)->set_time_heart_beat(dm->protocol_handle2, time);
	}
}

static void _get_time_heart_beat(struct lib_device_manager *dm)
{
	if (dm->protocol_handle2) {
		((struct protocol_logic_manager *)dm->protocol_handle2)->get_time_heart_beat(dm->protocol_handle2);
	}
}

static void _set_keyboard_custom_rgb(struct lib_device_manager *dm, unsigned char *data, unsigned int size)
{
	if (dm->protocol_handle2) {
		((struct protocol_logic_manager *)dm->protocol_handle2)->set_keyboard_custom_rgb(dm->protocol_handle2, data, size);
	}
}

static void _set_adaptive_modulation(struct lib_device_manager *dm, unsigned int value)
{
	if (dm->protocol_handle2) {
		((struct protocol_logic_manager *)dm->protocol_handle2)->set_adaptive_modulation(dm->protocol_handle2,value);
	}
}

static void _get_adaptive_modulation(struct lib_device_manager *dm)
{
	if (dm->protocol_handle2) {
		((struct protocol_logic_manager *)dm->protocol_handle2)->get_adaptive_modulation(dm->protocol_handle2);
	}
}

static void _get_ls_device_id(struct lib_device_manager *dm)
{
	if (dm->protocol_handle2) {
		((struct protocol_logic_manager *)dm->protocol_handle2)->get_ls_device_id(dm->protocol_handle2);
	}
}

static void _get_ls_check_sum(struct lib_device_manager *dm, unsigned int time)
{
	if (dm->protocol_handle2) {
		((struct protocol_logic_manager *)dm->protocol_handle2)->get_ls_check_sum(dm->protocol_handle2, time);
	}
}
static void __install_lib_device_manager(struct lib_device_manager *dm)
{
	dm->init = _init;
	dm->uninit = _uninit;
	dm->set_dpi = _set_dpi;
	dm->get_dpi = _get_dpi;
	dm->start_record = _start_record;
	dm->stop_record = _stop_record;
	dm->get_battery = _get_battery;
	dm->get_device_version = _get_device_version;
	dm->get_device_macaddr = _get_device_macaddr;
	dm->get_dongle_version = _get_dongle_version;
	dm->get_dongle_macaddr = _get_dongle_macaddr;
	dm->handshake = _start_authentication;
	dm->get_device_productNo = _get_device_productNo;
	dm->get_protocol_version = _get_protocol_version;
	dm->set_record_channel_size = _set_record_channel_size;
	dm->set_sleep_time = _set_sleep_time;
	//  dm->start_authentication = _start_authentication;
	dm->append_data = _append_data;
	dm->set_vibration = _set_vibration;
	dm->set_led_rgb = _set_led_rgb;
	dm->set_custom_key = _set_custom_key;
	dm->start_sincheck = _start_sincheck;
	dm->get_theme = _get_theme;
	dm->set_handshake_info = _set_handshake_info;
	dm->set_flip_page = _set_flip_page;
	dm->get_flip_page = _get_flip_page;
	dm->get_subtitle_satus = _get_subtitle_status;
	dm->get_custom_key = _get_custom_key;
	dm->get_work_mode = _get_work_mode;
	dm->get_connection_type = _get_connection_type;
	dm->start_ota = _start_ota;
	dm->update_ota_data = _update_ota_data;
	dm->end_ota = _end_ota;
	dm->set_polling_rate = _set_polling_rate;
	dm->get_polling_rate = _get_polling_rate;
	dm->get_dongle_product_no = _get_dongle_product_no;
	dm->set_led_intensity = _set_led_intensity;
	dm->get_led_intensity = _get_led_intensity;
	dm->get_rgb_backlight_param = _get_rgb_backlight_param;
	dm->set_rgb_backlight_param = _set_rgb_backlight_param;
	dm->get_laser_mode = _get_laser_mode;
	dm->set_laser_mode = _set_laser_mode;
	dm->get_slave_device_version = _get_slave_device_version;
	dm->get_wheel_mode = _get_wheel_mode;
	dm->set_wheel_mode = _set_wheel_mode;

	dm->heart_beat = _heart_beat;
	dm->player_play = _player_play;
	dm->set_player_volume = _set_player_volume;
	dm->get_player_volume = _get_player_volume;
	dm->set_time_heart_beat = _set_time_heart_beat;
	dm->get_time_heart_beat = _get_time_heart_beat;
	dm->set_keyboard_custom_rgb = _set_keyboard_custom_rgb;
	dm->set_adaptive_modulation = _set_adaptive_modulation;
	dm->get_adaptive_modulation = _get_adaptive_modulation;
	dm->get_ls_device_id = _get_ls_device_id;
	dm->get_ls_check_sum = _get_ls_check_sum;

	dm->set_os_type = _set_os_type;
	dm->protocol_type = 2;
	dm->protocol_handle1 = 0;
	dm->protocol_handle2 = 0;
	dm->known_protocol_type = -1;
	dm->ota_param.firmware = NULL;
	memset(dm->client_finger, 0x0, sizeof(dm->client_finger));
	mblue_aes_construct(&dm->aes);
	mblue_hash_construct(&dm->hash);
	mblue_md5_construct(&dm->md5);
	MadaoMessageConstruct(&dm->msgManager);
	CommonProtocolManagerConstruct(&dm->commonProtocol);
}

void lib_get_device_manager(struct lib_device_manager *dm)
{
	if (dm)
	{
		__install_lib_device_manager(dm);
	}
}