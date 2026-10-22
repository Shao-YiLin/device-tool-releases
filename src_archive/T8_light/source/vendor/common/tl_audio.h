/********************************************************************************************************
 * @file     tl_audio.h 
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
#ifndef _TL_AUDIO_H_
#define _TL_AUDIO_H_

#include "tl_common.h"

#ifndef TL_MIC_PACKET_BUFFER_NUM
#define TL_MIC_PACKET_BUFFER_NUM 16
#endif

#ifndef 	ADPCM_PACKET_LEN
#define 	ADPCM_PACKET_LEN		0
#endif

#ifndef 	TL_MIC_ADPCM_UNIT_SIZE
#define 	TL_MIC_ADPCM_UNIT_SIZE		0
#endif

//#ifndef		TL_MIC_BUFFER_SIZE
//#define		TL_MIC_BUFFER_SIZE		0
//#endif

#if TL_MIC_BUFFER_SIZE
u16 buffer_mic[TL_MIC_BUFFER_SIZE >> 1];
#endif

void proc_mic_encoder(void);
int *mic_encoder_data_buffer();
void mic_encoder_data_read_ok(void);

#endif
