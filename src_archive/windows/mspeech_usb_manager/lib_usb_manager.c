#include <Windows.h>
//#pragma  comment(lib, "Winmm.lib")
//#include <io.h>
#include "mblue_aes.h"
#include "mblue_hash.h"
#include "mblue_md5.h"
#include "mblue_crc.h"
#include "lib_usb_manager.h"
#include "libspeech_protocol.h"
#include "mspeech_usb_manager.h"

static void _write_func(struct lib_usb_manager *manager, unsigned char *data, unsigned int size)
{
	if (manager->write_cb)
		manager->write_cb(manager->base, data, size);
	return;
}

static struct libspeech_protocol *_get_handle(struct lib_usb_manager *manager)
{
	return (struct libspeech_protocol *)manager->handler;
}

static void _update_ota(struct lib_usb_manager *manager, unsigned char *data, unsigned int len);

static unsigned char __read_part_firmware(struct lib_usb_manager *manager, unsigned char *out_data, unsigned long size) {
	if (manager->ota_param.firmware == NULL || size != 32 || out_data == NULL || manager->ota_param.firmware_len < manager->ota_param.firmware_index) {
		if (manager->ota_param.firmware != NULL) {
			free(manager->ota_param.firmware);
			manager->ota_param.firmware = NULL;
		}
		manager->ota_param.firmware_len = 0;
		return 0;
	}
	if (manager->ota_param.firmware_len == manager->ota_param.firmware_index) {
		if (manager->ota_param.firmware != NULL) {
			free(manager->ota_param.firmware);
			manager->ota_param.firmware = NULL;
		}
		manager->ota_param.firmware_len = 0;
		return 2;
	}
	if (manager->ota_param.firmware_len < (manager->ota_param.firmware_index + size)) {
		memcpy(out_data, manager->ota_param.firmware + manager->ota_param.firmware_index, manager->ota_param.firmware_len - manager->ota_param.firmware_index);
		memset(out_data + (manager->ota_param.firmware_len - manager->ota_param.firmware_index), 0xff, size + manager->ota_param.firmware_index - manager->ota_param.firmware_len);
		manager->ota_param.firmware_index = manager->ota_param.firmware_len;
	}
	else if (manager->ota_param.firmware_len >= (manager->ota_param.firmware_index + size)) {
		memcpy(out_data, manager->ota_param.firmware + manager->ota_param.firmware_index, size);
		manager->ota_param.firmware_index += size;
	}
	return 1;
}

