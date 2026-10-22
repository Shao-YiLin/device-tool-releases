/********************************************************************************************************
 * @file     aaa_app.c
 *
 * @brief    for TLSR chips
 *
 * @author	 telink
 * @date     Sep. 30, 2010
 *
 * @par      Copyright (c) 2010, Telink Semiconductor (Shanghai) Co., Ltd.
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

#include "AAA_public_config.h"
#include "../common/tl_audio.h"
#include "../../third/protocol/protocol_manager.h"
#include "../../third/protocol/protocol_define.h"
#include "../../third/voice/voice_process.h"
#include "battery_manager.h"
#include "logic_protocol_manager.h"
#include "pair_info_manager.h"
#include "audio_manager.h"
#include "pair_info_manager.h"
#include "mouse_effect_logic_manager.h"
#include "../../third/fifo/mblue_fifo.h"
#include "OTA_manager.h"
#include "sensor/sensor_proxy.h"
#if PWM_LED_ENABLE
#include "RGB/RGB_config_file.h"
#endif

#define MY_APP_ADV_CHANNEL BLT_ENABLE_ADV_ALL
#define MY_ADV_INTERVAL_MIN ADV_INTERVAL_20MS
#define MY_ADV_INTERVAL_MAX ADV_INTERVAL_25MS
#define USER_OWN_ADDRESS_TYPE OWN_ADDRESS_RANDOM

typedef enum
{
	INIT_DONE_AAA,
	BEGIN_CONNECT_AAA,
	SMP_DONE_AAA,
	CONNECTED_DONE_AAA,
	CONNECTED_LOOP_AAA,

} BLE_MODE_CONNECT_STEP_AAA;


#define TX_FIFO_THRESHOLD_VALUE1 4
#define TX_FIFO_THRESHOLD_VALUE2 8
#define TX_FIFO_THRESHOLD_VALUE3 12
#define INSPECTIN_INTERVAL 5  //loop执行相应次数后校验loop周期
#define CYCLE_TIME_MIN 7450
#define CYCLE_TIME_MAX 17500
typedef struct{
	u32 cycle_time;
	u8 inspection_tick;
}dynamic_interval_param;
dynamic_interval_param dynamic_interval_info;

#define SIG_PROC_ENABLE 0
#define LONG_SUSPEND_TIMER_AAA 200
#define DEFAULT_INTERVAL 6
#define DEFAULT_LATENCY 60


#define DEFAULT_TIMEOUT 300

_attribute_data_retention_user u32 start_tick;
_attribute_data_retention_user u8 Switch_Adv_Type = 0;
_attribute_data_retention_user u8 bonded_peer_addr[6] = {0, 0, 0, 0, 0, 0};
_attribute_data_retention_user u8 ui_mic_enable = 0;
_attribute_data_retention_user u8 protocol_start_record_cmd;
_attribute_data_retention_user u8 protocol_stop_record_cmd;
_attribute_data_retention_user u32 loop_cnt;
_attribute_data_retention_user u32 connect_begin_tick;
_attribute_data_retention_user u8 ui_ota_is_working = 0;
_attribute_data_retention_user u8 conn_step = 0;
_attribute_data_retention_user u32 adv_begin_tick;
_attribute_data_retention_user u32 adv_count = 0;
_attribute_data_retention_user u8 ble_status_aaa;
_attribute_data_retention_user u32 idle_tick;
_attribute_data_retention_user u32 idle_count;
_attribute_data_retention_user u8 binding_master_addr[32] = {0};
_attribute_data_retention_user u8 temp_master_addr[14] = {0};
_attribute_data_retention_user int binding_master_addr_idx;
_attribute_data_retention_ u8 blt_rxfifo_b[RX_FIFO_SIZE * RX_FIFO_NUM] = {0};
_attribute_data_retention_ my_fifo_t blt_rxfifo =
	{
		RX_FIFO_SIZE,
		RX_FIFO_NUM,
		0,
		0,
		blt_rxfifo_b,
};

_attribute_data_retention_ u8 blt_txfifo_b[TX_FIFO_SIZE * TX_FIFO_NUM] = {0};
_attribute_data_retention_ my_fifo_t blt_txfifo =
	{
		TX_FIFO_SIZE,
		TX_FIFO_NUM,
		0,
		0,
		blt_txfifo_b,
};

static u32 connect_reject_timer_tick = 0;

//static u32 connect_accept_timer_tick = 0;

#define ADV_ADD_PROTOCOL_VERSION 1
//////////Adv Packet, Response Packet//////////////////////////////////////////////

_attribute_data_retention_user u8 tbl_advData_aaa[31] = {
	0x02, GAP_ADTYPE_FLAGS, 0x05,					   // BLE limited discoverable mode and BR/EDR not supported
	0x03, GAP_ADTYPE_APPEARANCE, 0xc2, 0x03,		   // 384, 0xc2:mouse(0xc1: keyboard), Generic category,
	0x03, GAP_ADTYPE_16BIT_INCOMPLETE, 0x12, 0x18,	   // incomplete list of service class UUIDs (0x1812-HID SERVICE, 180F-BATTERY)
	0x03, GAP_ADTYPE_MANUFACTURER_SPECIFIC, 0x03, 0x0, // app protocol version
	19, GAP_ADTYPE_LOCAL_NAME_SHORT, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

u8 lowbatt_flag = 0;
#if (BLE_AUDIO_ENABLE)
void ui_enable_mic(u8 en)
{
	ui_mic_enable = en;
	if (en)
	{
		audio_enable();
	}
	else
	{
		audio_disable();
		rgb_front()->set_single_colour(rgb_front(), close_all_rgb);
		rgb_back()->set_single_colour(rgb_back(), close_all_rgb);
	}
}
#endif

void connect_params_proc()
{
	bls_l2cap_requestConnParamUpdate(DEFAULT_INTERVAL, DEFAULT_INTERVAL, DEFAULT_LATENCY, DEFAULT_TIMEOUT);
	connect_reject_timer_tick = clock_time() | 1;
}

void app_switch_to_indirect_adv(u8 e, u8 *p, int n)
{
	Switch_Adv_Type += 1;

	if (Switch_Adv_Type & 0x01)
	{

		bls_ll_setAdvParam(MY_ADV_INTERVAL_MIN, MY_ADV_INTERVAL_MAX,
						   ADV_TYPE_CONNECTABLE_UNDIRECTED, USER_OWN_ADDRESS_TYPE,
						   0, NULL,
						   MY_APP_ADV_CHANNEL,
						   ADV_FP_NONE);
	}
	else
	{

		bls_ll_setAdvParam(ADV_INTERVAL_3_75MS, ADV_INTERVAL_3_75MS,
						   ADV_TYPE_CONNECTABLE_DIRECTED_LOW_DUTY, USER_OWN_ADDRESS_TYPE,
						   BLE_ADDR_PUBLIC, bonded_peer_addr,
						   BLT_ENABLE_ADV_ALL,
						   ADV_FP_NONE);
	}

	bls_ll_setAdvDuration(1000000, 1);

	bls_ll_setAdvEnable(1); //must: set adv enable
}
_attribute_ram_code_ void user_set_rf_power(u8 e, u8 *p, int n)
{
	rf_set_power_level_index(user_cfg.tx_power);
	//exit_suspend_flag=1;
}

int cccWrite(void *p)
{
//	if (conn_step != SMP_DONE_AAA)
//	{
//		conn_step = SMP_DONE_AAA;
//		connect_begin_tick = clock_time() | 1;
//	}
	return 0;
}
void task_connect(u8 e, u8 *p, int n)
{
	connect_begin_tick = clock_time() | 1;
	//clear_fifo();
	//reset_idle_status();
	connect_ok = 1;
	ui_ota_is_working = 0;
	//conn_params_pending=0;
	conn_step = BEGIN_CONNECT_AAA;
	bls_ll_setAdvDuration(0, 0);
	ble_status_aaa = BEGIN_CONNECTED_STATUS_AAA;

#if 1 //CUST_SAVE_MASTER_INFO_ENABLE_AAA
	memcpy(temp_master_addr, p - 2, 14);
#endif

#if BLE_AUDIO_ENABLE
	ui_mtu_size_exchange_req = 1;
#endif
}

void task_terminate(u8 e, u8 *p, int n) //*p is terminate reason
{
	if ((ble_status_aaa == DEEP_TERMINATE_STATUS_AAA))
	{
		ble_status_aaa = DEEP_SLEEPE_STATUS_AAA;
	}
	else
	{
		ble_status_aaa = POWER_ON_STATUS_AAA;
	}
	conn_step = 0;
	ui_ota_is_working = 0;
	connect_ok = 0;
	protocol_start_record_cmd = 0;
	protocol_stop_record_cmd = 0;
#if (BLE_AUDIO_ENABLE)
	if (ui_mic_enable)
	{
		ui_enable_mic(0);
	}
#endif
	has_web_key_flag = 0;
}

void task_gpio_eary_wakeup(u8 e, u8 *p, int n)
{
	if (e == BLT_EV_FLAG_GPIO_EARLY_WAKEUP)
	{
		ui_loop();
	}
}

// slave connect param update callback function
// len = 6，  interval = p[0] | p[1] << 8, latency = p[2] | p[3] <<8
static void conn_param_update_callback(u8 e, u8 *p, int n){
	if(n != 6)
		return;
	u32 interval = (*p) | (*(p+1) << 8);
	if(interval != DEFAULT_INTERVAL)
	{
		if(ble_status_aaa == T5S_CONNECTED_STATUS_AAA)
			connect_params_proc();
	}

}

// master request update connect param callback function
// len = 11, interval = p[3] | p[4] << 8
static void conn_param_request_callback(u8 e, u8 *p, int n){
	if(n != 11)
		return;
	u8 *p_value = p + 3;
	u32 interval = (*p_value) | (*(p_value+1) << 8);
	if(interval != DEFAULT_INTERVAL)
	{
		if(ble_status_aaa == T5S_CONNECTED_STATUS_AAA)
			connect_params_proc();
	}
}

extern void set_os_type(u8 os_type);
extern void reset_web_flag();
int app_host_event_callback(u32 h, u8 *para, int n)
{
	u8 event = h & 0xFF;
	switch (event)
	{
	case GAP_EVT_SMP_PARING_FAIL:
	{
		gap_smp_paringFailEvt_t *p = (gap_smp_paringFailEvt_t *)para;
		if (p->reason == PARING_FAIL_REASON_UNSPECIFIED_REASON)
		{
			bls_ll_terminateConnection(HCI_ERR_REMOTE_USER_TERM_CONN); //push terminate cmd into ble TX buffer
		}
	}
	break;
	case GAP_EVT_SMP_PARING_SUCCESS:
	{
		// printf("Bonding Device:%d\n",blc_smp_param_getCurrentBondingDeviceNumber());
		if(blc_smp_param_getCurrentBondingDeviceNumber()>=1)
			save_new_connect_device();
		else{
			// printf("SMP num error terminate\n");
			bls_ll_terminateConnection(HCI_ERR_REMOTE_USER_TERM_CONN);
		}
	}
	break;

	case GAP_EVT_SMP_CONN_ENCRYPTION_DONE:
	{
		bls_ll_setAdvEnable(0);
		gap_smp_connEncDoneEvt_t *p = (gap_smp_connEncDoneEvt_t *)para;
		if (p->re_connect == SMP_STANDARD_PAIR || p->re_connect == SMP_FAST_CONNECT) //first paring
		{
			//first paring
		//	printf("first paring\n");
			if (p->re_connect == SMP_STANDARD_PAIR)
				conn_step = BEGIN_CONNECT_AAA;
			else
				conn_step = SMP_DONE_AAA;
			u8 peer_addr_type = (*(temp_master_addr) & BIT(6)) ? BLE_ADDR_RANDOM : BLE_ADDR_PUBLIC;
			u8 os_type = OS_TYPE_WINDOWS;
			set_current_device_adv_type(peer_addr_type);
			if(peer_addr_type == BLE_ADDR_RANDOM){
				os_type = OS_TYPE_ANDROID;
			}if(is_mac_address(temp_master_addr + 2) == 1){
				os_type = OS_TYPE_MAC;
			}
			if(save_current_device(temp_master_addr + 8, temp_master_addr + 2, peer_addr_type, os_type))
			{
//				reset_web_flag();
			}

			else
				bls_ll_terminateConnection(HCI_ERR_REMOTE_USER_TERM_CONN);
//			if (memcmp(temp_master_addr, &binding_master_addr[8 * flash_dev_info.mast_id], 8))
//			{
//				memcpy(&binding_master_addr[8 * flash_dev_info.mast_id], temp_master_addr, 8);
//				binding_master_addr_idx += 32;
//				flash_write_page(CFG_MAST_ADDR + binding_master_addr_idx, 32, binding_master_addr);
//			}
//			if (deep_flag == BLE_PAIR_REBOOT_ANA_AAA)
//			{
//
//				write_deep_ana0(CLEAR_FLAG_ANA_AAA);
//				flash_dev_info.slave_mac_addr[flash_dev_info.mast_id]++;
//				save_dev_info_flash();
//			}
		}
//		else if (p->re_connect == SMP_FAST_CONNECT) //auto connect
//		{
//		//	printf("auto connect\n");
//			conn_step = SMP_DONE_AAA;
//			pair_device_info_t device = get_current_adv_device();
//			set_os_type(device.os_type);
//			force_save_current_device(FALSE);
//		}
		connect_begin_tick = clock_time() | 1;
	}
	break;
	case GAP_EVT_ATT_EXCHANGE_MTU:
	{
	}
	break;
	default:
		break;
	}

	return 0;
}

_attribute_ram_code_ void ble_remote_set_sleep_wakeup(u8 e, u8 *p, int n)
{
#if 1
	if (suspend_wake_up_enable && (blc_ll_getCurrentState() == BLS_LINK_STATE_CONN) && (((u32)(bls_pm_getSystemWakeupTick() - clock_time())) > 20 * CLOCK_16M_SYS_TIMER_CLK_1MS)) //suspend time > 30ms.add gpio wakeup
	{
		bls_pm_setWakeupSource(PM_WAKEUP_PAD); //gpio pad wakeup suspend/deepsleep
#if WHEEL_FUN_ENABLE_AAA
//		wheel_set_wakeup_level_suspend(1);
#endif
#if BUTTON_FUN_ENABLE_AAA
		btn_set_wakeup_level_suspend(1);
#endif
#if SENSOR_FUN_ENABLE_AAA
		mos_proxy_get_instance()->set_wakeup_level(1);
#endif
	}
#endif
}

void set_adv_type();

static u8 update_param_counter = 0;
int app_conn_param_update_response(u8 id, u16 result)
{
	if(result == CONN_PARAM_UPDATE_ACCEPT)
	{
		connect_reject_timer_tick = 0;
		update_param_counter = 0;
		if(ble_status_aaa == OK_CONNECTED_STATUS_AAA || ble_status_aaa == MUTI_DEVICE_CHANGE_STATUS_AAA)
			ble_status_aaa = T5S_CONNECTED_STATUS_AAA;
	}
	else if(result == CONN_PARAM_UPDATE_REJECT)
	{
		if(update_param_counter < 16)
		{
			update_param_counter ++;
		}
		else {
			update_param_counter = 0;
			if(ble_status_aaa == OK_CONNECTED_STATUS_AAA || ble_status_aaa == MUTI_DEVICE_CHANGE_STATUS_AAA)
				ble_status_aaa = T5S_CONNECTED_STATUS_AAA;
		}
		connect_reject_timer_tick = clock_time() | 1;
	}
	return 0;
}

int att_sig_proc_handler(u16 connHandle, u8 *p)
{
	return 0;
}

#if (OTA_ENABLE_AAA)
void app_enter_ota_mode(void)
{
	ui_ota_is_working = 1;
	bls_ota_setTimeout(160 * 1000 * 1000); //set OTA timeout  15 seconds
}
void app_debug_ota_result(int result)
{
	irq_disable();
	//flash_erase_sector(CFG_DEVICE_MODE_ADDR);
	wd_stop();
#if (BLE_OTA_LED_DEBUG && BLT_APP_LED_ENABLE)
	if (result == OTA_SUCCESS) //OTA success
	{
		sleep_us(2000000); //led on for 2 second
	}
	else //OTA fail
	{
		for (int i = 0; i < 4; i++)
		{
			sleep_us(250000);
			sleep_us(250000);
		}
	}
#endif
}
#endif

/**
 * batter state change will call this function
 */
