/*
 * @Author: xuhan
 * @Date: 2020-02-08 15:12:53
 * @LastEditors  : xuhan
 * @LastEditTime : 2020-02-10 20:09:47
 * @Description: 
 */

#include <string.h>
#include<Windows.h>
#include <time.h>

#include "lib_device_manager.h"
#include "protocol_2_logic_manager.h"
#include "protocol_1_logic_manager.h"
#include "mblue_heap.h"
#include "include/lib_speech_usb.h"
#include "include/libspeech_voice.h"
#include "libprotocol_logic_define.h"


#define MAX_COMMAND_SIZE 128

enum authenticateStep {
	setClientFingerStep = 0x1,
	getDeviceFingerStep = 0x2,
	getDeviceMacStep = 0x4,
	getDeviceVersionStep = 0x8
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
	while ((temp = *chare++) != '\0') { /*遍历chare字符串*/
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
	WCHAR szFetCmd[] = L"wmic BaseBoard get SerialNumber"; // 获取主板序列号命令行	
	char strEnSearch[] = { "SerialNumber" }; // 主板序列号的前导信息

	BOOL   bret = FALSE;
	HANDLE hReadPipe = NULL; //读取管道
	HANDLE hWritePipe = NULL; //写入管道	
	PROCESS_INFORMATION pi;   //进程信息	
	STARTUPINFO			si;	  //控制命令行窗口信息
	SECURITY_ATTRIBUTES sa;   //安全属性

	char			szBuffer[MAX_COMMAND_SIZE + 1] = { 0 }; // 放置命令行结果的输出缓冲区
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

	//1.0 创建管道
	bret = CreatePipe(&hReadPipe, &hWritePipe, &sa, 0);
	if (!bret)
	{
		goto END;
	}

	//2.0 设置命令行窗口的信息为指定的读写管道
	GetStartupInfo(&si);
	si.hStdError = hWritePipe;
	si.hStdOutput = hWritePipe;
	si.wShowWindow = SW_HIDE; //隐藏命令行窗口
	si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;

	//3.0 创建获取命令行的进程
	bret = CreateProcess(NULL, szFetCmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
	if (!bret)
	{
		goto END;
	}

	//4.0 读取返回的数据
	WaitForSingleObject(pi.hProcess, 500/*INFINITE*/);
	bret = ReadFile(hReadPipe, szBuffer, MAX_COMMAND_SIZE, &count, 0);
	if (!bret)
	{
		goto END;
	}

	//5.0 查找主板序列号
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

	//去掉中间的空格 \r \n
	for (int i = 0; i < strlen(strBuffer); i++)
	{
		if (strBuffer[i] != ' ' && strBuffer[i] != '\n' && strBuffer[i] != '\r')
		{
			lpszBaseBoard[j] = strBuffer[i];
			j++;
		}
	}

	bret = TRUE;

END:
	//关闭所有的句柄
	CloseHandle(hWritePipe);
	CloseHandle(hReadPipe);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return(bret);
}


static void _get_client_finger(struct lib_device_manager *dm, unsigned char *finger) {
	
	char board_uuid[128] = { 0 };
	get_board_serialnumber(board_uuid, sizeof(board_uuid));
	
	md5_ctx md5ctx;
	dm->md5.init(&dm->md5, &md5ctx);
	dm->md5.update(&dm->md5, &md5ctx, (unsigned char*)board_uuid, sizeof(board_uuid));
	dm->md5.final(&dm->md5, &md5ctx, finger);
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


static struct libspeech_voice *__get_voice_handle(void *handle)
{
    return ((struct libspeech_voice *)handle);
}

static void ___authentication_action(struct lib_device_manager *dm, int phase, char *data, int len)
{
    if (!dm || !data)
        return;

    if (phase == PHASE1)
    {

		if (strlen(dm->device_mac) == 0 || strlen(dm->device_finger) == 0 || strlen(dm->device_version))
			return;

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
				((struct protocol_1_logic_manager *)dm->protocol_handle1)->set_authenticate(dm->protocol_handle1, data, len , NULL, NULL, NULL, NULL);
		}

		/*unsigned char *input = getInfo();
		if (dm->protocol_type == 2) {
			if (dm->protocol_handle2)
				((struct protocol_logic_manager *)dm->protocol_handle2)->set_authenticate(dm->protocol_handle2, input, 8);
		}
		else {
			if (dm->protocol_handle1)
				((struct protocol_1_logic_manager *)dm->protocol_handle1)->set_authenticate(dm->protocol_handle1, input, 8);
		}*/
    }
    else if (phase == PHASE3)
    {
		if (len != 16)
			return;

		if (strlen(dm->client_random) == 0)
			return;

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
				dm->libmouse_action_cb(dm->device, LIBMOUSE_ACTION_AUTHENTICATION, 1);
			}
			else {
				dm->libmouse_action_cb(dm->device, LIBMOUSE_ACTION_AUTHENTICATION, 0);
			}
		}

		/*if(dm->libmouse_action_cb)
			if (setInfo(data, len)) {
				dm->libmouse_action_cb(dm->device, LIBMOUSE_ACTION_AUTHENTICATION, 1);
			}
			else {
				dm->libmouse_action_cb(dm->device, LIBMOUSE_ACTION_AUTHENTICATION, 0);
			}*/
    }
}