static void protocol_callback(void *param, unsigned char cmd, unsigned char sequence, unsigned char *data, unsigned int size)
{
	if (param == NULL)
		return;
	struct lib_usb_manager *manager = (struct lib_usb_manager *)param;
	if (manager->event_cb == NULL)
		return;
	if (cmd == DEVICE_EVENT)
	{
		unsigned char type = *data;
		switch (type)
		{
		case EVENT_TYPE_CONNECTED_DEVICE:
			manager->event_cb(manager->base, MSPEECH_USB_MANAGERE_EVENT_CONNECTED, data + 1, size - 1);
			break;
		case EVENT_TYPE_DISCONNECT_DEVICE:
			manager->event_cb(manager->base, MSPEECH_USB_MANAGERE_EVENT_DISCONNECT, data + 1, size - 1);
			break;
		case EVENT_TYPE_SCAN_DEVICE:
			manager->event_cb(manager->base, MSPEECH_USB_MANAGERE_EVENT_SCAN, data + 1, size - 1);
			break;
		default:
			break;
		}
	}
	else if (cmd == WRITE_DATA_RESP)
	{
		unsigned char type = *data;
		switch (type)
		{
		case WRITE_TYPE_OTA:
		{
			unsigned char state = *(data + 1);
			unsigned char result = *(data + 2);
			if (state == START_OTA_CB)
			{
				if (size == 19 && result == 1)
				{
					unsigned char salt[16] = { 0 };
					md5_ctx md5ctx;
					struct mblue_md5 md5;
					mblue_md5_construct(&md5);
					md5.init(&md5, &md5ctx);
					md5.update(&md5, &md5ctx, manager->ota_param.session, 16);
					md5.final(&md5, &md5ctx, salt);
					if (memcmp(salt, data + 3, size - 3) == 0)
					{
						unsigned char firmware_data[32] = { 0 };
						unsigned char read_result = __read_part_firmware(manager, firmware_data, PART_FIRMWARE_SIZE);
						if (read_result == 1)
						{
							_update_ota(manager, firmware_data, PART_FIRMWARE_SIZE);
							manager->event_cb(manager->base, MSPEECH_USB_MANAGERE_EVENT_START_OTA, &result, 1);
							break;
						}
					}
				}
				manager->end_ota(manager, 0);
				result = 0;
				manager->event_cb(manager->base, MSPEECH_USB_MANAGERE_EVENT_START_OTA, &result, 1);
			}
			else if (state == UPDATA_OTA_CB)
			{
				if (size == 7 && result == 1 && (manager->ota_param.ota_step & 1))
				{
					unsigned int firmware_hash;
					memcpy(&firmware_hash, data + 3, 4);
					if (manager->ota_param.firmware_hash_sum == firmware_hash)
					{
						unsigned char firmware_data[32] = { 0 };
						unsigned char read_result = __read_part_firmware(manager, firmware_data, PART_FIRMWARE_SIZE);
						if (read_result == 1)
						{
							_update_ota(manager, firmware_data, PART_FIRMWARE_SIZE);
							manager->event_cb(manager->base, MSPEECH_USB_MANAGERE_EVENT_UPDATE_OTA, (unsigned char *)&(manager->ota_param.firmware_index), 4);
							break;
						}
						else if (read_result == 2)
						{
							manager->end_ota(manager, 1);
							manager->event_cb(manager->base, MSPEECH_USB_MANAGERE_EVENT_UPDATE_OTA, (unsigned char *)&(manager->ota_param.firmware_index), 4);
							break;
						}
					}
				}
				result = 0;
				if (manager->ota_param.ota_step & 1)
					manager->end_ota(manager, 0);
				manager->event_cb(manager->base, MSPEECH_USB_MANAGERE_EVENT_UPDATE_OTA, &result, 1);
			}
			else if (state == END_OTA_CB)
			{
				manager->ota_param.ota_step &= 0xfe;
				if (manager->ota_param.firmware)
				{
					free(manager->ota_param.firmware);
					manager->ota_param.firmware = NULL;
				}
				manager->event_cb(manager->base, MSPEECH_USB_MANAGERE_EVENT_END_OTA, &result, 1);
			}
			break;
		}

		case WRITE_TYPE_TRANSPOND:
			manager->event_cb(manager->base, MSPEECH_USB_MANAGERE_EVENT_TRANSPOND, data + 1, size - 1);
			break;
		case WRITE_TYPE_MASTER_ATTRIBUTE:
		{
			unsigned char att_type = *(data + 1);
			if (att_type == VERSION_MASTER && size == 6)
			{
				memcpy(manager->ota_param.dongle_version, data + 2, 4);
				manager->ota_param.ota_step |= (1 << VERSION_MASTER);
			}
			else if (att_type == MAC_MASTER && size == 8)
			{
				memcpy(manager->ota_param.dongle_mac, data + 2, 6);
				manager->ota_param.ota_step |= (1 << MAC_MASTER);
			}
			else if (att_type == PRODUCT_NO && size == 6)
			{
				manager->event_cb(manager->base, MSPEECH_USB_MANAGERE_EVENT_PRODUCT_NO, data + 2, 4);
				memcpy(manager->ota_param.product_NO, data + 2, 4);
				manager->ota_param.ota_step |= (1 << PRODUCT_NO);
				manager->product_ack_flag = 0;
			}
			break;
		}
		default:
			break;
		}
	}
	else {
		if (manager->device_type == SINGLE_DONGLE)
		{
			if (manager->product_ack_flag)
			{
				if (cmd == 0x81)
				{
					unsigned int product_no = 0;
					manager->event_cb(manager->base, MSPEECH_USB_MANAGERE_EVENT_PRODUCT_NO, data, size);
					manager->product_ack_flag = 0;
				}
			}
			struct libspeech_protocol_command_data command = { 0 };
			(_get_handle(manager))->get_command_by_sequence(_get_handle(manager), cmd, sequence, data, size, &command);
			if (command.len) {
				manager->event_cb(manager->base, MSPEECH_USB_MANAGERE_EVENT_APPEND, command.data, command.len);
			}
		}
	}
}