static void _battery_status_change_callback(u8 status)
{
	if (status == 0) /////////////////////////normal_power
	{
		if(lowbatt_flag){
			lowbatt_flag = 0;
		}
	}
	else if (status == 1) ////////////////////low_power
	{
		if (charging_batt_count == 0) //////////锛坒ully charged or not charge锛塁harging does not enter low power consumption
		{
			lowbatt_flag = 1;
			rgb_back()->set_single_colour(rgb_back(), close_all_rgb);
			rgb_front()->set_single_colour(rgb_front(), close_all_rgb);
		}
	}
	else if (status == 2) ////////////////////ultra_low_power
	{
		if (charging_batt_count == 0) //////////锛坒ully charged or not charge锛塁harging does not enter ultra low power consumption
		{
			lowbatt_flag = 2;
			rgb_back()->set_single_colour(rgb_back(), close_all_rgb);
			rgb_front()->set_single_colour(rgb_front(), close_all_rgb);
		}
	}
	else if (status == 3) ////////////////////send batt to app
	{
		if (connect_ok && (ble_status_aaa == T5S_CONNECTED_STATUS_AAA))
		{
			if (charging_batt_count == 0)
				get_logic_protocol_manager()->send_notify_data(get_protocol_manager_instance()->event_pack(TYPE_BATTERY,
																												   batt_vol_mv_real));
		}

	}
}

