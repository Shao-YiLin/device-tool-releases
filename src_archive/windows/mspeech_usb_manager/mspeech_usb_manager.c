// mspeech_usb_manager.cpp : 定义 DLL 应用程序的导出函数。
//
#include <malloc.h>
#include "mspeech_usb_manager.h"
#include "lib_usb_manager.h"

static struct lib_usb_manager *_get_handle(struct mspeech_usb_manager *manager)
{
	return (struct lib_usb_manager *)(manager->handler);
}

static void _init(struct mspeech_usb_manager *manager, unsigned char device_type, event_callback cb, write_callback w_cb, void *param)
{
	if (manager->handler == NULL)
		manager->handler = malloc(sizeof(struct lib_usb_manager));
	lib_usb_manager_construct(_get_handle(manager));
	_get_handle(manager)->init(_get_handle(manager), device_type, cb, w_cb, param);
}

static void _start_service(struct mspeech_usb_manager *manager)
{
	if (manager->handler == NULL)
		return;
	_get_handle(manager)->start_service(_get_handle(manager));
}

static void _stop_service(struct mspeech_usb_manager *manager) {
	if (manager->handler == NULL)
		return;
	_get_handle(manager)->stop_service(_get_handle(manager));
}

static int _convert_to_usb_data(struct mspeech_usb_manager *manager, unsigned char *mac, unsigned char *data, unsigned int size)
{
	if (manager->handler == NULL)
		return 0;
	return _get_handle(manager)->convert_to_usb_data(_get_handle(manager), mac, data, size);
}

static void _append(struct mspeech_usb_manager *manager, unsigned char *data, unsigned int size) {
	if (manager->handler == NULL)
		return;
	_get_handle(manager)->append(_get_handle(manager), data, size);
}

static void _scan_device(struct mspeech_usb_manager *manager, unsigned int scan_timer, unsigned char scan_rssi) {
	if (manager->handler == NULL)
		return;
	_get_handle(manager)->scan_device(_get_handle(manager), scan_timer, scan_rssi);
}

static void _get_connected_device(struct mspeech_usb_manager *manager) {
	if (manager->handler == NULL)
		return;
	_get_handle(manager)->get_connected_device(_get_handle(manager));
}

static void _connect_device(struct mspeech_usb_manager *manager, unsigned char *mac) {
	if (manager->handler == NULL)
		return;
	_get_handle(manager)->connect_device(_get_handle(manager), mac);
}

static void _remove_device(struct mspeech_usb_manager *manager, unsigned char *mac) {
	if (manager->handler == NULL)
		return;
	_get_handle(manager)->remove_device(_get_handle(manager), mac);
}

static void _uninit(struct mspeech_usb_manager *manager) {
	if (manager->handler == NULL)
		return;
	(_get_handle(manager))->uninit(manager->handler);
	if (NULL != manager->handler)
	{
		free(manager->handler);
		manager->handler = NULL;
	}
}

static void _get_usb_event_data(struct mspeech_usb_manager *manager, unsigned char *data, unsigned int size, struct mspeech_usb_event_data *_event)
{
	if (NULL == _event)
		return;
	_event->version = *(data);
	_event->state = *(data + 1);
	memcpy(_event->mac, data + 2, 6);
	_event->len = size - 8;
	memcpy(_event->data, data + 8, _event->len);
}

static void _get_usb_write_data(struct mspeech_usb_manager *manager, unsigned char *data, unsigned int size, struct mspeech_usb_write_data *write) {
	if (NULL == write)
		return;

	memcpy(write->mac, data, 6);
	write->len = size - 6;
	memcpy(write->data, data + 6, write->len);
}

static void _get_usb_scan_data(struct mspeech_usb_manager *manager, unsigned char *data, unsigned int size, struct mspeech_usb_scan_data *scan)
{
	if (NULL == scan)
		return;
	scan->version = *(data);
	scan->rssi = *(data + 1);
	memcpy(scan->mac, data + 2, 6);
	scan->len = size - 8;
	memcpy(scan->data, data + 8, scan->len);
}

static void _reset_device(struct mspeech_usb_manager *manager)
{
	if (manager->handler == NULL)
		return;
	_get_handle(manager)->reset_device(_get_handle(manager));
}

static void _get_master_product_no(struct mspeech_usb_manager *manager)
{
	if (manager->handler == NULL)
		return;
	_get_handle(manager)->get_dongle_product_no(_get_handle(manager));
}

static unsigned char _start_ota(struct mspeech_usb_manager *manager, unsigned char *session, unsigned char *firmware, unsigned int size)
{
	if (manager->handler == NULL)
		return 0;
	return _get_handle(manager)->start_ota(_get_handle(manager), session, firmware, size);
}

static void _end_ota(struct mspeech_usb_manager *manager, unsigned char value)
{
	if (manager->handler == NULL)
		return;
	_get_handle(manager)->end_ota(_get_handle(manager), value);
}

void mspeech_usb_manager_construct(struct mspeech_usb_manager *usb)
{
	if (usb) {
		usb->handler = NULL;
		usb->init = _init;
		usb->convert_to_usb_data = _convert_to_usb_data;
		usb->append = _append;
		usb->uninit = _uninit;
		usb->scan_device = _scan_device;
		usb->connect_device = _connect_device;
		usb->remove_device = _remove_device;
		usb->get_connected_device = _get_connected_device;
		usb->get_usb_event_data = _get_usb_event_data;
		usb->get_usb_scan_data = _get_usb_scan_data;
		usb->get_usb_write_data = _get_usb_write_data;
		usb->start_service = _start_service;
		usb->stop_service = _stop_service;
		usb->reset_device = _reset_device;
		usb->start_ota = _start_ota;
		usb->end_ota = _end_ota;
		usb->get_dongle_product_no = _get_master_product_no;
	}
}