static void _init(struct lib_usb_manager *manager, unsigned char device_type, lib_event_cb cb, lib_write_cb w_cb, void *param)
{
	if (_get_handle(manager) == NULL)
		manager->handler = malloc(sizeof(struct libspeech_protocol));
	libspeech_protocol_construct((_get_handle(manager)));
	(_get_handle(manager))->init(_get_handle(manager), protocol_callback, manager);
	manager->event_cb = cb;
	manager->write_cb = w_cb;
	manager->base = param;
	manager->device_type = device_type;
	memset(&(manager->ota_param), 0, sizeof(ota_item));
	manager->ota_param.product_NO[0] = '5';
	manager->ota_param.product_NO[1] = '0';
	manager->ota_param.product_NO[2] = '0';
	manager->ota_param.product_NO[3] = '0';
}

static void _uninit(struct lib_usb_manager *manager) {
	// _destory_thread_(manager);
	(_get_handle(manager))->uninit(_get_handle(manager));
	if (NULL != _get_handle(manager))
	{
		free(_get_handle(manager));
		manager->handler = NULL;
	}
}

static void _start_service(struct lib_usb_manager *manager)
{
	if (_get_handle(manager) == NULL)
		return;
	(_get_handle(manager))->start_service(_get_handle(manager));
}

static void _stop_service(struct lib_usb_manager *manager) {
	if (_get_handle(manager) == NULL)
		return;
	(_get_handle(manager))->stop_service(_get_handle(manager));
}

static int _convert_to_usb_data(struct lib_usb_manager *manager, unsigned char *mac, unsigned char *data, unsigned int size)
{
	if (_get_handle(manager) == NULL)
		return 0;
	if (manager->device_type == MULTI_DONGLE)
	{
		unsigned char buffer[128] = { 0 };
		buffer[0] = 0x2;
		memcpy(buffer + 1, mac, 6);
		memcpy(buffer + 7, data, size);
		struct libspeech_protocol_command_data command = { 0 };
		(_get_handle(manager))->get_command(_get_handle(manager), WRITE_DATA, buffer, 7 + size, &command);
		if (command.len) {
			_write_func(manager, command.data, command.len);
			return command.len;
		}
	}
	else if (manager->device_type == SINGLE_DONGLE)
	{
		_write_func(manager, data, size);
	}
	return 0;
}

static void _scan_device(struct lib_usb_manager *manager, unsigned int scan_timer, unsigned char scan_rssi) {
	if (_get_handle(manager) == NULL)
		return;
	if (manager->device_type == MULTI_DONGLE)
	{
		unsigned char buffer[32] = { 0 };
		memcpy(buffer, &scan_timer, 4);
		buffer[4] = scan_rssi;
		struct libspeech_protocol_command_data command = { 0 };
		(_get_handle(manager))->get_command(_get_handle(manager), SCAN_DEVICE, buffer, 5, &command);
		if (command.len) {
			_write_func(manager, command.data, command.len);
		}
	}
}

static void _get_connected_device(struct lib_usb_manager *manager) {
	if (_get_handle(manager) == NULL)
		return;
	if (manager->device_type == MULTI_DONGLE)
	{
		unsigned char buffer[64] = { 0 };
		buffer[0] = GET_CONNECTED_DEVICE;

		struct libspeech_protocol_command_data command = { 0 };
		(_get_handle(manager))->get_command(_get_handle(manager), DEVICE_MANAGER, buffer, 1, &command);
		if (command.len) {
			_write_func(manager, command.data, command.len);
		}
	}
}

static void _connect_device(struct lib_usb_manager *manager, unsigned char *mac) {
	if (_get_handle(manager) == NULL)
		return;
	if (manager->device_type == MULTI_DONGLE)
	{
		unsigned char buffer[64] = { 0 };
		buffer[0] = CONNECT_DEVICE;
		memcpy(buffer + 1, mac, 6);

		struct libspeech_protocol_command_data command = { 0 };
		(_get_handle(manager))->get_command(_get_handle(manager), DEVICE_MANAGER, buffer, 7, &command);
		if (command.len) {
			_write_func(manager, command.data, command.len);
		}
	}
}