void user_init_normal(void)
{
	vMblueIdentificationInit();
	blt_txfifo.wptr = 0;
	blt_txfifo.rptr = 0;
	blt_rxfifo.wptr = 0;
	blt_rxfifo.rptr = 0;
	random_generator_init(); //this is must
	load_paired_device();	 //get history device information
	init_mouse_device_data();
	data_out_fifo_init(ble_notify_data_proc);
	dynamic_interval_info.cycle_time = CYCLE_TIME_MIN;
#if (ENCODER_TYPE == 1)
	codec_init();
#endif
	//blc_app_loadCustomizedParameters(); //load customized freq_offset cap value//zzh
	memcpy(mouse_mac_address, (u8 *)&user_cfg.dev_mac, 6);
	mouse_mac_address[4] = MACADDR_CHANNEL;
	mouse_mac_address[5] = MACADDR_AUTH_TYPE;
	binding_master_addr_idx = flash_info_load_aaa(CFG_MAST_ADDR, binding_master_addr, 32);
	////// Controller Initialization  //////////
	blc_ll_initBasicMCU();				 //mandatory
	set_last_device_adv();				 //get current device param
	blc_ll_initConnection_module();		 //connection module  mandatory for BLE slave/master
	blc_ll_initSlaveRole_module();		 //slave module: 	 mandatory for BLE slave,
	blc_ll_initPowerManagement_module(); //pm module:      	 optional

	////// Host Initialization  //////////
	blc_gap_peripheral_init(); //gap initialization
	extern void my_att_init();
	my_att_init();										  //gatt initialization
	blc_l2cap_register_handler(blc_l2cap_packet_receive); //l2cap initialization
	blc_smp_peripheral_init();
	blc_smp_param_setBondingDeviceMaxNumber(MAX_BONDING_DEVICE_NUM);
	// Hid device on android7.0/7.1 or later version
	// New paring: send security_request immediately after connection complete
	// reConnect:  send security_request 1000mS after connection complete. If master start paring or encryption before 1000mS timeout, slave do not send security_request.
	blc_smp_configSecurityRequestSending(SecReq_IMM_SEND, SecReq_PEND_SEND, 1000); //if not set, default is:  send "security request" immediately after link layer connection established(regardless of new connection or reconnection )

	blc_gap_registerHostEventHandler(app_host_event_callback);
	blc_gap_setEventMask(GAP_EVT_MASK_SMP_PARING_FAIL |
						 GAP_EVT_MASK_SMP_PARING_SUCCESS |
						 GAP_EVT_MASK_ATT_EXCHANGE_MTU |
						 GAP_EVT_MASK_SMP_CONN_ENCRYPTION_DONE);

	user_set_rf_power(0, 0, 0);
	bls_app_registerEventCallback(BLT_EV_FLAG_SUSPEND_EXIT, &user_set_rf_power);
	blc_l2cap_registerConnUpdateRspCb(app_conn_param_update_response);

	//ble event call back
	bls_app_registerEventCallback(BLT_EV_FLAG_CONNECT, &task_connect);
	bls_app_registerEventCallback(BLT_EV_FLAG_TERMINATE, &task_terminate);
	bls_app_registerEventCallback(BLT_EV_FLAG_ADV_DURATION_TIMEOUT, &app_switch_to_indirect_adv);
	bls_app_registerEventCallback(BLT_EV_FLAG_GPIO_EARLY_WAKEUP, &task_gpio_eary_wakeup);
	bls_app_registerEventCallback(BLT_EV_FLAG_CONN_PARA_UPDATE, &conn_param_update_callback);
	bls_app_registerEventCallback(BLT_EV_FLAG_CONN_PARA_REQ, &conn_param_request_callback);

	///////////////////// Power Management initialization///////////////////
	blc_ll_initPowerManagement_module();
	bls_pm_setSuspendMask(SUSPEND_CONN);
	bls_app_registerEventCallback(BLT_EV_FLAG_SUSPEND_ENTER, &ble_remote_set_sleep_wakeup);

#if (BATT_CHECK_ENABLE) //battery check must do before OTA relative operation
	battery_manager_init(250000000, _battery_status_change_callback);
//	battery_manager_init(5000000, _battery_status_change_callback);
#endif

#if (OTA_ENABLE_AAA)
	////////////////// OTA relative ////////////////////////
	bls_ota_clearNewFwDataArea(); //must
	bls_ota_registerStartCmdCb(app_enter_ota_mode);
	bls_ota_registerResultIndicateCb(app_debug_ota_result); //debug
#endif
	start_tick = clock_time();
	web_flag = analog_read(DEEP_ANA_REG9); //web
}

