/*
 * @Descripttion:
 * @version:
 * @Author: xdhuang
 * @Date: 2021-05-20 13:31:09
 * @LastEditors: xdhuang
 * @LastEditTime: 2021-05-28 10:25:04
 * @Copyright: Hefei Madao Information Technology Co., Ltd
 */
#include "common/types.h"
typedef enum
{
	OUT_MOUSE_REPORT = 1,
	OUT_KEYBOARD_REPORT  = (1<<1),
	OUT_CONSUMER_REPORT = (1<<2),
	OUT_SYSYTEM_REPORT = (1<<3),
	OUT_SPP_REPORT = (1<<4),
	OUT_PROT_REPORT = (1<<5)
} OUT_REPORT_TYPE;

typedef enum
{
	IN_PROTOCOL_REPORT = 1,
} IN_REPORT_TYPE;

typedef u8 (*notify_data_out_callback)(u16 type, u8 *data, u32 size);

void data_out_fifo_init(notify_data_out_callback cb);
u8 check_data_out_fifo_empty();
void clear_data_out_fifo();
u8 push_data_out_fifo(u8 *buf, u16 data_type, u32 len);
void data_out_loop_proc();
u32 protocol_send_tick;



