
#include "audio_manager.h"

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

#if (BLE_AUDIO_ENABLE1)
static void audio_set_status(int en)
{
    //AMIC Bias output  GPIO_PC4
	gpio_set_func(GPIO_AMIC1_BIAS, AS_GPIO);
	gpio_set_output_en(GPIO_AMIC1_BIAS, 1);
	gpio_set_input_en(GPIO_AMIC1_BIAS, 0);

    if (en) //audio on
    {
        ///////////////////// AUDIO initialization///////////////////
        //buffer_mic set must before audio_init !!!
		//led_close_channel();
    	gpio_setup_up_down_resistor(MIC_P, PM_PIN_UP_DOWN_FLOAT);
    	gpio_setup_up_down_resistor(MIC_M, PM_PIN_UP_DOWN_FLOAT);
    	gpio_write(GPIO_AMIC1_BIAS, 1);
    	gpio_setup_up_down_resistor(GPIO_AMIC1_BIAS, PM_PIN_PULLUP_10K);
        sleep_us(100);
        audio_config_mic_buf((unsigned short*)buffer_mic, TL_MIC_BUFFER_SIZE);
        audio_amic_init(AUDIO_16K);
#if (BATT_CHECK_ENABLE)
        battery_set_detect_enable(0);
#endif
    }
    else //audio off
    {
    	gpio_setup_up_down_resistor(MIC_P, PM_PIN_PULLDOWN_100K);
		gpio_setup_up_down_resistor(MIC_M, PM_PIN_PULLDOWN_100K);
        adc_power_on_sar_adc(0); //power off sar adc
		gpio_write(GPIO_AMIC1_BIAS, 0);
        gpio_setup_up_down_resistor(GPIO_AMIC1_BIAS, PM_PIN_PULLDOWN_100K);
    }

}

void audio_enable()
{
    audio_set_status(1);
}

void audio_disable()
{
    audio_set_status(0);
}

void audio_dispatch()
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

    proc_mic_encoder();
    if (fun_mode == RF_1M_BLE_MODE && blc_ll_getTxFifoNumber() >= RF_TX_FIFO_ALLOW_NUM)
    	return;

	int *p = mic_encoder_data_buffer();
	if (p)
	{
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
#endif