void set_rgb_gpio_mode()
{
	gpio_set_func(GPIO_PB0, AS_GPIO);
	gpio_setup_up_down_resistor(GPIO_PB0, PM_PIN_PULLDOWN_100K);
	gpio_set_func(GPIO_PA3, AS_GPIO);
	gpio_setup_up_down_resistor(GPIO_PA3, PM_PIN_PULLDOWN_100K);
	gpio_set_func(GPIO_PC6, AS_GPIO);
	gpio_setup_up_down_resistor(GPIO_PC6, PM_PIN_PULLDOWN_100K);
	gpio_set_func(GPIO_PA4, AS_GPIO);
	gpio_setup_up_down_resistor(GPIO_PA4, PM_PIN_PULLDOWN_100K);
	gpio_set_func(GPIO_PC7, AS_GPIO);
	gpio_setup_up_down_resistor(GPIO_PC7, PM_PIN_PULLDOWN_100K);
	gpio_set_func(GPIO_PA0, AS_GPIO);
	gpio_setup_up_down_resistor(GPIO_PA0, PM_PIN_PULLDOWN_100K);
}

void enter_deep_aaa()
{
	write_deep_ana0(DEEP_SLEEP_ANA_AAA);
#if BUTTON_FUN_ENABLE_AAA
	btn_set_wakeup_level_deep();
#endif
	set_rgb_gpio_mode();
	cpu_set_gpio_wakeup(GPIO_BATT_CHECK, 1, 1); //pad  wakeup deep
#if WHEEL_FUN_ENABLE_AAA
	wheel_set_wakeup_level_deep();
#endif
#if SENSOR_FUN_ENABLE_AAA
	mos_proxy_get_instance()->set_wakeup_level(1);
#endif
	cpu_sleep_wakeup(DEEPSLEEP_MODE, PM_WAKEUP_PAD, 0);
}

