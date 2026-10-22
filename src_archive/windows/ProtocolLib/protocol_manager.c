
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "protocol_manager.h"

#include "protocol_define.h"
#include "mblue_hash.h"
#include "mblue_crc.h"
#include "mblue_heap.h"

#define PROTOCOL_HEADER_PRE_SIZE 0x7
#define PROTOCOL_FIFO_SIZE 2048

static u8 get_global_sequence(struct protocol_manager *pm)
{
	return pm->global_sequence++ % 256;
}

static void set_global_sequence_reset(struct protocol_manager *pm)
{
	pm->global_sequence = 0;
}

static u32 get_crc32(u8 *data, int len)
{
	return calculate_crc32(data, len);
}

static struct command_data *get_protocol_command(struct protocol_manager *pm, u8 cmd, u8 sequence, u8 *data, u32 length, struct command_data *command)
{
	if (pm == NULL || command == NULL)
		return NULL;
	u32 total_size = PROTOCOL_HEADER_PRE_SIZE;
	if (length > 0)
	{
		total_size = total_size + length + 4;
	}
	struct common_header pre_header;
	pre_header.magic = COMAMND_MAGIC;
	pre_header.sequence = sequence;
	pre_header.cmd = cmd;
	u8 payload_length[4] = { 0 };
	intToByte(length, payload_length);

	// if (NULL != pm->command.data)
	// {
	//     mblue_free(pm->command.data);
	//     pm->command.data = NULL;
	// }

	// pm->command.data = mblue_malloc(total_size);
	command->len = total_size;
	command->sequence = pre_header.sequence;
	memset(command->data, 0, sizeof(command->data));
	memcpy(command->data, &pre_header, 3);
	memcpy(command->data + 3, payload_length, sizeof(payload_length));
	if (length > 0)
	{
		memcpy(command->data + PROTOCOL_HEADER_PRE_SIZE, data, length);
		u32 sign = get_crc32(data, length);
		memcpy(command->data + PROTOCOL_HEADER_PRE_SIZE + length, &sign, sizeof(sign));
	}
	return command;
}

static void _init(struct protocol_manager *pm, ProtocolListener listener, void *param)
{
	pm->protocol_listener = listener;
	pm->cb_param = param;
	InitializeCriticalSection(&pm->lock);
	pm->fifo = mblue_fifo_alloc(PROTOCOL_FIFO_SIZE);
}

static void _uninit(struct protocol_manager *pm)
{
	log_d("protocol manager uninit \n");
	if (NULL != pm->fifo)
	{
		//  pm->fifo->reset(pm->fifo);
		mblue_fifo_free(pm->fifo);
		pm->fifo = NULL;
	}
	DeleteCriticalSection(&pm->lock);
}

static bool __is_full_pack(struct protocol_manager *pm, u8 *data, u32 len)
{

	if (len >= PROTOCOL_HEADER_PRE_SIZE)
	{
		struct protocol_header protocol_header = { 0 };
		protocol_header.head.magic = *data;
		protocol_header.head.sequence = *(data + 1);
		protocol_header.head.cmd = *(data + 2);
		protocol_header.head.length = byteToInt(data + 3, 4);
		if (protocol_header.head.length > 0)
		{
			if (len == protocol_header.head.length + 4 + PROTOCOL_HEADER_PRE_SIZE)
			{

				memcpy(protocol_header.payload, data + PROTOCOL_HEADER_PRE_SIZE, protocol_header.head.length);
				//	protocol_header.sign = byteToInt(data + PROTOCOL_HEADER_PRE_SIZE + protocol_header.head.length, 4);
				pm->protocol_listener(pm->cb_param, protocol_header.head.cmd, protocol_header.head.sequence, protocol_header.payload, protocol_header.head.length);
				return true;
			}
		}
		else if (len == PROTOCOL_HEADER_PRE_SIZE) {
			pm->protocol_listener(pm->cb_param, protocol_header.head.cmd, protocol_header.head.sequence, protocol_header.payload, protocol_header.head.length);
			return true;
		}
	}

	return false;
}

static void _add_data(struct protocol_manager *pm, u8 *data, u32 len)
{
	if (NULL == pm->fifo)
		return;
	EnterCriticalSection(&pm->lock);
	if (__is_full_pack(pm, data, len)) {
		LeaveCriticalSection(&pm->lock);
		return;
	}
	pm->fifo->put(pm->fifo, data, len);
	LeaveCriticalSection(&pm->lock);
}

static struct command_data *_get_command(struct protocol_manager *pm, u8 cmd, u8 *payload, u32 len, struct command_data *command)
{
	return get_protocol_command(pm, cmd, get_global_sequence(pm), payload, len, command);
}

