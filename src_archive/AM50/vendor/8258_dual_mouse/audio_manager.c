#include "common/types.h"
#include "drivers/8258/gpio_8258.h"
#include "drivers/8258/dfifo.h"
#include "common/string.h"
#include "AAA_public_config.h"
#include "AAA_app_config.h"
#include "../common/tl_audio.h"
#include "../../third/protocol/protocol_manager.h"
#include "../../third/protocol/protocol_define.h"
#include "../../third/voice/voice_process.h"
#include "logic_protocol_manager.h"
#include "audio_manager.h"
#include "mouse_effect_logic_manager.h"
#if BLE_AUDIO_ENABLE
static void audio_set_status(u8 en)
{
	//AMIC Bias output
	gpio_set_func(GPIO_AMIC1_BIAS, AS_GPIO);
	gpio_set_output_en(GPIO_AMIC1_BIAS, 1);
	gpio_set_input_en(GPIO_AMIC1_BIAS, 0);


	if (en)
	{ //audio on
    	gpio_write(GPIO_AMIC1_BIAS, 1);

 
		audio_config_mic_buf((unsigned short*)buffer_mic, TL_MIC_BUFFER_SIZE);
		//Amic config
		audio_amic_init(AUDIO_16K);
	}
	else
	{ //audio off
		adc_power_on_sar_adc(0);
		gpio_write(GPIO_AMIC1_BIAS, 0);
        gpio_setup_up_down_resistor(GPIO_AMIC1_BIAS, PM_PIN_PULLDOWN_100K);

	}
#if (BATT_CHECK_ENABLE)
	battery_set_detect_enable(!en); //Voice and power acquisition can not be used at the same time, are ADC
#endif
}

void audio_enable()
{
    audio_set_status(1);
}

void audio_disable()
{
    audio_set_status(0);
}

void audio_dispatch(void)
{
	static u32 audioProcTick = 0;
	if (clock_time_exceed(audioProcTick, 1000))
	{
		audioProcTick = clock_time();
	}
	else
	{
		return;
	}

	///////////////////////////////////////////////////////////////
	proc_mic_encoder();
	//////////////////////////////////////////////////////////////////
	if (blc_ll_getTxFifoNumber() < 9)
	{
		int *p = mic_encoder_data_buffer();

		if (p)
		{
			//total1channel++;
			struct command_data *record =
				get_protocol_manager_instance()->record(RECORD_CHANNEL_0, (u8 *)p, ADPCM_PACKET_LEN);
			if (record)
			{
				if (BLE_SUCCESS == get_logic_protocol_manager()->send_notify_data(record))
				{
					mic_encoder_data_read_ok();
				}
			}
		}
	}
}

#endif