extern void sensor_powrer_down();
void enter_deep_aaa_ultra()
{
	write_deep_ana0(DEEP_SLEEP_ANA_AAA);
	set_rgb_gpio_mode();
	cpu_set_gpio_wakeup(GPIO_BATT_CHECK, 1, 1); //pad  wakeup deep
#if SENSOR_FUN_ENABLE_AAA
	mos_proxy_get_instance()->set_wakeup_level(0);
	mos_proxy_get_instance()->set_power_mode(POWER_DOWN_MODE);
#endif
	cpu_sleep_wakeup(DEEPSLEEP_MODE, PM_WAKEUP_PAD, 0);
}

u8 direct_ble_flag = 0;
void set_adv_type()
{
	//bls_ll_setAdvEnable(0);
	sleep_ms(5);
	direct_ble_flag = 0;
	pair_device_info_t device = get_current_adv_device();
	ble_adv_channel = device.device_index;
	if (device.adv_type == BLE_ADDR_RANDOM || device.adv_type == BLE_ADDR_INVALID)
	{
		bls_ll_setAdvDuration(0, 0);
		bls_ll_setAdvParam(MY_ADV_INTERVAL_MIN, MY_ADV_INTERVAL_MAX,
						   ADV_TYPE_CONNECTABLE_UNDIRECTED, USER_OWN_ADDRESS_TYPE,
						   0, NULL,
						   MY_APP_ADV_CHANNEL,
						   ADV_FP_NONE);
	}
	else
	{
		if (device.valid_flag == 1)
		{
			direct_ble_flag = 1;
			bls_ll_setAdvDuration(0, 0);
			bls_ll_setAdvParam(ADV_INTERVAL_3_75MS, ADV_INTERVAL_3_75MS,
							   ADV_TYPE_CONNECTABLE_DIRECTED_LOW_DUTY, USER_OWN_ADDRESS_TYPE,
							   BLE_ADDR_PUBLIC, device.host_mac,
							   BLT_ENABLE_ADV_ALL,
							   ADV_FP_NONE);
			//	bls_ll_setAdvDuration(2000000, DIRECT_ADV_TO_UNDIRECT_ENABLE);
		}
		else
		{
			bls_ll_setAdvDuration(0, 0);
			bls_ll_setAdvParam(MY_ADV_INTERVAL_MIN, MY_ADV_INTERVAL_MAX,
							   ADV_TYPE_CONNECTABLE_UNDIRECTED, USER_OWN_ADDRESS_TYPE,
							   0, NULL,
							   MY_APP_ADV_CHANNEL,
							   ADV_FP_NONE);
		}
	}
#if BLT_APP_LED_ENABLE
		led_ble_Adv();
#endif
	sleep_ms(10);
	bls_ll_setAdvEnable(1);
}

void adv_count_poll();
u8 adving_flag = 0; //In the charging state.There will be no response after 30S broadcast, and it will not go into sleep, but the Bluetooth light will be off, and the Bluetooth light will flash again for 30S after pressing the button again
static void device_adv_state_proc()
{
	if (connect_count)
		return;
	adv_count_poll();
	if (adv_count < BLE_ADV_TIMER_OUT)
	{
#if BLT_APP_LED_ENABLE
		led_ble_Adv();
#endif
	}
	else
	{
		adving_flag = 1;
		if (charging_batt_count == 0) //no charge
		{
			ble_status_aaa = DEEP_SLEEPE_STATUS_AAA;
			//printf("DEEP_SLEEPE_STATUS_AAA\r\n");
		}
		else
		{
			bls_ll_setAdvEnable(0);
		}
	}
}

