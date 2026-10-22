/*
 * @Descripttion:
 * @version:
 * @Author: xdhuang
 * @Date: 2021-05-20 13:31:09
 * @LastEditors: xdhuang
 * @LastEditTime: 2021-05-28 10:25:08
 * @Copyright: Hefei Madao Information Technology Co., Ltd
 */
#include "data_out_manager.h"
#include "../../common/string.h"
#include "drivers/8258/timer.h"

#define TX_BUF_NUM 32
#define TX_BUF_SIZE 70

typedef struct
{
	u8 fifo[TX_BUF_NUM][TX_BUF_SIZE];
	u8 wptr;
	u8 rptr;
} data_out_info;


typedef struct
{
	u16 type;
	u32 len;
	u8 buf[TX_BUF_SIZE - 6];
} data_pack;

data_out_info tx_fifo;
data_out_info tx_protocol_fifo;
static notify_data_out_callback notify_data_cb;

u8 check_data_out_fifo_empty()
{
	return (tx_fifo.wptr == tx_fifo.rptr) ? 1 : 0;
}

u8 check_protocol_out_fifo_empty()
{
	return (tx_protocol_fifo.wptr == tx_protocol_fifo.rptr) ? 1 : 0;
}

void clear_data_out_fifo()
{
	tx_fifo.wptr = 0;
	tx_fifo.rptr = 0;
	tx_protocol_fifo.wptr = 0;
	tx_protocol_fifo.rptr = 0;
}
void data_out_fifo_init(notify_data_out_callback cb)
{
	notify_data_cb = cb;
	clear_data_out_fifo();
	protocol_send_tick = 0;
}

u8 push_data_out_fifo(u8 *buf, u16 data_type, u32 len)
{
	if(data_type == OUT_PROT_REPORT)
		protocol_send_tick = clock_time() | 1;
	if(notify_data_cb != NULL)
	{
		if(notify_data_cb(data_type, buf, len))
			return 1;
	}
	if(data_type == OUT_MOUSE_REPORT)
	{
		return 1;
	}
	if(data_type == OUT_PROT_REPORT)
	{
		if ((tx_protocol_fifo.wptr - tx_protocol_fifo.rptr) >= TX_BUF_NUM)
		{
			return 0;
		}
		data_pack *p = (data_pack *)tx_protocol_fifo.fifo[tx_protocol_fifo.wptr & (TX_BUF_NUM - 1)];
		p->type = data_type;
		p->len = len;

		memset(p->buf, 0x0, sizeof(p->buf));
		memcpy(p->buf, buf, len);

		tx_protocol_fifo.wptr++;
		return 1;
	}
	if ((tx_fifo.wptr - tx_fifo.rptr) >= TX_BUF_NUM)
	{
		return 0;
	}
	data_pack *p = (data_pack *)tx_fifo.fifo[tx_fifo.wptr & (TX_BUF_NUM - 1)];
	p->type = data_type;
	p->len = len;

	memset(p->buf, 0x0, sizeof(p->buf));
	memcpy(p->buf, buf, len);

	tx_fifo.wptr++;
	return 1;
}

void data_out_loop_proc()
{
	if(!check_data_out_fifo_empty())
	{
		data_pack *p = (data_pack *)tx_fifo.fifo[tx_fifo.rptr & (TX_BUF_NUM - 1)];
		if(notify_data_cb != NULL)
		{
			if(notify_data_cb(p->type, p->buf, p->len))
			{
				tx_fifo.rptr++;
			}
		}
	}
	if(!check_protocol_out_fifo_empty())
	{
		data_pack *p = (data_pack *)tx_protocol_fifo.fifo[tx_protocol_fifo.rptr & (TX_BUF_NUM - 1)];
		if(notify_data_cb != NULL)
		{
			if(notify_data_cb(p->type, p->buf, p->len))
			{
				tx_protocol_fifo.rptr++;
			}
		}
	}
	else
	{
		if (protocol_send_tick > 0 && clock_time_exceed(protocol_send_tick, 5000 * 1000))
		{
			if(notify_data_cb != NULL)
			{
				if(notify_data_cb(0, NULL, 0))
					protocol_send_tick = 0;
			}
		}

	}
}

