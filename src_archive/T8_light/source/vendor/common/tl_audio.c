/********************************************************************************************************
 * @file     tl_audio.c 
 *
 * @brief    for TLSR chips
 *
 * @author	 public@telink-semi.com;
 * @date     Sep. 18, 2015
 *
 * @par      Copyright (c) Telink Semiconductor (Shanghai) Co., Ltd.
 *           All rights reserved.
 *           
 *			 The information contained herein is confidential and proprietary property of Telink 
 * 		     Semiconductor (Shanghai) Co., Ltd. and is available under the terms 
 *			 of Commercial License Agreement between Telink Semiconductor (Shanghai) 
 *			 Co., Ltd. and the licensee in separate contract or the terms described here-in. 
 *           This heading MUST NOT be removed from this file.
 *
 * 			 Licensees are granted free, non-transferable use of the information in this 
 *			 file under Mutual Non-Disclosure Agreement. NO WARRENTY of ANY KIND is provided. 
 *           
 *******************************************************************************************************/
#include "drivers.h"
#include "tl_audio.h"
//#include "drivers.h"
#include <stack/ble/trace.h>

#include <third/voice/voice_process.h>

#if TL_MIC_BUFFER_SIZE
#define BUFFER_PACKET_SIZE ((ADPCM_PACKET_LEN >> 2) * TL_MIC_PACKET_BUFFER_NUM)

int		buffer_mic_enc[BUFFER_PACKET_SIZE];
u8		buffer_mic_pkt_wptr;
u8		buffer_mic_pkt_rptr;

u32		adb_t2;


void proc_mic_encoder(void)
{
	static u16 buffer_mic_rptr;
	u16 mic_wptr = get_mic_wr_ptr();
	u16 l = (mic_wptr >= buffer_mic_rptr) ? (mic_wptr - buffer_mic_rptr) : 0xffff;

	if (l >= (TL_MIC_BUFFER_SIZE >> 2))
	{

		u16 *ps = buffer_mic + buffer_mic_rptr;

		short outLen = 0;
		codec_encode(
			(char*)ps,
			(char *)(buffer_mic_enc + (ADPCM_PACKET_LEN >> 2) * (buffer_mic_pkt_wptr & (TL_MIC_PACKET_BUFFER_NUM - 1))), &outLen);

		buffer_mic_rptr = buffer_mic_rptr ? 0 : (TL_MIC_BUFFER_SIZE >> 2);
		buffer_mic_pkt_wptr++;
		int pkts = (buffer_mic_pkt_wptr - buffer_mic_pkt_rptr) & (TL_MIC_PACKET_BUFFER_NUM * 2 - 1);
		if (pkts > TL_MIC_PACKET_BUFFER_NUM)
		{
			buffer_mic_pkt_rptr++;
			log_event(TR_T_adpcm_enc_overflow);
		}

		log_task_end(TR_T_adpcm);
	}
}

int *mic_encoder_data_buffer()
{
	if (buffer_mic_pkt_rptr == buffer_mic_pkt_wptr)
	{
		return 0;
	}

	int *ps = buffer_mic_enc + (ADPCM_PACKET_LEN >> 2) *
								   (buffer_mic_pkt_rptr & (TL_MIC_PACKET_BUFFER_NUM - 1));

	return ps;
}

void mic_encoder_data_read_ok (void)
{
	buffer_mic_pkt_rptr++;
}

#endif