void ble_status_proc_aaa(u8 is_new_key_event)
{
	if (ble_status_aaa == T5S_CONNECTED_STATUS_AAA)
	{
		connect_count = 1;
#if BLE_CONNECT_ENTER_DEEPSLEEP_AFTER_TIME_OUT_ENABLE_AAA
		if(flash_dpi_info.ble_time_out > 0)
		{
			if ((idle_count >= flash_dpi_info.ble_time_out) && (charging_batt_count == 0) && (ui_mic_enable == 0))
			{
				bls_ll_terminateConnection(HCI_ERR_REMOTE_USER_TERM_CONN);
				ble_status_aaa = DEEP_TERMINATE_STATUS_AAA;
			}
		}
#endif
		//connect_params_proc();
	}
	else if (ble_status_aaa == POWER_ON_STATUS_AAA)
	{
		connect_count = 0;
		Switch_Adv_Type = 0;
		set_adv_type();
		adv_begin_tick = clock_time() | 1;
		adv_count = 0;
		ble_status_aaa = LOW_ADV_STATUS_AAA;
	}
	else if (ble_status_aaa == LOW_ADV_STATUS_AAA)
	{
#if BLE_ADV_ENTER_DEEPSLEEP_AFTER_TIME_OUT_ENABLE_AAA
		if (!adving_flag)
			device_adv_state_proc();
#endif
	}
	else if (ble_status_aaa == BEGIN_CONNECTED_STATUS_AAA)
	{
		connect_count = 1;
		if (conn_step == BEGIN_CONNECT_AAA)
		{
			//clear_fifo();
			if (clock_time_exceed(connect_begin_tick, ENTER_ESTABLISH_MILL_SECOND * 1000))
			{
				ble_status_aaa = OK_CONNECTED_STATUS_AAA;
			}
		}
		else if (conn_step == SMP_DONE_AAA)
		{
			ble_status_aaa = OK_CONNECTED_STATUS_AAA;
		}
	}
	else if (ble_status_aaa == OK_CONNECTED_STATUS_AAA)
	{
		clear_data_out_fifo();
		get_logic_protocol_manager()->clear_cache();
#if BLT_APP_LED_ENABLE
		led_Connected();
#endif
#if BLE_WEB_ENABLE
		pair_device_info_t device = get_current_adv_device();
		if ((device.host_mac[4] == 0x49) && (device.host_mac[5] == 0x4d))
			dongle_ble_web = 1;
		else
			dongle_ble_web = 0;
#endif
		connect_params_proc();
		ble_status_aaa = MUTI_DEVICE_CHANGE_STATUS_AAA;
	}
	else if(ble_status_aaa == MUTI_DEVICE_CHANGE_STATUS_AAA)
	{
		if(connect_reject_timer_tick > 0 && clock_time_exceed(connect_reject_timer_tick, 500000)){
			connect_params_proc();
		}
	}
	else if (ble_status_aaa == DEEP_SLEEPE_STATUS_AAA)
	{
		if ((ui_ota_is_working == 0) && (!blc_ll_isControllerEventPending()))
		{
			enter_deep_aaa();
		}
	}
}
void reset_idle_status()
{
	idle_count = 0;
	loop_cnt = 0;
	idle_tick = clock_time();
	sensor_operation_flag = 1;
	adv_begin_tick = idle_tick | 1;
	adv_count = 0;
	if (adving_flag) //In the charging state.There will be no response after 30S broadcast, and it will not go into sleep, but the Bluetooth light will be off, and the Bluetooth light will flash again for 30S after pressing the button again
	{
		bls_ll_setAdvEnable(1);
		adving_flag = 0;
	}
}

void idle_status_poll()
{
	u8 n;
	n = ((u32)(clock_time() - idle_tick)) / CLOCK_16M_SYS_TIMER_CLK_1S;
	idle_tick += n * CLOCK_16M_SYS_TIMER_CLK_1S;
	idle_count += n;
}

void adv_count_poll()
{
	u8 n;
	n = ((u32)(clock_time() - adv_begin_tick)) / CLOCK_16M_SYS_TIMER_CLK_1S;
	adv_begin_tick += n * CLOCK_16M_SYS_TIMER_CLK_1S;
	adv_count += n;
}

static u32 battery_timer_tick;
void check_batt()
{
	if(battery_is_charging())//充电器插入
	{
		if(charging_batt_count == 0)
		{
			get_logic_protocol_manager()->send_notify_data(get_protocol_manager_instance()->event_pack(TYPE_BATTERY, 5000));
			charging_batt_count = 1;
			battery_timer_tick = clock_time();
		}
		lowbatt_flag = 0; //////////////////////////////////////do not enter low power reminder when charging
		if(clock_time_exceed(battery_timer_tick, 500 * 1000))
		{
			if(battery_is_full())
			{
				rgb_front()->set_single_colour(rgb_front(), charge_full_rgb);
				rgb_back()->set_single_colour(rgb_back(), charge_full_rgb);
			}
			else
			{
				setting_rgb_data rgb_value = get_rgb_value_by_index(1);
				rgb_front()->set_breathing_colour(rgb_front(), RECHARGE_RGB_PRIORITY, 0xff, 3000, 1000, 1000, (rgb_data *)&rgb_value, 1);
				rgb_back()->set_breathing_colour(rgb_back(), RECHARGE_RGB_PRIORITY, 0xff, 3000, 1000, 1000, (rgb_data *)&rgb_value, 1);
			}
		}
	}
	else //充电器未插入
	{
		if (battery_get_detect_enable() && charging_batt_count){
			reset_idle_status();
			charging_batt_count = 0; ////////////////////////////////Allow entering low power RGB_red flicker
			reset_battery_detect_time();
			battery_manager_check_proc();
			if(!ui_mic_enable)
			{
				rgb_front()->set_single_colour(rgb_front(), close_all_rgb);
				rgb_back()->set_single_colour(rgb_back(), close_all_rgb);
			}
			get_logic_protocol_manager()->send_notify_data(get_protocol_manager_instance()->event_pack(TYPE_BATTERY, batt_vol_mv_real));
		}

	}
}

