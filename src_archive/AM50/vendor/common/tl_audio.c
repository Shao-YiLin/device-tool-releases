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
#include "../8258_dual_mouse/AAA_public_Config.h"
#include "../8258_dual_mouse/logic_protocol_manager.h"
#include "../../third/protocol/protocol_manager.h"
#include "../../third/protocol/protocol_define.h"
#include "SinAudioCheck.h"
#define	AUDIO_GAIN 0

#define				NUM_OF_ORIG_SAMPLE				2
//char buffer[640] = {0};
////char buffer1[640] = {0};
//static int CalcWave(char *buffer, int len)
//{
//
//	int l = len / 2;
//	for (int i = 0; i < l; i++)
//	{
//		short v0 = (((short)buffer[i * 2 + 1] << 8) | buffer[2 * i]);
//		voice_power = (v0 > voice_power) ? v0 : ((-v0 > voice_power) ? -v0 : voice_power);
//	}
//	voice_power = (int)((voice_power * 30) >> 15);
////	voice_power = (voice_power + 3) * 2;
////	printf("sss:%d\r\n", voice_power);
//	if (voice_power > 30)
//		voice_power = 30;
//	else if(voice_power < 0)
//	{
//		voice_power = abs(voice_power);
//	}
//	printf("ddd:%d\r\n", voice_power);
//	return voice_power;
//}


#if		TL_MIC_BUFFER_SIZE

#define	BUFFER_PACKET_SIZE		((ADPCM_PACKET_LEN >> 2) * TL_MIC_PACKET_BUFFER_NUM)

int		buffer_mic_enc[BUFFER_PACKET_SIZE];
u8		buffer_mic_pkt_wptr;
u8		buffer_mic_pkt_rptr;

#if		AUDIO_GAIN
static u8 audio_gain = 0x30;
static u8 low_vol_index;
static int CalcWave(short *buffer, int len)
{
	int voice_power;
	for (int i = 0; i < len; i++)
	{
		//voice_power = (buffer[i] > voice_power) ? buffer[i] : ((-buffer[i] > voice_power) ? -buffer[i] : voice_power);
		unsigned short voice_power_1;
		voice_power_1 = abs(buffer[i]);
		voice_power = (int)(voice_power_1 > voice_power) ? voice_power_1 : voice_power;
	}
	if(voice_power >= 26000)
	{
		low_vol_index = 0;
		if(audio_gain > 0x20)
		{
			audio_gain --;
			printf("\r\audio-%x \r\n", audio_gain);
			reg_aud_alc_vol_l_chn = MASK_VAL( FLD_AUD_ALC_MIN_VOLUME_IN_DIGITAL_MODE,  audio_gain, \
												  	  FLD_AUD_ALC_DIGITAL_MODE_AUTO_REGULATE_EN, 0);
		}
	}
	else if(voice_power < 10000){
		low_vol_index ++;
		if(low_vol_index > 50)
		{
			low_vol_index = 30;
			if(audio_gain < 0x2b){
				audio_gain ++;
				printf("\r\audio+%x \r\n", audio_gain);
				reg_aud_alc_vol_l_chn = MASK_VAL( FLD_AUD_ALC_MIN_VOLUME_IN_DIGITAL_MODE,  audio_gain, \
														  FLD_AUD_ALC_DIGITAL_MODE_AUTO_REGULATE_EN, 0);
			}
		}
	}
	else
	{
		low_vol_index = 0;
	}
	return voice_power;
}
#endif

u32		adb_t2;


#define TL_NOISE_SUPRESSION_ENABLE 0 // TODO : too much calculation can have packet drop
char buffer[640] = {0};
//char buffer1[640] = {0};
short voice_power_1;
#if PWM_LED_ENABLE
static int CalcWave(char *buffer, int len)
{

	int l = len / 2;
	for (int i = 0; i < l; i++)
	{
		short v0 = (((short)buffer[i * 2 + 1] << 8) | buffer[2 * i]);
		voice_power = (v0 > voice_power) ? v0 : ((-v0 > voice_power) ? -v0 : voice_power);
	}
	voice_power = (int)((voice_power * 30) >> 15);
	if (voice_power > 30)
		voice_power = 30;
	else if(voice_power < 0)
	{
		voice_power = abs(voice_power);
	}
//	printf("\r\nvvv: %d \r\n", voice_power);
	return voice_power;
}
#endif

_attribute_ram_code_ void proc_mic_encoder(void)
{
	static u16	buffer_mic_rptr;
	u16 mic_wptr = get_mic_wr_ptr();
	u16 l = (mic_wptr >= buffer_mic_rptr) ? (mic_wptr - buffer_mic_rptr) : 0xffff;
	if (l >=(TL_MIC_BUFFER_SIZE>>2)) {
		s16 *ps = buffer_mic + buffer_mic_rptr;
#if		AUDIO_GAIN
		CalcWave(ps, 320);
#endif



#if(ENCODER_TYPE)
		short outLen = 0;

//#if PWM_LED_ENABLE
//		memcpy(buffer, ps, 640);
//		CalcWave(buffer, 640);
//#endif

		codec_encode(
				(char*)ps,
				(char *) (buffer_mic_enc
						+ (ADPCM_PACKET_LEN >> 2) * (buffer_mic_pkt_wptr
								& (TL_MIC_PACKET_BUFFER_NUM - 1))), &outLen);

#else
		mic_to_adpcm_split ( ps, TL_MIC_ADPCM_UNIT_SIZE,
				(s16 *)(buffer_mic_enc + (ADPCM_PACKET_LEN>>2) *
						(buffer_mic_pkt_wptr & (TL_MIC_PACKET_BUFFER_NUM - 1))), 1);
#endif
		buffer_mic_rptr = buffer_mic_rptr ? 0 : (TL_MIC_BUFFER_SIZE >> 2);
		buffer_mic_pkt_wptr++;
		int pkts = (buffer_mic_pkt_wptr - buffer_mic_pkt_rptr) & (TL_MIC_PACKET_BUFFER_NUM*2-1);
		if (pkts > TL_MIC_PACKET_BUFFER_NUM) {
			buffer_mic_pkt_rptr++;
			log_event (TR_T_adpcm_enc_overflow);
		}

		log_task_end (TR_T_adpcm);
	}
}

int	*mic_encoder_data_buffer ()
{
	if (buffer_mic_pkt_rptr == buffer_mic_pkt_wptr) {
			return 0;
	}

	int *ps = buffer_mic_enc + (ADPCM_PACKET_LEN>>2) *
			(buffer_mic_pkt_rptr & (TL_MIC_PACKET_BUFFER_NUM - 1));
	//buffer_mic_pkt_rptr++;

	return ps;
}

void mic_encoder_data_read_ok (void)
{
	buffer_mic_pkt_rptr++;
}

#endif


