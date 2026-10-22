/*
 * @Author: xuhan
 * @Date: 2020-02-08 18:10:57
 * @LastEditors  : xuhan
 * @LastEditTime : 2020-02-08 18:13:41
 * @Description:
 */

#include "libspeech_protocol.h"
#include "protocol_manager.h"
#include "mblue_heap.h"

static void _init(struct libspeech_protocol *protocol, LIBSPEECH_PROTOCOL_CALLBACK listener, void *param)
{
	if (protocol->handle)
	{
		mblue_free(protocol->handle);
		protocol->handle = NULL;
	}
	protocol->handle = protocol_manager_construct();
	struct protocol_manager *pm = protocol->handle;
	if (!pm)
		return;


	pm->init(pm, listener, param);
}

static void _uninit(struct libspeech_protocol *protocol)
{
	struct protocol_manager *pm = protocol->handle;
	if (!pm)
		return;

	pm->uninit(pm);
	if (pm)
	{
		mblue_free(protocol->handle);
		protocol->handle = NULL;
	}
}

static void _add_data(struct libspeech_protocol *protocol, unsigned char *data, unsigned int len)
{
	struct protocol_manager *pm = protocol->handle;
	if (!pm)
		return;

	pm->add_data(pm, data, len);
}

static struct libspeech_protocol_command_data *_get_command(struct libspeech_protocol *protocol, unsigned char cmd, unsigned char *payload, unsigned int len, struct libspeech_protocol_command_data *(command))
{
	struct protocol_manager *pm = protocol->handle;
	if (!pm)
		return 0;

	return (struct libspeech_protocol_command_data *)(pm->get_command(pm, cmd, payload, len, (struct command_data*)command));
}

static struct libspeech_protocol_command_data *_get_comman_by_sequence(struct libspeech_protocol *protocol, unsigned char cmd, unsigned char sequence, unsigned char *payload, unsigned int len, struct libspeech_protocol_command_data *(command))
{
	struct protocol_manager *pm = protocol->handle;
	if (!pm)
		return 0;

	return (struct libspeech_protocol_command_data *)(pm->get_command_by_sequence(pm, cmd, sequence, payload, len, (struct command_data*)command));
}

static void _start_service(struct libspeech_protocol *protocol)
{
	struct protocol_manager *pm = protocol->handle;
	if (!pm)
		return;

	pm->start_service(pm);
}

static void _stop_service(struct libspeech_protocol *protocol)
{
	struct protocol_manager *pm = protocol->handle;
	if (!pm)
		return;

	pm->stop_service(pm);
}

static void _set_sleep(struct libspeech_protocol *protocol, unsigned int time)
{
	struct protocol_manager *pm = protocol->handle;
	if (!pm)
		return;

	pm->set_sleep(pm, time);
}

static void __install_libspeech_protocol_operation(struct libspeech_protocol *protocol)
{
	if (!protocol)
		return;

	protocol->init = _init;
	protocol->uninit = _uninit;
	protocol->add_data = _add_data;
	protocol->get_command = _get_command;
	protocol->get_command_by_sequence = _get_comman_by_sequence;
	protocol->start_service = _start_service;
	protocol->stop_service = _stop_service;
	protocol->set_sleep = _set_sleep;
	protocol->handle = NULL;
}

void libspeech_protocol_construct(struct libspeech_protocol *protocol)
{
	if (!protocol)
		return;

	__install_libspeech_protocol_operation(protocol);
}