static u8 five_seconds_flag = 0;
_attribute_data_retention_user u32 ultra_pow_tick;
static void five_seconds_power_off()
{
	if (clock_time_exceed(ultra_pow_tick, 1000000))
	{
		ultra_pow_tick = clock_time();
		five_seconds_flag++;
	}
	if (five_seconds_flag == 4)
	{
		five_seconds_flag = 0;
		enter_deep_aaa_ultra();
	}
}

static void is_low_or_ultra_low_power()
{
	if (lowbatt_flag == 1) ///////////low_power
	{
		rgb_front()->set_single_colour(rgb_front(), low_battery_rgb);
		rgb_back()->set_single_colour(rgb_back(), low_battery_rgb);
	}
	else if (lowbatt_flag == 2) ///////ultra_low_power
	{
		rgb_front()->set_single_colour(rgb_front(), ultra_low_battery_rgb);
		rgb_back()->set_single_colour(rgb_back(), ultra_low_battery_rgb);
		five_seconds_power_off(); /////In the ultra-low power state, it will enter deep sleep after one minute, and the button cannot wake up,It can only be woken up by charging it or switching it on and off
	}
	else if(ui_mic_enable == 0 && charging_batt_count == 0)
	{
		if(sensor_operation_flag)
		{
			if(mouse_propertie_param.RGB_theme == RGB_BREATH_MODE)
			{
				rgb_back()->set_breathing_colour(rgb_back(), BREATHING_PRIORITY, 0xff, 3000, 2000, 1000,
						(rgb_data *)mouse_propertie_param.breathe_RGB_param.rgb_data, mouse_propertie_param.breathe_RGB_param.rgb_data_len);
				rgb_front()->set_breathing_colour(rgb_front(), BREATHING_PRIORITY, 0xff, 3000, 2000, 1000,
						(rgb_data *)mouse_propertie_param.breathe_RGB_param.rgb_data, mouse_propertie_param.breathe_RGB_param.rgb_data_len);

			}
			else if(mouse_propertie_param.RGB_theme == RGB_NORMAL_MODE)
			{
				rgb_front()->set_single_colour(rgb_front(), custom_theme(&mouse_propertie_param.userdefined_RGB_param.rgb_data, 0xff));
				rgb_back()->set_single_colour(rgb_back(), custom_theme(&mouse_propertie_param.userdefined_RGB_param.rgb_data, 0xff));
			}
			if(idle_count > 30)
			{
				sensor_operation_flag = 0;
				rgb_front()->set_single_colour(rgb_front(), close_all_rgb);
				rgb_back()->set_single_colour(rgb_back(), close_all_rgb);
			}
		}
	}
}

void ble_pm_aaa()
{
#if BLE_AUDIO_ENABLE
	if ((ui_mic_enable) || (charging_batt_count) || (protocol_send_flag) || rgb_front()->get_repeatCount(rgb_front()) || rgb_back()->get_repeatCount(rgb_back())|| (lowbatt_flag != 0) || sensor_operation_flag )
	{
		bls_pm_setSuspendMask(SUSPEND_DISABLE);
		return;
	}
#endif
#if BLE_APP_PM_ENABLE
	if (has_web_key_flag || btn_value || get_ble_led_condition() || ui_ota_is_working || (loop_cnt < LONG_SUSPEND_TIMER_AAA) || (lowbatt_flag != 0))
	{
		if(super_mouse_flag && flash_polling_rate.ble_frequency > DEFAULT_BLE_FREQUENCY)
		{
			bls_pm_setSuspendMask(SUSPEND_DISABLE);
			suspend_wake_up_enable = 0;
			return;
		}
		bls_pm_setManualLatency(0);
#if 1
		if (suspend_wake_up_enable)
		{
			//start_tick = clock_time();
			bls_pm_setWakeupSource(0);
#if WHEEL_FUN_ENABLE_AAA
			wheel_set_wakeup_level_suspend(0);
#endif
#if BUTTON_FUN_ENABLE_AAA
			btn_set_wakeup_level_suspend(0);
#endif
#if SENSOR_FUN_ENABLE_AAA
		mos_proxy_get_instance()->set_wakeup_level(0);
#endif
		}
#endif
		suspend_wake_up_enable = 0;
	}
	else
	{
		loop_cnt = LONG_SUSPEND_TIMER_AAA + 1;
		bls_pm_setWakeupSource(PM_WAKEUP_PAD);
#if 1
		if (suspend_wake_up_enable == 0)
		{
#if WHEEL_FUN_ENABLE_AAA
			wheel_set_wakeup_level_suspend(1);
#endif
#if BUTTON_FUN_ENABLE_AAA
			btn_set_wakeup_level_suspend(1);
#endif
#if SENSOR_FUN_ENABLE_AAA
			mos_proxy_get_instance()->set_wakeup_level(1);
#endif
		}
#endif
		suspend_wake_up_enable = 1;
	}
	bls_pm_setSuspendMask(SUSPEND_CONN);
#endif
}

void dealwith_protocol_cmd()
{
	if (protocol_start_record_cmd == START_RECORD)
	{
		if (ui_mic_enable == 0)
		{
			ui_enable_mic(1);
			get_logic_protocol_manager()->start_record_respond(ERR_SUCCESS);
			protocol_start_record_cmd = 0;
			reset_idle_status();
		}
		else
		{
			get_logic_protocol_manager()->start_record_respond(ERR_SUCCESS);
			protocol_start_record_cmd = 0;
		}
	}
	else if (protocol_stop_record_cmd == STOP_RECORD)
	{
		if (ui_mic_enable)
		{
			ui_enable_mic(0);
			get_logic_protocol_manager()->stop_record_respond(ERR_SUCCESS);
			protocol_stop_record_cmd = 0;
			reset_idle_status();
		}
		else
		{
			get_logic_protocol_manager()->stop_record_respond(ERR_SUCCESS);
			protocol_stop_record_cmd = 0;
		}
	}
}

