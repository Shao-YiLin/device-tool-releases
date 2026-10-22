#include "AAA_public_config.h"
#include "logic_protocol_manager.h"
#include "../../third/protocol/protocol_define.h"

#if BUTTON_FUN_ENABLE_AAA
static u8 reset_flag = 0;
static u8 ble_reset_count = 0;
static u32 reset_tick = 0;

extern void set_adv_type();
u8 mode_keycode_flag = OS_TYPE_WINDOWS; ////1-windows;2-android;3-mac
void set_os_type(u8 os_type)
{
	mode_keycode_flag = os_type;
	set_current_device_os_type(os_type);
}

void reset_all_index_pairs()
{
	flash_erase_sector(BONDING_FLASH_ADDR);
	clear_last_pair_device();
	clear_latest_mac_addr();
	remove_all();
	sleep_ms(1000);
	start_reboot();
}
/**
 * @brief  清空当前索引的设备连接数据
 * @note
 * @param  ble_index:
 * @retval None
 */
static void send_ble_reset(int ble_index)
{
	if (connect_ok)
	{
		bls_ll_terminateConnection(HCI_ERR_REMOTE_USER_TERM_CONN);
	}
	if (blc_smp_param_getCurrentBondingDeviceNumber() >= (MAX_BONDING_DEVICE_NUM - 1))
	{
		reset_all_index_pairs();
	}

	set_device_adv_by_reset(ble_index, true);
	set_adv_type();
}

/**
 * @brief  当前的索引的设备发送广播数据
 * @note
 * @param  ble_index:
 * @retval None
 */
static void send_ble_adv(int ble_index)
{
	pair_device_info_t last_device = get_current_adv_device();
	if (last_device.device_index == ble_index)
	{
		if (connect_ok)
		{
			return;
		}
		else
		{
			set_device_adv_by_reset(ble_index, false);
			set_adv_type();
			return;
		}
	}
	else
	{
		if (connect_ok)
		{
			bls_ll_terminateConnection(HCI_ERR_REMOTE_USER_TERM_CONN);
			set_device_adv_by_reset(ble_index, false);
			return;
		}
	}
	set_device_adv_by_reset(ble_index, false);
	set_adv_type();
}

void check_web_process_aaa()
{
	static u8 key_flag = 0;

	if (btn_value == MS_BTN_VOICE) //MS_BTN_CPI -> PIN_BTN_VOICE
	{
		if ((key_flag == 0) && (clock_time_exceed(btn_tick, 4000000)))
		{
			key_flag = 1; //按下语音按键，此标志为置0
			has_web_key_flag = 1;
		}
	}
	else
	{
		key_flag = 0; //按下其他按键，此标志为置0
	}
}

static u32 change_chanel_tick = 0;
void check_reset()
{
	if ((btn_value == MS_BTN_RESET))
	{
		if ((clock_time_exceed(reset_tick, 1000000)))
		{
			reset_tick = clock_time();
			ble_reset_count++;
		}
		if (ble_reset_count > 3)
		{
			if(reset_flag)
			{
				send_ble_reset(ble_adv_channel);
				reset_flag = 0;
			}
		}
		else
			reset_flag = 1;
	}
	else
	{
		if (reset_flag)
		{
			if(clock_time_exceed(change_chanel_tick, 1000000))
			{
				change_chanel_tick = clock_time();
				send_ble_adv((ble_adv_channel % CHANNEL_3) + 1);
			}
			reset_flag = 0;
		}
		ble_reset_count = 0;
	}
}
#endif