static void _remove_device(struct lib_usb_manager *manager, unsigned char *mac) {
	if (_get_handle(manager) == NULL)
		return;
	if (manager->device_type == MULTI_DONGLE)
	{
		unsigned char buffer[64] = { 0 };
		buffer[0] = REMOVE_DEVICE;
		memcpy(buffer + 1, mac, 6);

		struct libspeech_protocol_command_data command = { 0 };
		(_get_handle(manager))->get_command(_get_handle(manager), DEVICE_MANAGER, buffer, 7, &command);
		if (command.len) {
			_write_func(manager, command.data, command.len);
		}
	}
}

static void _reset_device(struct lib_usb_manager *manager)
{
	if (_get_handle(manager) == NULL)
		return;
	if (manager->device_type == MULTI_DONGLE)
	{
		unsigned char buffer[64] = { 0 };
		buffer[0] = RESET_DEVICE;

		struct libspeech_protocol_command_data command = { 0 };
		(_get_handle(manager))->get_command(_get_handle(manager), DEVICE_MANAGER, buffer, 1, &command);
		if (command.len) {
			_write_func(manager, command.data, command.len);
		}
	}
}

static void _get_master_version(struct lib_usb_manager *manager)
{
	if (_get_handle(manager) == NULL)
		return;
	if (_get_handle(manager) == NULL)
		return;
	unsigned char buffer[64] = { 0 };
	buffer[0] = WRITE_TYPE_MASTER_ATTRIBUTE;
	buffer[1] = VERSION_MASTER;
	struct libspeech_protocol_command_data command = { 0 };
	_get_handle(manager)->get_command(_get_handle(manager), WRITE_DATA, buffer, 2, &command);
	if (command.len) {
		_write_func(manager, command.data, command.len);
	}
}

static void _get_master_mac(struct lib_usb_manager *manager)
{
	if (_get_handle(manager) == NULL)
		return;
	unsigned char buffer[64] = { 0 };
	buffer[0] = WRITE_TYPE_MASTER_ATTRIBUTE;
	buffer[1] = MAC_MASTER;
	struct libspeech_protocol_command_data command = { 0 };
	(_get_handle(manager))->get_command(_get_handle(manager), WRITE_DATA, buffer, 2, &command);
	if (command.len) {
		_write_func(manager, command.data, command.len);
	}
}

static void _get_master_product_no(struct lib_usb_manager *manager)
{
	if (_get_handle(manager) == NULL)
		return;
	unsigned char buffer[64] = { 0 };
	buffer[0] = WRITE_TYPE_MASTER_ATTRIBUTE;
	buffer[1] = PRODUCT_NO;
	manager->product_ack_flag = 1;
	struct libspeech_protocol_command_data command = { 0 };
	(_get_handle(manager))->get_command(_get_handle(manager), WRITE_DATA, buffer, 2, &command);
	if (command.len) {
		_write_func(manager, command.data, command.len);
	}
}

static void _get_common_sessionkey(unsigned char* key, unsigned char *mac, unsigned char *version) {
	memcpy(key, mac, 6);
	*(key + 6) = '{';
	memcpy(key + 7, version, 4);
	*(key + 11) = '}';
	struct mblue_hash hash;
	mblue_hash_construct(&hash);
	unsigned int hash_key = hash.sdbm_mem(&hash, key, 12);
	memcpy(key + 12, &hash_key, 4);
}

static void get_firmware_key(unsigned char* key, unsigned char *product_no, unsigned int len)
{
	*key = '{';
	memcpy(key + 1, (unsigned char *)&len, 4);
	*(key + 5) = '{';
	memcpy(key + 6, product_no, 4);
	*(key + 10) = '}';
	struct mblue_hash hash;
	mblue_hash_construct(&hash);
	unsigned int hash_key = hash.sdbm_mem(&hash, key + 1, 10);
	memcpy(key + 11, &hash_key, 4);
	*(key + 15) = '}';
}