static void __action_cb_fn(void *param, unsigned char cmd, int mode, int channel, char *data, int len)
{
    struct lib_device_manager *dm = param;

    switch (cmd)
    {
    case KEY_DOWN:
        dm->libmouse_event_cb(dm->device, LIBMOUSE_EVENT_KEY_DOWN);
        break;
    case KEY_UP:
        dm->libmouse_event_cb(dm->device, LIBMOUSE_EVENT_KEY_UP);
        break;
    case RECORD_DATA:
    {
		dm->libmouse_audio_cb(dm->device, channel, data, len);

		/*EnterCriticalSection(&dm->lock);
		struct voice_cache cache;
		cache.channel = channel;
		cache.len = len;
		memcpy(cache.data, data, len);
		dm->fifo->put(dm->fifo, &cache, sizeof(cache));
		LeaveCriticalSection(&dm->lock);
*/


   //      int ret = 0;
   //      unsigned short pcm_buffer[320] = {0};
   //      unsigned short pcm_len = 0;
		 //unsigned char temp[40] = { 0 };
		 //memcpy(temp, data, 40);
   //      if (channel == 0)
   //      {
   //          ret = __get_voice_handle(dm->voice_handle1)->decode(dm->voice_handle1, (unsigned short *)temp, pcm_buffer, &pcm_len);
   //      }
   //      else if (channel == 1)
   //      {
   //          ret = __get_voice_handle(dm->voice_handle2)->decode(dm->voice_handle2, (unsigned short *)temp, pcm_buffer, &pcm_len);
   //      }
   //      if (ret)
   //         dm->libmouse_audio_cb(dm->device, channel, pcm_buffer, pcm_len);
			 
    }
    break;
    case DPI_DATA:
    {
        if (len == 4 && data != NULL)
        {
            int cur_dpi_level = 0;
            memcpy(&cur_dpi_level, data, len);
            dm->libmouse_action_cb(dm->device, LIBMOUSE_ACTION_DPI, cur_dpi_level);
        }
    }
    break;
    case BATTER:
    {
        int cur_batter = 0;
        memcpy(&cur_batter, data, len);
        dm->libmouse_action_cb(dm->device, LIBMOUSE_ACTION_BATTERY, cur_batter);
    }
    break;
    case HOTPLUG:
        dm->libmouse_event_cb(dm->device, LIBMOUSE_EVENT_LEFT);
        break;
    case BLE_STATUS:
    {
        if (len == 4 && data != NULL)
        {
            int cur_ble_status = 0;
            memcpy(&cur_ble_status, data, len);
            if (cur_ble_status == 1)
                dm->libmouse_event_cb(dm->device, LIBMOUSE_EVENT_BLE_CONNECTED);
            else
                dm->libmouse_event_cb(dm->device, LIBMOUSE_EVENT_BLE_DISCONNECTED);
        }
    }

    break;
    case VERSION_MOURSE_DATA:
    {
        if (len == 4 && data != NULL)
        {
            dm->libmouse_info_cb(dm->device, LIBMOUSE_INFO_MOUSE_VERSION, data, len);
			memset(dm->device_version, 0x0, sizeof(dm->device_version));
			memcpy(dm->device_version, data, 6);
			dm->authentication_step |= getDeviceVersionStep;
        }
    }
    break;
    case VERSION_DONGLE_DATA:
    {
        if (len == 4 && data != NULL)
        {
            dm->libmouse_info_cb(dm->device, LIBMOUSE_INFO_DONGLE_VERSION, data, len);
        }
    }
    break;
    case MAC_MOURSE_DATA:
    {
        if (len == 6 && data != NULL)
        {
            dm->libmouse_info_cb(dm->device, LIBMOUSE_INFO_MOUSE_MACADDR, data, len);
			memset(dm->device_mac, 0x0, sizeof(dm->device_mac));
			memcpy(dm->device_mac, data, 6);
			dm->authentication_step |= getDeviceMacStep;
        }
    }
    break;
    case MAC_DONGLE_DATA:
    {
        if (len == 6 && data != NULL)
        {
            dm->libmouse_info_cb(dm->device, LIBMOUSE_INFO_DONGLE_MACADDR, data, len);
        }
    }
    break;
    case START_RECORD:
        dm->libmouse_action_cb(dm->device, LIBMOUSE_ACTION_START_RECORD, mode);
        break;
    case STOP_RECORD:
        dm->libmouse_action_cb(dm->device, LIBMOUSE_ACTION_STOP_RECORD, mode);
        break;
    case SET_DPI:
        dm->libmouse_action_cb(dm->device, LIBMOUSE_ACTION_SET_DPI, mode);
        break;
    case HANDSHAKE:
        dm->libmouse_action_cb(dm->device, LIBMOUSE_ACTION_HANDSHAKE, mode);
        break;
    case PRODUCT_NO:
        if (len == 4 && data != NULL)
        {
            dm->libmouse_info_cb(dm->device, LIBMOUSE_INFO_MOUSE_PRODUCTNO, data, len);
        }
        break;
    case DONGLE_PRODUCT_NO:
        if (len == 4 && data != NULL)
        {
            dm->libmouse_info_cb(dm->device, LIBMOUSE_INFO_MOUSE_PRODUCTNO, data, len);
        }
        break;
    case PROTOCOL_VERSION:
        if (len == 4 && data != NULL)
        {
            dm->is_new_protocol = 1;
            dm->is_timeout = 0;
            dm->libmouse_info_cb(dm->device, LIBMOUSE_INFO_PROTOCOL_VERSION, data, len);
        }
        break;
    case AUTHENTICATE:
        ___authentication_action(dm, mode, data, len);
        break;
	case DEVICE_FINGER:
	{
		if (len == 16 && data != NULL) {
			memset(dm->device_finger, 0x0, sizeof(dm->device_finger));
			memcpy(dm->device_finger, data, 16);
			dm->authentication_step |= getDeviceFingerStep;
		}
	}
		break;
	case CLIENT_FINGER: {
		if (mode == 1)
			dm->authentication_step |= setClientFingerStep;
		else
			dm->authentication_step &= ~setClientFingerStep;
	}
		break;
	case IFLY_AUTHENTICATE: {
		//if (setInfo(data, len)) {
		//	// ifly authentication success 
		//}
	}
		break;
    default:
        break;
    }
}

