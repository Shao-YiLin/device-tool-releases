/*
 * @Author: xuhan
 * @Date: 2020-02-08 18:11:28
 * @LastEditors  : xuhan
 * @LastEditTime : 2020-02-08 18:11:49
 * @Description:
 */
#ifndef _PROTOCOL_MANAGER_H_
#define _PROTOCOL_MANAGER_H_
#include "include/commondefine.h"
#include "mblue_fifo.h"
#include <windows.h>

struct common_header
{
	u8 magic;
	u8 sequence;
	u8 cmd;
	u32 length;
};

struct protocol_header
{
	struct common_header head;
	// u8 *payload;
	u8 payload[256];
	u32 sign;
};

struct command_data
{
	// u8 *data;
	u8 data[128];
	u8 len;
	u8 sequence;
};

enum DataType
{
	RECORD = 0x1,
	COMMAND = 0x2
};

/*
 *
 */
typedef void(*ProtocolListener)(void *, u8, u8, u8 *, u32);

struct protocol_manager
{
	bool is_parse_service_run;
	ProtocolListener protocol_listener;
	struct mblue_fifo *fifo;

	u8 global_sequence;
	struct protocol_header protocol_header;
	CRITICAL_SECTION lock;
	u32 sleep_time;
	void *cb_param;

	void(*init)(struct protocol_manager *pm, ProtocolListener listener, void *param);
	void(*uninit)(struct protocol_manager *pm);
	void(*add_data)(struct protocol_manager *pm, u8 *data, u32 len);
	struct command_data *(*get_command)(struct protocol_manager *pm, u8 cmd, u8 *payload, u32 len, struct command_data *command);
	struct command_data *(*get_command_by_sequence)(struct protocol_manager *pm, u8 cmd, u8 sequence, u8 *payload, u32 len, struct command_data *command);
	void(*start_service)(struct protocol_manager *pm);
	void(*stop_service)(struct protocol_manager *pm);
	void(*set_sleep)(struct protocol_manager *pm, u32 time);
};

struct protocol_manager *protocol_manager_construct(void);

#endif