static unsigned char _check_the_firmware(struct lib_usb_manager *manager, unsigned char *firmware, unsigned int size)
{
	if (firmware == NULL || size < 32 || ((size - 8) % 16) != 0)
		return 0;
	unsigned int file_size = 0;
	unsigned int file_size1 = 0;
	unsigned char product_NO[4] = { 0 };
	unsigned char session_key[16] = { 0 };
	unsigned char data_word[32] = { 0 };
	memcpy(product_NO, firmware, 4);
	memcpy((unsigned char *)&file_size1, firmware + 4, 4);
	memcpy(data_word, firmware + 8, 32);
	get_firmware_key(session_key, product_NO, file_size1);
	struct aes_ctx aesctx;
	struct mblue_aes aes;
	mblue_aes_construct(&aes);
	aes.init_ctx(&aes, &aesctx, session_key);
	aes.ecb_decrypt(&aes, &aesctx, data_word);
	aes.ecb_decrypt(&aes, &aesctx, data_word + 16);

	memcpy((unsigned char *)&file_size, data_word + 0x18, 4);
	file_size += (file_size % 16) ? (16 - (file_size % 16)) : 0;
	manager->ota_param.firmware_len = size - 8;
	if (file_size != manager->ota_param.firmware_len || *(data_word + 8) != 0x4b || file_size1 != manager->ota_param.firmware_len || memcmp(product_NO, manager->ota_param.product_NO, 4))
		return 0;
	manager->ota_param.firmware = (unsigned char *)malloc(sizeof(unsigned char) * manager->ota_param.firmware_len);
	memcpy(manager->ota_param.firmware, data_word, 32);
	for (unsigned int i = 32; i < manager->ota_param.firmware_len; i += 16)
	{
		memcpy(manager->ota_param.firmware + i, firmware + 8 + i, 16);
		aes.ecb_decrypt(&aes, &aesctx, manager->ota_param.firmware + i);
	}
	return 1;
}

static void _start_ota_func(struct lib_usb_manager *manager, unsigned char *firmware, unsigned int size)
{
	if (_get_handle(manager) == NULL)
		return;
	unsigned char check_step = 0;
	while ((manager->ota_param.ota_step & (1 << PRODUCT_NO)) == 0)
	{
		if ((check_step % 10) == 0)
			_get_master_product_no(manager);
		check_step++;
		if (check_step > 50)
		{
			break;
		}
		Sleep(100);
	}
	check_step = 0;
	while ((manager->ota_param.ota_step & (1 << VERSION_MASTER)) == 0)
	{
		if ((check_step % 10) == 0)
			_get_master_version(manager);
		check_step++;
		if (check_step > 50)
		{
			break;
		}
		Sleep(100);
	}
	check_step = 0;
	while ((manager->ota_param.ota_step & (1 << MAC_MASTER)) == 0)
	{
		if ((check_step % 10) == 0)
			_get_master_mac(manager);
		check_step++;
		if (check_step > 50)
		{
			break;
		}
		Sleep(100);
	}
	struct libspeech_protocol_command_data command = { 0 };
	if ((manager->ota_param.ota_step & (1 << VERSION_MASTER)) && (manager->ota_param.ota_step & (1 << MAC_MASTER)))
	{
		unsigned char buffer[64] = { 0 };
		buffer[0] = WRITE_TYPE_OTA;
		buffer[1] = START_OTA_CB;
		manager->ota_param.firmware_crc32_sum = 0;
		manager->ota_param.firmware_hash_sum = 0;
		manager->ota_param.firmware_index = 0;
		_get_common_sessionkey(manager->ota_param.session_key, manager->ota_param.dongle_mac, manager->ota_param.dongle_version);
		struct aes_ctx aesctx;
		struct mblue_aes aes;
		mblue_aes_construct(&aes);
		aes.init_ctx(&aes, &aesctx, manager->ota_param.session_key);
		memcpy(buffer + 2, manager->ota_param.session, 16);
		aes.ecb_decrypt(&aes, &aesctx, manager->ota_param.session);
		if (_check_the_firmware(manager, firmware, size))
			(_get_handle(manager))->get_command(_get_handle(manager), WRITE_DATA, buffer, 18, &command);
	}
	if (command.len) {
		_write_func(manager, command.data, command.len);
	}
	else
	{
		unsigned char result = 0;
		manager->ota_param.ota_step &= 0xfe;
		if (manager->ota_param.firmware)
		{
			free(manager->ota_param.firmware);
			manager->ota_param.firmware = NULL;
		}
		if (manager->event_cb)
			manager->event_cb(manager->base, MSPEECH_USB_MANAGERE_EVENT_START_OTA, &result, 1);
	}
}