static void _usb_write_cb_fn(void *param, unsigned char *data, int size)
{
  //  printf("usb write data \n");
    struct lib_device_manager *dm = param;
    if (dm->usb_handle)
        dm->usb_handle->write(dm->usb_handle, data, size);
}

static void __lib_usb_cb_fn(void *param, unsigned char *data, unsigned int size)
{
    struct lib_device_manager *dm = param;
	if (dm->protocol_type == 2)
	{
		if(dm->protocol_handle2)
			((struct protocol_logic_manager *)dm->protocol_handle2)->add_data(dm->protocol_handle2, data, size);
	}
	else {
		if(dm->protocol_handle1)
			((struct protocol_1_logic_manager *)dm->protocol_handle1)->add_data(dm->protocol_handle1, data, size);
	}
       
}

static int __init_protocol_handle(struct lib_device_manager *dm)
{
    if (!dm)
        return 0;

	if (dm->protocol_type == 2) {
		if (!dm->protocol_handle2)
			return 0;
		protocol_logic_manager_construct(dm->protocol_handle2);
		((struct protocol_logic_manager *)dm->protocol_handle2)->register_action_callback(dm->protocol_handle2, __action_cb_fn, _usb_write_cb_fn, dm);
		return ((struct protocol_logic_manager *)dm->protocol_handle2)->init(dm->protocol_handle2);
	}else {
		if (!dm->protocol_handle1)
			return 0;
		protocol_1_logic_manager_construct(dm->protocol_handle1);
		((struct protocol_1_logic_manager *)dm->protocol_handle1)->register_action_callback(dm->protocol_handle1, __action_cb_fn, _usb_write_cb_fn, dm);
		return ((struct protocol_1_logic_manager *)dm->protocol_handle1)->init(dm->protocol_handle1);
	}
}