void user_requestMtuSizeExchange(void)
{

	if (ui_mtu_size_exchange_req && blc_ll_getCurrentState() == BLS_LINK_STATE_CONN)
	{
		ui_mtu_size_exchange_req = 0;
		blc_att_requestMtuSizeExchange(BLS_CONN_HANDLE, 0x009e);
	}
}

static u8 blemode_to_usbmode_flag = 0;
extern void sincheck_loop();
void ui_loop()
{
#if (BLE_AUDIO_ENABLE)
	dealwith_protocol_cmd();

	if (ui_mic_enable)
	{
		if(lowbatt_flag == 0)
		{
			rgb_front()->set_single_colour(rgb_front(), voice_rgb_by_value(0,255,255));
			rgb_back()->set_single_colour(rgb_back(), voice_rgb_by_value(0,255,255));
		}

		audio_dispatch();
	}
#endif

#if (BATT_CHECK_ENABLE)
	check_batt(); //Test whether it is charging, light effect display
	if (battery_get_detect_enable())
		battery_manager_check_proc();
#if MODULE_USB_ENABLE
	if(charging_batt_count)
	{
		if(blemode_to_usbmode_flag == 0)
		{
			usb_init_interrupt();
			usb_set_pin_en();
		}
		blemode_to_usbmode();
		blemode_to_usbmode_flag |= 1;
	}
	else
	{
		if(blemode_to_usbmode_flag)
		{
			usb_dp_pullup_en (0);
			gpio_set_func(GPIO_PA5, AS_GPIO);
			gpio_set_func(GPIO_PA6, AS_GPIO);
			usbhw_disable_manual_interrupt(FLD_CTRL_EP_AUTO_STD | FLD_CTRL_EP_AUTO_DESC);
			blemode_to_usbmode_flag = 0;
		}
	}
#endif
#endif

	get_ble_data_report_aaa();
	loop_cnt++;

	data_out_loop_proc();
	super_mouse_proc();

#if BLT_APP_LED_ENABLE
	led_hw_proc();
#endif

	is_low_or_ultra_low_power();

#if BLE_WEB_ENABLE
	if(has_web_key_flag){
		if(dongle_ble_web){
			u32 product_no_32;
			memcpy(&product_no_32, device_product_no, 4);
			if(get_logic_protocol_manager()->send_notify_data(get_protocol_manager_instance()->event_pack(TYPE_WEB, product_no_32)) == BLE_SUCCESS)
				has_web_key_flag = 0;
		}
		else{
			open_website();
		}
	}
#endif

#if SINCHECK_PCM_DATA_ENABLE
	sincheck_loop();
#endif
#if PROTOCOL_OTA_ENABLE
	ota_loop_proc();
#endif
#if PWM_LED_ENABLE
	rgb_back()->loop(rgb_back(), 1);
	rgb_front()->loop(rgb_front(), 1);
#endif
}

static u32 dynamical_connection_param(){
	if(ui_mic_enable)
		return dynamic_interval_info.cycle_time = CYCLE_TIME_MIN;
	dynamic_interval_info.inspection_tick ++;
	if(dynamic_interval_info.inspection_tick % INSPECTIN_INTERVAL)
		return dynamic_interval_info.cycle_time;
	u8 txFifoNumber = blc_ll_getTxFifoNumber();
	if(txFifoNumber < TX_FIFO_THRESHOLD_VALUE1){
		dynamic_interval_info.cycle_time -= 500;
	}
	else if(txFifoNumber < TX_FIFO_THRESHOLD_VALUE2){
		dynamic_interval_info.cycle_time += 1000;
	}
	else if(txFifoNumber < TX_FIFO_THRESHOLD_VALUE3){
		dynamic_interval_info.cycle_time += 2000;
	}
	else{
		dynamic_interval_info.cycle_time += 3000;
	}

	if(dynamic_interval_info.cycle_time < CYCLE_TIME_MIN){
		dynamic_interval_info.cycle_time = CYCLE_TIME_MIN;
	}
	else if(dynamic_interval_info.cycle_time > CYCLE_TIME_MAX){
		dynamic_interval_info.cycle_time = CYCLE_TIME_MAX;
	}
	return dynamic_interval_info.cycle_time;
}

/////////////////////////////////////////////////////////////////////
// main loop flow
/////////////////////////////////////////////////////////////////////


u32 _get_loop_cycle()
{
	if(!super_mouse_flag )
		return dynamical_connection_param();
	else if(flash_polling_rate.ble_frequency <= DEFAULT_BLE_FREQUENCY)
		return CYCLE_TIME_MIN;
	else return (1000000/flash_polling_rate.ble_frequency);
}

void main_loop(void)
{
	////////////////////////////////////// BLE entry /////////////////////////////////
	blt_sdk_main_loop();
	if (clock_time_exceed(start_tick, dynamic_interval_info.cycle_time))
	{
		start_tick = clock_time();
		////////////////////////////////////// UI entry /////////////////////////////////
	#if (BLE_AUDIO_ENABLE)
		user_requestMtuSizeExchange();
	#endif

		//start_tick += dynamic_interval_info.cycle_time * 16;
		ui_loop();
		//dynamical_connection_param();
		dynamic_interval_info.cycle_time = _get_loop_cycle();
		idle_status_poll();

		ble_status_proc_aaa(has_new_key_event);

		ble_pm_aaa();
	}
}
