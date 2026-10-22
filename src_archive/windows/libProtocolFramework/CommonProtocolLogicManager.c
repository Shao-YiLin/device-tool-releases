
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <Windows.h>
#include "CommonProtocolLogicManager.h"
#include "protocol_define.h"

static void __event_action(struct CommonProtocolManager *lm, unsigned char *data, unsigned int len)
{
	if (NULL == data || len < 5)
		return;

	if (!lm->pCBfn)
		return;

	unsigned char type = *data;
	unsigned int value = byteToInt(data + 1, 4);

	if (type == EVENT_TYPE_KEY)
	{
		unsigned int device_index = (value & 0xffff0000) >> 16;
		unsigned int device_status = (value & 0x0000ffff);
		if (device_status == EVENT_KEY_UP)
		{
			lm->pCBfn(lm->pParam, KEY_UP, device_index, 0, NULL, 0);
		}
		else if (device_status == EVENT_KEY_DOWN)
		{
			lm->pCBfn(lm->pParam, KEY_DOWN, device_index, 0, NULL, 0);
		}
	}
	else if (type == EVENT_TYPE_BATTERY)
	{
		lm->pCBfn(lm->pParam, BATTER, 0, 0, (char *)&value, sizeof(value));
	}
	else if (type == EVENT_TYPE_BLE)
	{
		lm->pCBfn(lm->pParam, BLE_STATUS, 0, 0, (char *)&value, sizeof(value));
	}
	else if (type == EVENT_TYPE_SINCHECK) {
		lm->pCBfn(lm->pParam, SIN_CHECK, 0, 0, (char *)&value, sizeof(value));
	}

	else if (type == EVENT_TYPE_RGB_BACKLIGHT) {


		lm->pCBfn(lm->pParam, GET_BACKHLIGHT, 0, 0, data, 4);

	}
	else if (type == EVENT_TYPE_LASER) {


		lm->pCBfn(lm->pParam, GET_LASER, 0, 0, (char *)&value, sizeof(value));

	}
}

static void _protocolCallBackFn(void *param, unsigned char cmd, unsigned char sequence, unsigned char *data, unsigned int size) {
	//printf("_protocolCallBackFn: %d \n", cmd);
	struct CommonProtocolManager *mg = (struct CommonProtocolManager *)param;
	if (cmd == 0x9) {  // event key
		__event_action(mg, data, size);
	}
	else if (cmd == 0xa0) {
		if (mg->pCBfn)
			mg->pCBfn(mg->pParam, RECORD_DATA, *data, *(data + 1), (char *)(data + 2), size - 2);
	}
	else { // other key
		if(mg->pParseCBfn)
			mg->pParseCBfn(mg->pParam, cmd, sequence, data, size);
	}
}

static DWORD WINAPI __protocol_thread_func(LPVOID param) {

	struct CommonProtocolManager *mg = (struct CommonProtocolManager *)param;
	if (NULL != mg)
	{
		mg->pProtocolHandle.start_service(&mg->pProtocolHandle);
	}
	return 0;
}

static void _startService(struct CommonProtocolManager *pm) {

	if (NULL == pm)
		return;

	LPDWORD dwThreadId = NULL;
	pm->pThreadHandle = CreateThread(NULL, 0, __protocol_thread_func, pm, 0, dwThreadId);
	if (NULL == pm->pThreadHandle)
	{
		pm->threadState = 0;
	}
	else
	{
		pm->threadState = 1;
	}
}

static void _stopService(struct CommonProtocolManager *pm) {

	if (NULL == pm)
		return;

	if (pm->threadState) {
		pm->pProtocolHandle.stop_service(&pm->pProtocolHandle);
		pm->threadState = 0;
	}

	if (pm->pThreadHandle)
	{
		TerminateThread(pm->pThreadHandle, 0);
		pm->pThreadHandle = 0;
	}
}

static void _init(struct CommonProtocolManager *pm, LOGIC_ACTION_CALLBACK callback, PROTOCOL_PARSE_CALLBACK parseCB, void *param) {
	pm->pCBfn = callback;
	pm->pParam = param;
	pm->pParseCBfn = parseCB;
	
	pm->pProtocolHandle.init(&pm->pProtocolHandle, _protocolCallBackFn, pm);
	pm->pProtocolHandle.set_sleep(&pm->pProtocolHandle, 1);
	_startService(pm);
}

static void _uninit(struct CommonProtocolManager *pm) {

	_stopService(pm);
	pm->pProtocolHandle.uninit(&pm->pProtocolHandle);
}

static void _append(struct CommonProtocolManager *pm, unsigned char *data, int size) {
	pm->pProtocolHandle.add_data(&pm->pProtocolHandle, data, size);
}

void CommonProtocolManagerConstruct(struct CommonProtocolManager *manager){
	if (manager) {
		manager->init = _init;
		manager->uninit = _uninit;
		manager->append = _append;
		libspeech_protocol_construct(&manager->pProtocolHandle);
	}
}