static void __uninit_protocol_handle(struct lib_device_manager *dm)
{
    if (!dm)
        return;
	if (dm->protocol_type == 2) {
		if (dm->protocol_handle2) {
			((struct protocol_logic_manager *)dm->protocol_handle2)->uninit(dm->protocol_handle2);
		}
	}
	else {
		if (dm->protocol_handle1)
		((struct protocol_1_logic_manager *)dm->protocol_handle1)->uninit(dm->protocol_handle1);
	}
      
}

static void _init(struct lib_device_manager *dm,
                  libmouse_event_cb_fn event_cb,
                  libmouse_action_cb_fn action_cb,
                  libmouse_audio_cb_fn audio_cb,
                  libmouse_info_cb_fn info_cb)
{

    dm->libmouse_event_cb = event_cb;
    dm->libmouse_action_cb = action_cb;
    dm->libmouse_audio_cb = audio_cb;
    dm->libmouse_info_cb = info_cb;

	//dm->fifo = mblue_fifo_alloc(sizeof(struct voice_cache) * 40);
	//InitializeCriticalSection(&dm->lock);


	if(!dm->protocol_handle1)
		dm->protocol_handle1 = mblue_malloc(sizeof(struct protocol_1_logic_manager));

	if(!dm->protocol_handle2)
		dm->protocol_handle2 = mblue_malloc(sizeof(struct protocol_logic_manager));

	//if(!dm->voice_handle1)
	//	 dm->voice_handle1 = mblue_malloc(sizeof(struct libspeech_voice));
 //    libspeech_voice_construct(dm->voice_handle1);
 //    __get_voice_handle(dm->voice_handle1)->init(dm->voice_handle1);

	// if (!dm->voice_handle2)
	//	dm->voice_handle2 = mblue_malloc(sizeof(struct libspeech_voice));
 //    libspeech_voice_construct(dm->voice_handle2);
 //    __get_voice_handle(dm->voice_handle2)->init(dm->voice_handle2);

    if (!dm->usb_handle)
        dm->usb_handle = mblue_malloc(sizeof(struct lib_speech_usb));

    // int ret = __init_protocol_handle(dm);
}

static void _uninit(struct lib_device_manager *dm)
{
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

    /* if (dm->voice_handle1)
     {
         __get_voice_handle(dm->voice_handle1)->uninit(dm->voice_handle1);
         mblue_free(dm->voice_handle1);
         dm->voice_handle1 = 0;
     }

     if (dm->voice_handle2)
     {
         __get_voice_handle(dm->voice_handle2)->uninit(dm->voice_handle2);
         mblue_free(dm->voice_handle2);
         dm->voice_handle2 = 0;
     }

	 if (NULL != dm->fifo)
	 {
		 dm->fifo->reset(dm->fifo);
		 mblue_fifo_free(dm->fifo);
		 dm->fifo = NULL;
	 }
	 DeleteCriticalSection(&dm->lock);
	 */
}