static struct command_data *_get_command_by_sequence(struct protocol_manager *pm, u8 cmd, u8 sequence, u8 *payload, u32 len, struct command_data *command)
{
	return get_protocol_command(pm, cmd, sequence, payload, len, command);
}


static struct protocol_header *_parse_command(struct protocol_manager *pm, struct mblue_fifo *fifo)
{
	if (NULL == fifo)
		return NULL;

	//EnterCriticalSection(&pm->lock);
	memset(&pm->protocol_header, 0, sizeof(pm->protocol_header));
	u32 size = fifo->len(fifo);
	if (size >= PROTOCOL_HEADER_PRE_SIZE)
	{
		u8 *temp = mblue_malloc(size + 1);
		fifo->peek(fifo, temp, size);
		pm->protocol_header.head.magic = *temp;
		pm->protocol_header.head.sequence = *(temp + 1);
		pm->protocol_header.head.cmd = *(temp + 2);
		pm->protocol_header.head.length = byteToInt(temp + 3, 4);
		if (pm->protocol_header.head.length > 0)
		{
			if (size >= pm->protocol_header.head.length + 4 + PROTOCOL_HEADER_PRE_SIZE)
			{
				//   param->payload = mblue_malloc(param->head.length);
			  //     memset(pm->protocol_header.payload, 0, sizeof(pm->protocol_header.payload));
				memcpy(pm->protocol_header.payload, temp + PROTOCOL_HEADER_PRE_SIZE, pm->protocol_header.head.length);
				pm->protocol_header.sign = byteToInt(temp + PROTOCOL_HEADER_PRE_SIZE + pm->protocol_header.head.length, 4);

				// remove from fifo
				fifo->get(fifo, temp, pm->protocol_header.head.length + 4 + PROTOCOL_HEADER_PRE_SIZE);
			}
		}
		mblue_free(temp);
		temp = NULL;
	}
	//LeaveCriticalSection(&pm->lock);

	return &pm->protocol_header;
}

static bool check_sign_valid(struct protocol_header *param)
{
	if (NULL == param || param->head.length == 0 || param->payload == NULL)
		return false;

	u32 payloadCrc = get_crc32(param->payload, param->head.length);
	if (payloadCrc == param->sign)
		return true;
	return false;
}

static void _start_parse_service(struct protocol_manager *pm)
{
	if (pm->is_parse_service_run)
		return;

	log_d("protocol manager start parse service \n");

	pm->is_parse_service_run = true;
	while (pm->is_parse_service_run)
	{
		EnterCriticalSection(&pm->lock);
		if (pm->fifo->len(pm->fifo) <= 0)
		{
			LeaveCriticalSection(&pm->lock);
			Sleep(pm->sleep_time);
			continue;
		}

		u8 start = 0;
		pm->fifo->peek(pm->fifo, &start, 1);
		if (start == COMAMND_MAGIC)
		{
			struct protocol_header *header = _parse_command(pm, pm->fifo);
			if (NULL != header)
			{
				if (check_sign_valid(header) && NULL != pm->protocol_listener)
					//	if (NULL != pm->protocol_listener)
				{
					pm->protocol_listener(pm->cb_param, header->head.cmd, header->head.sequence, header->payload, header->head.length);
				}

				// if (NULL != header->payload)
				//     mblue_free(header->payload);

			//    mblue_free(header);
			}
		}
		else
		{
			pm->fifo->get(pm->fifo, &start, 1);
		}
		LeaveCriticalSection(&pm->lock);
		//	Sleep(pm->sleep_time);
	}
}

static void _stop_parse_service(struct protocol_manager *pm)
{

	EnterCriticalSection(&pm->lock);
	pm->is_parse_service_run = false;
	pm->fifo->reset(pm->fifo);
	LeaveCriticalSection(&pm->lock);
	log_d("stop parse service \n");
}

static void _set_sleep(struct protocol_manager *pm, u32 time)
{
	pm->sleep_time = time;
}
///////////////////////////////////////////////////////////////////////////////////////////

static void __install_protocol_mangaer_operation(struct protocol_manager *pm)
{
	if (!pm)
		return;
	pm->init = _init;
	pm->uninit = _uninit;
	pm->add_data = _add_data;
	pm->get_command = _get_command;
	pm->get_command_by_sequence = _get_command_by_sequence;
	pm->start_service = _start_parse_service;
	pm->stop_service = _stop_parse_service;
	pm->set_sleep = _set_sleep;
	pm->is_parse_service_run = false;
	pm->sleep_time = 1;
}

struct protocol_manager *protocol_manager_construct(void)
{
	struct protocol_manager *pm = mblue_malloc(sizeof(struct protocol_manager));
	if (!pm)
		return 0;
	__install_protocol_mangaer_operation(pm);
	return pm;
}