static unsigned char _start_ota(struct lib_usb_manager *manager, unsigned char *session, unsigned char *firmware, unsigned int size)
{
	if (_get_handle(manager) == NULL || session == NULL)
		return 0;
	if (manager->ota_param.ota_step & 1)
		return 0;
	manager->ota_param.ota_step |= 1;
	memcpy(manager->ota_param.session, session, 16);
	_start_ota_func(manager, firmware, size);
	return 1;
}

static void _update_ota(struct lib_usb_manager *manager, unsigned char *data, unsigned int len)
{
	if (manager == NULL || len != PART_FIRMWARE_SIZE)
		return;
	unsigned char temp_buf[34] = { 0 };
	temp_buf[0] = WRITE_TYPE_OTA;
	temp_buf[1] = UPDATA_OTA_CB;
	for (int i = 0; i < 16; i++) {
		temp_buf[i + 2] = *(data + i) ^ manager->ota_param.session_key[i];
	}
	struct aes_ctx aesctx;
	struct mblue_aes aes;
	mblue_aes_construct(&aes);
	aes.init_ctx(&aes, &aesctx, manager->ota_param.session);
	aes.ecb_encrypt(&aes, &aesctx, temp_buf + 2);

	for (int i = 0; i < 16; i++) {
		temp_buf[i + 18] = *(data + i + 16) ^ manager->ota_param.session_key[i];
	}
	struct mblue_hash hash;
	mblue_hash_construct(&hash);
	manager->ota_param.firmware_hash_sum += hash.sdbm_mem(&hash, data, 32);
	manager->ota_param.firmware_crc32_sum += calculate_crc32(data, 32);
	aes.ecb_encrypt(&aes, &aesctx, temp_buf + 16 + 2);
	struct libspeech_protocol_command_data command = { 0 };
	(_get_handle(manager))->get_command(_get_handle(manager), WRITE_DATA, temp_buf, 34, &command);
	if (command.len) {
		_write_func(manager, command.data, command.len);
	}
}

static void _end_ota(struct lib_usb_manager *manager, unsigned char value)
{
	if (manager == NULL)
		return;
	unsigned char buffer[20] = { 0 };
	buffer[0] = WRITE_TYPE_OTA;
	buffer[1] = END_OTA_CB;
	buffer[2] = value;
	manager->ota_param.ota_step &= 0xfe;
	struct libspeech_protocol_command_data command = { 0 };
	if (value == 1)
	{
		memcpy(buffer + 3, &(manager->ota_param.firmware_crc32_sum), 4);
		(_get_handle(manager))->get_command(_get_handle(manager), WRITE_DATA, buffer, 7, &command);
	}
	else
	{
		(_get_handle(manager))->get_command(_get_handle(manager), WRITE_DATA, buffer, 3, &command);
	}
	if (command.len) {
		_write_func(manager, command.data, command.len);
	}
}

static void _append(struct lib_usb_manager *manager, unsigned char *data, unsigned int size) {
	(_get_handle(manager))->add_data(_get_handle(manager), data, size);
}

void lib_usb_manager_construct(struct lib_usb_manager *usb)
{
	if (usb) {
		usb->product_ack_flag = 0;
		usb->handler = NULL;
		usb->device_type = 0;
		usb->init = _init;
		usb->convert_to_usb_data = _convert_to_usb_data;
		usb->append = _append;
		usb->uninit = _uninit;
		usb->scan_device = _scan_device;
		usb->connect_device = _connect_device;
		usb->remove_device = _remove_device;
		usb->get_connected_device = _get_connected_device;
		usb->start_service = _start_service;
		usb->stop_service = _stop_service;
		usb->reset_device = _reset_device;
		usb->start_ota = _start_ota;
		usb->end_ota = _end_ota;
		usb->get_dongle_product_no = _get_master_product_no;
	}
}