//static void *__usb_thread_func(void *param)
//{
//    struct lib_device_manager *lm = param;
//    if (NULL != lm && lm->usb_handle)
//    {
//       lm->usb_handle->startRead(lm->usb_handle);
//    }
//	return NULL;
//}

static DWORD WINAPI __usb_thread_func(LPVOID param) {

	    struct lib_device_manager *lm = param;
	    if (NULL != lm && lm->usb_handle)
	    {
	       lm->usb_handle->start_read(lm->usb_handle);
	    }
	return 0;
}

static void __check_protocol_version(struct lib_device_manager *dm)
{
  /*  int ret = __init_protocol_handle(dm);*/
    if (dm->protocol_type == 2)
    {
        dm->is_timeout = true;
        //  dm->handshake(dm);
        dm->get_protocol_version(dm);
        Sleep(3000);
        if (dm->is_timeout)
        {
            printf("read protocolVersion timeout \n");
            __uninit_protocol_handle(dm);
            dm->is_new_protocol = false;
            dm->protocol_type = 1;
			Sleep(3000);
			  __init_protocol_handle(dm);
          //  __check_protocol_version(dm);
        }
    }
    else
    {
        //    dm->handshake(dm);
    }
}

//static DWORD WINAPI __voice_thread_func(LPVOID param) {
//
//	struct lib_device_manager *dm = param;
//	if (NULL == dm)
//		return 0;
//	
//	while (true) {
//		EnterCriticalSection(&dm->lock);
//		if (dm->fifo->size <= 0)
//		{
//			LeaveCriticalSection(&dm->lock);
//			continue;
//		}
//
//		u32 size = dm->fifo->len(dm->fifo);
//		if (size >= sizeof(struct voice_cache)) {
//			struct voice_cache cache;
//			dm->fifo->get(dm->fifo, &cache, sizeof(struct voice_cache));
//			      int ret = 0;
//			unsigned short pcm_buffer[320] = {0};
//			unsigned short pcm_len = 0;
//			if (cache.channel== 0)
//			{
//				ret = __get_voice_handle(dm->voice_handle1)->decode(dm->voice_handle1, (unsigned short *)cache.data, pcm_buffer, &pcm_len);
//			 }
//			else if (cache.channel == 1)
//			{
//				ret = __get_voice_handle(dm->voice_handle2)->decode(dm->voice_handle2, (unsigned short *)cache.data, pcm_buffer, &pcm_len);
//			}
//			if (ret)
//				dm->libmouse_audio_cb(dm->device, cache.channel, pcm_buffer, pcm_len);
//		}
//		LeaveCriticalSection(&dm->lock);
//		Sleep(1);
//	}
//
//	return 0;
//}


static int _open_device(struct lib_device_manager *dm, struct libmouse_device_info *usb_device)
{
	int ret = __init_protocol_handle(dm);

	/*LPDWORD voiceThreadId = NULL;
	dm->voice_pthread_handle = CreateThread(NULL, 0, __voice_thread_func, dm, 0, voiceThreadId);*/

    dm->device = usb_device;
    libspeechusb_construct_device(dm->usb_handle);
    // pthread_create(&dm->pthread_handle, NULL, (void *)__usb_thread_func, dm);
    ret = dm->usb_handle->init(dm->usb_handle, (struct libspeechusb_device_info *)usb_device, __lib_usb_cb_fn, dm);
    if (ret)
    {
        Sleep(2000);
		LPDWORD dwThreadId = NULL;
		dm->pthread_handle = CreateThread(NULL, 0, __usb_thread_func, dm, 0, dwThreadId);
        //pthread_create(&dm->pthread_handle, NULL, (void *)__usb_thread_func, dm);
    }

	Sleep(2000);
    __check_protocol_version(dm);

    return ret;
}

