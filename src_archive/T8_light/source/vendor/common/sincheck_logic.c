#include "../8258_dual_kb/logic_protocol_manager.h"
#include "../8258_dual_kb/audio_manager.h"
#include "../../third/protocol/protocol_manager.h"
#include "../../third/protocol/protocol_define.h"
#include "SinAudioCheck.h"
#include "tl_audio.h"

#if SINCHECK_PCM_DATA_ENABLE
#define SPB_CMD 8

static u8 real_pcm_index = 0;
static short short_pcm_buff[320 * SPB_CMD] = {0};
static u8 delay_cmd = 0;
static u8 sincheck_cmd = 0;
static void real_pam_to_one_pcm_buff(s16 *buff)
{
	memcpy(short_pcm_buff + 320 * real_pcm_index, buff, 640);
	printf("real_pcm_index: %d \r\n", real_pcm_index);
}

static void checkin_pcm()
{
	int ret = -1;
	ret = checkAgreement(short_pcm_buff, 320 * SPB_CMD);
	printf("ret :%d\r\n", ret);
	if (ret == 0)
		get_logic_protocol_manager()->send_notify_data(get_protocol_manager_instance()->event_pack(TYPE_SINCHECK, ERR_SUCCESS));
	else
		get_logic_protocol_manager()->send_notify_data(get_protocol_manager_instance()->event_pack(TYPE_SINCHECK, ERR_FAIL));
	sincheck_cmd = 0;
}

static void real_pcm_buff_to_checkin()
{
	audio_disable();
	checkin_pcm();
	memset(short_pcm_buff, 0, 640 * SPB_CMD);
	real_pcm_index = 0;
	delay_cmd = 0;
}

void start_sincheck()
{
	sincheck_cmd |= 1;
	audio_enable();
	return;
}

static u32 mic_buffer_timer;
void sincheck_loop()
{
	if(!sincheck_cmd)
		return ;
	static u32 audioProcTick = 0;
	if (clock_time_exceed(audioProcTick, 1000))
	{
		audioProcTick = clock_time();
	}
	else
	{
		return;
	}
	static u16	buffer_mic_rptr;
	u16 mic_wptr = get_mic_wr_ptr();
	u16 l = (mic_wptr >= buffer_mic_rptr) ? (mic_wptr - buffer_mic_rptr) : 0xffff;
	if (l >=(TL_MIC_BUFFER_SIZE>>2))
	{
		s16 *ps = (s16 *)buffer_mic + buffer_mic_rptr;
		mic_buffer_timer = clock_time() | 1;
		delay_cmd++;
		if (delay_cmd > 150)
		{
			if (real_pcm_index < SPB_CMD)
			{
				real_pam_to_one_pcm_buff(ps);
				// real_pam_to_one_pcm_buff(buffer);
				real_pcm_index++;
			}
			else
			{
				real_pcm_buff_to_checkin();
				// audio_disable_cmd = 1;
				printf("stop\r\n");
			}
		}
	}
	else
	{
		if (clock_time_exceed(mic_buffer_timer, 1000000))
		{
			audio_config_mic_buf((unsigned short*)buffer_mic, TL_MIC_BUFFER_SIZE);
			audio_amic_init(AUDIO_16K);
		}
	}

}
#endif