static int _close_device(struct lib_device_manager *dm)
{

    if (dm->pthread_handle)
    {
		CloseHandle(dm->pthread_handle);
        dm->pthread_handle = 0;
    }

	//if (dm->voice_pthread_handle) {
	//	CloseHandle(dm->voice_pthread_handle);
	//	dm->voice_pthread_handle = 0;
	//}

    if (dm->usb_handle)
    {
        dm->usb_handle->uninit(dm->usb_handle);
        mblue_free(dm->usb_handle);
    }

    return 1;
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

#define AUTHENTICATION_SLEEP_TIME			(300)
#define SLEEP_TIME							(200)
#define RETRY_MAX_COUNT						(5)

static void _start_authentication(struct lib_device_manager *dm)
{
//	dm->handshake(dm);


	/*if (dm->protocol_type == 2) {
		if (dm->protocol_handle2)
			((struct protocol_logic_manager *)dm->protocol_handle2)->set_authenticate(dm->protocol_handle2, NULL, 0);
	}
	else {
		if (dm->protocol_handle1)
			((struct protocol_1_logic_manager *)dm->protocol_handle1)->set_authenticate(dm->protocol_handle1, NULL, 0);
	}*/

	if (dm->protocol_type == 1) {
		dm->handshake(dm);
		return;
	}

	struct protocol_logic_manager *lm = dm->protocol_handle2;
	if (NULL == lm)
		return;

	dm->authentication_step = 0;

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	_set_client_finger(dm);
	Sleep(AUTHENTICATION_SLEEP_TIME);
	int retry_count = 0;
	int step_retry_count = 0;
	step_retry_count++;

	while (!(dm->authentication_step & setClientFingerStep)) {
		if (step_retry_count > RETRY_MAX_COUNT)
			goto EXIT_LABEL;

		retry_count++;
		Sleep(SLEEP_TIME);
		if (retry_count > RETRY_MAX_COUNT) {
			_set_client_finger(dm);
			step_retry_count++;
			retry_count = 0;
		}
	}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	
	lm->get_device_finger(lm);
	Sleep(AUTHENTICATION_SLEEP_TIME);
	retry_count = 0;
	step_retry_count = 1;
	while (!(dm->authentication_step & getDeviceFingerStep)) {
		if (step_retry_count > RETRY_MAX_COUNT)
			goto EXIT_LABEL;

		retry_count++;
		Sleep(SLEEP_TIME);
		if (retry_count >= RETRY_MAX_COUNT) {
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

		retry_count++;
		Sleep(SLEEP_TIME);
		if (retry_count > RETRY_MAX_COUNT) {
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

		retry_count++;
		Sleep(SLEEP_TIME);
		if (retry_count >= RETRY_MAX_COUNT) {
			dm->get_device_version(dm);
			step_retry_count++;
			retry_count = 0;
		}
	}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	step_retry_count = 1;
	dm->handshake(dm);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

EXIT_LABEL:
	if (step_retry_count > RETRY_MAX_COUNT) {
		if (NULL != dm->libmouse_action_cb)
			dm->libmouse_action_cb(dm->device, LIBMOUSE_ACTION_AUTHENTICATION, 0);
	}
	return;
}

static void __install_lib_device_manager(struct lib_device_manager *dm)
{
    dm->init = _init;
    dm->uninit = _uninit;
    dm->open_device = _open_device;
    dm->close_device = _close_device;
    dm->set_dpi = _set_dpi;
    dm->get_dpi = _get_dpi;
    dm->start_record = _start_record;
    dm->stop_record = _stop_record;
    dm->get_battery = _get_battery;
    dm->get_device_version = _get_device_version;
    dm->get_device_macaddr = _get_device_macaddr;
    dm->get_dongle_version = _get_dongle_version;
    dm->get_dongle_macaddr = _get_dongle_macaddr;
    dm->handshake = _handshake;
    dm->get_device_productNo = _get_device_productNo;
    dm->get_protocol_version = _get_protocol_version;
    dm->set_record_channel_size = _set_record_channel_size;
    dm->start_authentication = _start_authentication;
    dm->protocol_type = 2;
	dm->protocol_handle1 = 0;
	dm->protocol_handle2 = 0;
	/*dm->voice_handle1 = 0;
	dm->voice_handle2 = 0;*/
	mblue_aes_construct(&dm->aes);
	mblue_hash_construct(&dm->hash);
	mblue_md5_construct(&dm->md5);
}

void lib_get_device_manager(struct lib_device_manager *dm)
{
    if (dm)
    {
        __install_lib_device_manager(dm);
    }
}