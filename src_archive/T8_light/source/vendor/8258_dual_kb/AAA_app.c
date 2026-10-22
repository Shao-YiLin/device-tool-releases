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
#include "../../third/fifo/mblue_fifo.h"
#include "logic_protocol_manager.h"
#include "battery_manager.h"
#include "audio_manager.h"
#include "keyboard_scan.h"
#include "pair_info_manager.h"
#include "OTA_manager.h"


#define CUST_SAVE_MASTER_INFO_ENABLE_AAA 1

#define MY_APP_ADV_CHANNEL BLT_ENABLE_ADV_ALL
#define MY_ADV_INTERVAL_MIN ADV_INTERVAL_20MS
#define MY_ADV_INTERVAL_MAX ADV_INTERVAL_25MS
#define USER_OWN_ADDRESS_TYPE OWN_ADDRESS_RANDOM
#define MCU_NORMAL_SLEEP 1

typedef enum
{
	INIT_DONE_AAA,
	FIRST_PAIR_DONE_AAA,
	READ_GATT_DONE_AAA,
	RECONN_DONE_AAA,
	ALL_DONE_AAA,

} CONNECT_STEP_AAA;


#define TX_FIFO_THRESHOLD_VALUE1 4
#define TX_FIFO_THRESHOLD_VALUE2 8
#define TX_FIFO_THRESHOLD_VALUE3 12
#define INSPECTIN_INTERVAL 5  //loop执行相应次数后校验loop周期
#define CYCLE_TIME_MIN 7500
#define CYCLE_TIME_MAX 17500
typedef struct{
	u32 cycle_time;
	u8 inspection_tick;
}dynamic_interval_param;
dynamic_interval_param dynamic_interval_info;

#define SIG_PROC_ENABLE 0
#define LONG_SUSPEND_TIMER_AAA 200

#define DEFAULT_LATENCY 60
#define DEFAULT_INTERVAL 6

#define DEFAULT_TIMEOUT 300

_attribute_data_retention_user u32 start_tick;

_attribute_data_retention_user u8 Switch_Adv_Type = 0;
_attribute_data_retention_user u8 bonded_peer_addr[6] = {0, 0, 0, 0, 0, 0};
_attribute_data_retention_user u8 ui_mic_enable = 0;
_attribute_data_retention_user u8 ui_mtu_size_exchange_req = 0;
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
_attribute_data_retention_ u8 blt_rxfifo_b[RX_FIFO_SIZE * RX_FIFO_NUM] = {0};
u8 connect_count;
u8 charging_flag = 0;
_attribute_data_retention_ my_fifo_t blt_rxfifo = {
	RX_FIFO_SIZE,
	RX_FIFO_NUM,
	0,
	0,
	blt_rxfifo_b,
};

_attribute_data_retention_ u8 blt_txfifo_b[TX_FIFO_SIZE * TX_FIFO_NUM] = {0};
_attribute_data_retention_ my_fifo_t blt_txfifo = {
	TX_FIFO_SIZE,
	TX_FIFO_NUM,
	0,
	0,
	blt_txfifo_b,
};

static u32 connect_reject_timer_tick = 0;
//////////Adv Packet, Response Packet//////////////////////////////////////////////

_attribute_data_retention_user u8 tbl_advData_aaa[31] = {
	0x02, GAP_ADTYPE_FLAGS, 0x05,					   // BLE limited discoverable mode and BR/EDR not supported
	0x03, GAP_ADTYPE_APPEARANCE, 0xc1, 0x03,		   // 384, Keyboard, Generic category,
	0x03, GAP_ADTYPE_16BIT_INCOMPLETE, 0x12, 0x18,	   // incomplete list of service class UUIDs (0x1812-HID SERVICE, 180F-BATTERY)
	0x03, GAP_ADTYPE_MANUFACTURER_SPECIFIC, 0x03, 0x0, // app protocol version
	19, GAP_ADTYPE_LOCAL_NAME_SHORT, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};


void set_adv_scanRsp_data()
{
	if (device_name_len == 0)
	{
		device_name_len = sizeof(DEVICE_NAME_AAA);
		if (device_name_len > USER_DEVICE_NAME_MAX_LEN)
		{
			device_name_len = USER_DEVICE_NAME_MAX_LEN;
		}
		memcpy(user_cfg.device_name, DEVICE_NAME_AAA, device_name_len);
	}
	else
	{
		if (device_name_len > USER_DEVICE_NAME_MAX_LEN)
		{
			device_name_len = USER_DEVICE_NAME_MAX_LEN;
		}
	}
	memset(&user_cfg.device_name[device_name_len], 0, USER_DEVICE_NAME_MAX_LEN - device_name_len);
	tbl_advData_aaa[14] = device_protocol_version;
	memcpy(&tbl_advData_aaa[DEV_POSITION], user_cfg.device_name, device_name_len);
	tbl_advData_aaa[DEV_POSITION - 2] = device_name_len + 1;
	bls_ll_setAdvData((u8 *)tbl_advData_aaa, device_name_len + DEV_POSITION);
	bls_ll_setScanRspData(&tbl_advData_aaa[DEV_POSITION - 2], device_name_len + 2);
}

#if BLE_AUDIO_ENABLE1
void user_requestMtuSizeExchange(void)
{

	if (ui_mtu_size_exchange_req && blc_ll_getCurrentState() == BLS_LINK_STATE_CONN)
	{
		ui_mtu_size_exchange_req = 0;
		blc_att_requestMtuSizeExchange(BLS_CONN_HANDLE, 0x009e);
	}
}
#endif

#if (BLE_AUDIO_ENABLE1)
void ui_enable_mic(u8 en)
{
	ui_mic_enable = en;
	if (en)
	{
    	start_audio_light();
		audio_enable();
	}
	else
	{
    	stop_audio_light();
		audio_disable();
	}
}
#endif

void connect_params_proc()
{
	bls_l2cap_requestConnParamUpdate(DEFAULT_INTERVAL, DEFAULT_INTERVAL, DEFAULT_LATENCY, DEFAULT_TIMEOUT);
//	connect_reject_timer_tick = clock_time() | 1;// 更新完也要继续更新，直到成功或次数大于16次
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
		//	printf("UNDIRECTED\r\n");
	}
	else
	{
		bls_ll_setAdvParam(ADV_INTERVAL_3_75MS, ADV_INTERVAL_3_75MS,
						   ADV_TYPE_CONNECTABLE_DIRECTED_LOW_DUTY, USER_OWN_ADDRESS_TYPE,
						   BLE_ADDR_PUBLIC, bonded_peer_addr,
						   BLT_ENABLE_ADV_ALL,
						   ADV_FP_NONE);
		//	printf("DIRECTED\r\n");
	}

	bls_ll_setAdvDuration(1000000, 1);

	bls_ll_setAdvEnable(1); //must: set adv enable
}

_attribute_ram_code_ void user_set_rf_power(u8 e, u8 *p, int n)
{
	rf_set_power_level_index((user_cfg.tx_power));
	//exit_suspend_flag=1;
}

int cccWrite(void *p)
{
//	if (conn_step != RECONN_DONE_AAA)
//	{
//		conn_step = READ_GATT_DONE_AAA;
//		connect_begin_tick = clock_time() | 1;
//	}
//
	return 0;
}

void task_connect(u8 e, u8 *p, int n)
{
	connect_begin_tick = clock_time() | 1;
	clear_data_out_fifo();
	//reset_idle_status();
	clear_pair_flag();
	connect_ok = 1;
	ui_ota_is_working = 0;
	//conn_params_pending=0;
	conn_step = 0;
	bls_ll_setAdvDuration(0, 0);
	ble_status_aaa = BEGIN_CONNECTED_STATUS_AAA;

#if CUST_SAVE_MASTER_INFO_ENABLE_AAA
	memcpy(temp_master_addr, p - 2, 14);
#endif
	if(get_current_adv_device().device_index == CHANNEL_1)
	{
		u8 mac_dongle[3] = BOUND_DONGLE_MAC;
		if(memcmp(temp_master_addr + 5, mac_dongle, 3))
		{
			bls_ll_terminateConnection(HCI_ERR_REMOTE_USER_TERM_CONN);
			conn_step = 0;
			return;
		}
	}
	ui_mtu_size_exchange_req = 1;
	//rf_packet_connect_t *pc = (rf_packet_connect_t *)(p - 6);

#if AUTO_CHECK_OS_TYPE

	//peer_type=pc->type;
	//peer_con_interval=pc->interval;
	//peer_con_timeout=pc->timeout;

	if (os_type == UNKNOW_OS_TYPE)
	{
		if (pc->txAddr)
		{
			if ((pc->interval >= 0x0c) && (pc->interval <= 0x18) && (pc->timeout <= 0xc8))
			{
				os_type = APPLE_OS_TYPE;
			}
			else
			{
				os_type = ANDROID_OS_TYPE;
			}
		}
		else //public
		{

			// os_type maybe detect errors
			//because  some andriod phone is public for exampe huawei asw=4.4.3
			os_type = WINDOWS_OS_TYPE;
		}
	}
#endif
}

void task_terminate(u8 e, u8 *p, int n) //*p is terminate reason
{
#if 1
	if ((ble_status_aaa == DEEP_TERMINATE_STATUS_AAA))
	{
		ble_status_aaa = DEEP_SLEEPE_STATUS_AAA;
	}
	else
	{
		ble_status_aaa = POWER_ON_STATUS_AAA;
	}
	has_web_key_flag = 0;

#else
	bls_ll_setAdvEnable(0);
#endif
#if BLT_SOFTWARE_TIMER_ENABLE
	delet_soft_time();
#endif
	conn_step = 0;
	ui_ota_is_working = 0;
	connect_ok = 0;
    protocol_start_record_cmd = 0;
    protocol_stop_record_cmd = 0;
#if AUTO_CHECK_OS_TYPE

	os_type = UNKNOW_OS_TYPE;
#endif

#if (BLE_AUDIO_ENABLE1)
	if (ui_mic_enable)
	{
		ui_enable_mic(0);
	}
#endif
}

void task_gpio_eary_wakeup(u8 e, u8 *p, int n)
{
	ui_loop();
}

// slave connect param update callback function
// len = 6，  interval = p[0] | p[1] << 8, latency = p[2] | p[3] <<8
static void conn_param_update_callback(u8 e, u8 *p, int n){
	if(n != 6)
		return;
	u32 interval = (*p) | (*(p+1) << 8);
	if(interval != DEFAULT_INTERVAL)
	{
		connect_reject_timer_tick = clock_time() | 1;
	}
//	printf("LUP:%d\n", (*p+2) | (*(p+3) << 8));

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
		connect_reject_timer_tick = clock_time() | 1;
	}
//	printf("LRE:%d\n", (*p+5) | (*(p+6) << 8));
}

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
	}
	break;

	case GAP_EVT_SMP_CONN_ENCRYPTION_DONE:
	{
		bls_ll_setAdvEnable(0);
		gap_smp_connEncDoneEvt_t *p = (gap_smp_connEncDoneEvt_t *)para;
		if (p->re_connect == SMP_STANDARD_PAIR || p->re_connect == SMP_FAST_CONNECT)
		{
			if (p->re_connect == SMP_STANDARD_PAIR)
				conn_step = FIRST_PAIR_DONE_AAA;
			else
				conn_step = RECONN_DONE_AAA;
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
//				//bls_smp_enableParing (SMP_PARING_CONN_TRRIGER);
//				flash_write_page(CFG_MAST_ADDR + binding_master_addr_idx, 32, binding_master_addr);
//			}
//			if (deep_flag == BLE_PAIR_REBOOT_ANA_AAA)
//			{
//				write_deep_ana0(CLEAR_FLAG_ANA_AAA);
//				flash_dev_info.slave_mac_addr[flash_dev_info.mast_id]++;
//				save_dev_info_flash();
//				printf("save dev info2\n");
//			}
		}
//		else if (p->re_connect == SMP_FAST_CONNECT)
//		{ //auto connect
//			conn_step = RECONN_DONE_AAA;
//			pair_device_info_t device=get_current_adv_device();
//			set_os_type(device.os_type);
//			force_save_current_device(FALSE);
//
//		}
	}
	break;

	default:
		break;
	}

	return 0;
}

void task_conn_update_req(u8 e, u8 *p, int n)
{
#if 0

	conn_params_pending=1;

	u16 interval = p[3] | p[4]<<8;
    u16 latency = p[5] | p[6]<<8;

    if((interval>=ConParams[conn_params_cout].Min)&&(interval<=ConParams[conn_params_cout].Max) && latency > 0){
    	conn_params_cout=0xff;

    }
	else
	{
		conn_params_cout++;
	}

#endif
}
#if 0

void	task_conn_update_done (u8 e, u8 *p, int n)
{
	if (ble_status_aaa == T5S_CONNECTED_STATUS_AAA) // for windows
    {
        if ((conn_params_cout == 0) && (bls_ll_getConnectionInterval() > (ConParams[0].Min+4)))
        {
            if (deep_flag != CONN_PARAM_FAIL_REBOOT_ANA_AA)
            {
                bls_ll_terminateConnection(HCI_ERR_REMOTE_USER_TERM_CONN);
                ble_status_aaa = CONN_PARAM_FAIL_TERMINATE_STATUS_AAA;
            }
        }
    }
	
}
#endif

_attribute_ram_code_ void ble_remote_set_sleep_wakeup(u8 e, u8 *p, int n)
{
#if 1
	if (suspend_wake_up_enable && (blc_ll_getCurrentState() == BLS_LINK_STATE_CONN) && (((u32)(bls_pm_getSystemWakeupTick() - clock_time())) > 10 * CLOCK_16M_SYS_TIMER_CLK_1MS))
	{										   //suspend time > 30ms.add gpio wakeup
		bls_pm_setWakeupSource(PM_WAKEUP_PAD); //gpio pad wakeup suspend/deepsleep
		wheel_set_wakeup_level(1);
	}
#endif
}

static u8 update_param_counter = 0;
int app_conn_param_update_response(u8 id, u16  result)
{
//	printf("RES[0/1]:%d\n",result);
	if(result == CONN_PARAM_UPDATE_ACCEPT)
	{
		update_param_counter = 0;
		connect_reject_timer_tick = 0;
	}
	else if(result == CONN_PARAM_UPDATE_REJECT)
	{
		if(update_param_counter < 16)
		{
			connect_reject_timer_tick = clock_time() | 1;
			update_param_counter ++;
		}
	}
	return 0;
}

int att_sig_proc_handler(u16 connHandle, u8 *p)
{
#if 0 //SIG_PROC_ENABLE

	rf_pkt_l2cap_sig_connParaUpRsp_t* pp = (rf_pkt_l2cap_sig_connParaUpRsp_t*)p;

#if 0 //test debug
	foreach(i, 16){PrintHex(*((u8*)pp + i));}printf(".\n");
#endif

	u8 sig_conn_param_update_rsp[9] = { 0x0A, 0x06, 0x00, 0x05, 0x00, 0x13, 0x01, 0x02, 0x00 };
	if(!memcmp(sig_conn_param_update_rsp, &pp->rf_len, 9) && ((pp->type&0b11) == 2)){//l2cap data pkt, start pkt
		if(pp->result == 0x0000)
		{
#if PRINT_DEBUG_INFO
			printf("Host has accepted conn parameters\r\n");

#endif		
			conn_params_pending = 0;
			conn_params_cout=0xff;
		}
		else if(pp->result == 0x0001)
		{
			conn_params_pending = 0;
			conn_params_cout++;
#if PRINT_DEBUG_INFO

			printf("Host has rejected conn parameters\r\n");
#endif
		}
	}
#endif
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
	wd_stop();
	if (result == OTA_SUCCESS)
	{					   //OTA success
		sleep_us(2000000); //led on for 2 second
	}
	else
	{ //OTA fail
		for (int i = 0; i < 4; i++)
		{
			sleep_us(250000);
			sleep_us(250000);
		}
	}
}
#endif

static u32 low_power_begin_tick;
static u32 low_power_pc_count = 0;
static void low_power_count_reset()
{
	low_power_begin_tick = clock_time() | 1;
	low_power_pc_count = 0;
}

static void battery_ultra_low_enter_deepsleep()
{
	if((clock_time() - low_power_begin_tick) /CLOCK_16M_SYS_TIMER_CLK_1S)
	{
		low_power_begin_tick = clock_time() | 1;
		low_power_pc_count ++;
	}
	if (low_power_pc_count > LOW_POWER_TIME_S-1)
	{
		// 睡眠之前抬起按键
		ble_send_key_up();
		// 关闭电源指示灯状态灯光
//		RGB_pull_out_light();
//		gpio_write(PIN_BAT_LED,1);
//		// 提前1s关机，避免从机Flash写入失败
//		sonix_keyboard_instance()->set_rgb_param(SET_LEDS_BRIGHTNESS,0, FALSE);
	}
	if(low_power_pc_count>LOW_POWER_TIME_S)
	{
		enter_deep_aaa(0);
	}
}

/**
 * batter state change will call this function
 * @ state  正常：0， 低电：1， 超低电：2 低电量通知:3
 */
u8 lowbatt_flag = 0;  // ==0正常电量/==1低电量/==2超低电量
#if (BATT_ENABLE_AAA)
static void _battery_status_change_callback(u8 status)
{
	//	printf("sss: %d %d\r\n",status,lowbatt_flag);

	if (status == 0) //正常
	{
		low_power_count_reset();
	}
	else if (status == 1) //低电
	{
		if (!charging_flag) //（充满或者未充电）
		{
			battery_set_detect_enable(0);
			gpio_set_output_en(PIN_BAT_LED, 1);
			lowbatt_flag=1;
			low_power_count_reset();
		}
	}
	else if (status == 2) //超低电量
	{
		if (!charging_flag) //（充满或者未充电）
		{
			battery_set_detect_enable(0);
			gpio_set_output_en(PIN_BAT_LED, 1);
			lowbatt_flag=2;
		}
	}
	else if (status == 3) //低电量通知
	{
		// led_charger_set(1);
		// send lower power notify
		if (connect_ok && (!charging_flag)){
			get_logic_protocol_manager()->send_notify_data(get_protocol_manager_instance()->event_pack(TYPE_BATTERY, batt_vol_mv_real));
		}
	}
}
#endif

void low_batt_display()
{
#if BATT_ENABLE_AAA
#if (BLT_APP_LED_ENABLE )
	led_low_battery_display(lowbatt_flag);
#endif
	if(lowbatt_flag==2)  //超低电量
	{
		battery_ultra_low_enter_deepsleep();
	}
#endif
}

void check_charging_status()
{
	if(battery_is_charging())//充电器插入
	{
		if(charging_flag == 0)
		{
			get_logic_protocol_manager()->send_notify_data(get_protocol_manager_instance()->event_pack(TYPE_BATTERY, 5000));
			charging_flag = 1;
		}
//		lowbatt_flag = 0; //////////////////////////////////////do not enter low power reminder when charging

		if(battery_is_full())
		{
//				filled_tick = clock_time();
			RGB_fully_charged_batt_light();// 充满电
//			printf("BAT:1\n");
		}
		else
		{
			RGB_charging_light();// 正在充电
//			printf("BAT:0\n");
		}
		if(lowbatt_flag){
			lowbatt_flag = 0; //充电时不进入低电量提醒
			low_power_count_reset();
		}
	}
	else //充电器未插入
	{
		if (charging_flag){//拔出充电器
			charging_flag = 0; ////////////////////////////////Allow entering low power RGB_red flicker
			reset_battery_detect_time();
			battery_manager_check_proc();
			if(!ui_mic_enable)// 若不在语音状态，立即关闭灯光
			{
				RGB_pull_out_light();// 拔出充电器灯光状态（关闭）
			}
			get_logic_protocol_manager()->send_notify_data(get_protocol_manager_instance()->event_pack(TYPE_BATTERY, batt_vol_mv_real));
		}

	}
}

// 键盘扫描回调函数，扫描结果PUT fifo
void _keyboard_scan_cb_func(u8 *data, u8 size)
{
//	printf("scan:");
//	for(int i = 0; i< size; i++)
//	{
//		printf(" %x ", *(data +i));
//	}
//	printf("\n");
	u32 ret = scan_data_fifo.put(&scan_data_fifo, data, size);
	if(ret == MBLUE_ERROR){
		scan_data_fifo.reset(&scan_data_fifo);
		scan_data_fifo.put(&scan_data_fifo, data, size);
	}

}

static u8 sleepLock = 1;		// 睡眠锁，灯光熄灭时才起作用，禁止进入suspend
static u8 keyboard_up_flag = 1;	// 标识按键是否全部抬起
static u32 keybord_up_tick = 0;	// 计时从机进入sleep
void set_keyboard_up(u8 state)
{
#if BLE_APP_PM_ENABLE
	sleepLock = 1;// 有按键事件就变化
	if(state)
	{
		keybord_up_tick = clock_time();
		if(!keyboard_up_flag)
			keyboard_up_flag = 1;
	}
	else if(keyboard_up_flag)
		keyboard_up_flag = 0;
#endif

}

void user_init_normal(void)
{
	//blc_app_loadCustomizedParameters();  //load customized freq_offset cap value

	vMblueIdentificationInit();

	load_paired_device();

	mblue_fifo_construct(&scan_data_fifo, fifo_buffer, 128);

	scan_manager_instance()->init(_keyboard_scan_cb_func, set_keyboard_up);

	data_out_fifo_init(ble_notify_data_proc);

#if (ENCODER_TYPE == 1)
	codec_init();
#endif
	random_generator_init(); //this is must

////////////////// BLE stack initialization ////////////////////////////////////
#if BLE_SNIFF_DEBUG
	u8 tbl_mac[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0xc7};
	extern u8 blc_adv_channel[];
	//_attribute_data_retention_	u8		blc_adv_channel[3] = {37, 38, 39};
	blc_adv_channel[0] = 38;
	blc_adv_channel[1] = 38;
	blc_adv_channel[2] = 38;

#endif
	memcpy(keyboard_mac_address, (u8 *)&user_cfg.dev_mac, 6);
	keyboard_mac_address[4] = MACADDR_CHANNEL;
	keyboard_mac_address[5] = MACADDR_AUTH_TYPE;

	////// Controller Initialization  //////////
	blc_ll_initBasicMCU(); //mandatory

	// get current device param
	set_last_device_adv();
	blc_ll_initConnection_module(); //connection module  mandatory for BLE slave/master
	blc_ll_initSlaveRole_module();	//slave module: 	 mandatory for BLE slave,

	blc_ll_initPowerManagement_module(); //pm module:      	 optional

	////// Host Initialization  //////////
	blc_gap_peripheral_init(); //gap initialization
	extern void my_att_init();
	my_att_init();										  //gatt initialization
	blc_l2cap_register_handler(blc_l2cap_packet_receive); //l2cap initialization

	//// smp initialization ////
	//#if (BLE_REMOTE_SECURITY_ENABLE == 1)
	blc_smp_peripheral_init();

	//bls_smp_configParingSecurityInfoStorageAddr(BONDING_FLASH_ADDR);
	// set bonding device size
	blc_smp_param_setBondingDeviceMaxNumber(MAX_BONDING_DEVICE_NUM);

	//Hid device on android7.0/7.1 or later version
	// New paring: send security_request immediately after connection complete
	// reConnect:  send security_request 1000mS after connection complete. If master start paring or encryption before 1000mS timeout, slave do not send security_request.
	blc_smp_configSecurityRequestSending(SecReq_IMM_SEND, SecReq_PEND_SEND, 1000); //if not set, default is:  send "security request" immediately after link layer connection established(regardless of new connection or reconnection )

	blc_gap_registerHostEventHandler(app_host_event_callback);
	blc_gap_setEventMask(GAP_EVT_MASK_SMP_PARING_FAIL |
						 GAP_EVT_MASK_SMP_PARING_SUCCESS |
						 GAP_EVT_MASK_ATT_EXCHANGE_MTU |
						 GAP_EVT_MASK_SMP_CONN_ENCRYPTION_DONE);


#if SIG_PROC_ENABLE
	blc_l2cap_reg_att_sig_hander(att_sig_proc_handler); //register sig process handler
#endif
	blc_l2cap_registerConnUpdateRspCb(app_conn_param_update_response);
	///////////////////// USER application initialization ///////////////////

	//set rf power index, user must set it after every suspend wakeup, cause relative setting will be reset in suspend
	user_set_rf_power(0, 0, 0);
	bls_app_registerEventCallback(BLT_EV_FLAG_SUSPEND_EXIT, &user_set_rf_power);

	//ble event call back
	bls_app_registerEventCallback(BLT_EV_FLAG_CONNECT, &task_connect);
	bls_app_registerEventCallback(BLT_EV_FLAG_TERMINATE, &task_terminate);
	bls_app_registerEventCallback(BLT_EV_FLAG_ADV_DURATION_TIMEOUT, &app_switch_to_indirect_adv);
	bls_app_registerEventCallback(BLT_EV_FLAG_GPIO_EARLY_WAKEUP, &task_gpio_eary_wakeup);
	bls_app_registerEventCallback(BLT_EV_FLAG_CONN_PARA_UPDATE, &conn_param_update_callback);
	bls_app_registerEventCallback(BLT_EV_FLAG_CONN_PARA_REQ, &conn_param_request_callback);

	///////////////////// Power Management initialization///////////////////
#if (BLE_APP_PM_ENABLE)
	blc_ll_initPowerManagement_module();

#if (PM_DEEPSLEEP_RETENTION_ENABLE)
	bls_pm_setSuspendMask(SUSPEND_ADV | DEEPSLEEP_RETENTION_ADV | SUSPEND_CONN | DEEPSLEEP_RETENTION_CONN);
	blc_pm_setDeepsleepRetentionThreshold(95, 95);
	blc_pm_setDeepsleepRetentionEarlyWakeupTiming(400);
	blc_pm_setDeepsleepRetentionType(DEEPSLEEP_MODE_RET_SRAM_LOW32K);
#else
	bls_pm_setSuspendMask(SUSPEND_CONN);
#endif

	bls_app_registerEventCallback(BLT_EV_FLAG_SUSPEND_ENTER, &ble_remote_set_sleep_wakeup);
#else
	bls_pm_setSuspendMask(SUSPEND_DISABLE);
#endif
	//battery check must do before OTA relative operation
#if (BATT_ENABLE_AAA)
	//user_batt_check_init();
	battery_manager_init(250000000, _battery_status_change_callback);
#endif

#if (OTA_ENABLE_AAA)
	////////////////// OTA relative ////////////////////////
	bls_ota_clearNewFwDataArea(); //must
	bls_ota_registerStartCmdCb(app_enter_ota_mode);
	bls_ota_registerResultIndicateCb(app_debug_ota_result); //debug
#endif

//	sonix_keyboard_instance()->init();// scanning SPI initialization
	web_data_init();
	web_data_nkro_init();
	start_tick = clock_time();
	web_flag = analog_read(DEEP_ANA_REG9); //web
}

_attribute_ram_code_ void user_init_deepRetn(void)
{
#if (PM_DEEPSLEEP_RETENTION_ENABLE)

	blc_ll_initBasicMCU(); //mandatory
	rf_set_power_level_index((user_cfg.tx_power));

	blc_ll_recoverDeepRetention();

	DBG_CHN0_HIGH; //debug

	irq_enable();

	//pm mask disable,in main_loop according to ui task will reset pm mask
	bls_pm_setSuspendMask(SUSPEND_DISABLE);

	//app_ui_init_deepRetn();
#endif
}

void enter_deep_aaa(u8 en )  //en表示有无GPIO唤醒
{
	write_deep_ana0(DEEP_SLEEP_ANA_AAA);

	u8 set_sonix_sleep = 0;
	while(!sonix_keyboard_instance()->set_mcu_mode(MCU_SLEEP) && set_sonix_sleep<5)
	{
		set_sonix_sleep++;
		sleep_ms(2);
	}

	scan_manager_instance()->set_wakeup(1);
	wheel_set_wakeup_level(en);
	S1_set_wakeup_level(en);
	cpu_set_gpio_wakeup(GPIO_BATT_CHECK, 1, 1);
	cpu_set_gpio_wakeup(PIN_ON_OFF_STATE, 1, 1);
	clear_pair_flag();
	//ble_status_aaa=0;
	cpu_sleep_wakeup(DEEPSLEEP_MODE, PM_WAKEUP_PAD, 0);
}
#if CUST_SAVE_MASTER_INFO_ENABLE_AAA
_attribute_data_retention_user u8 directed_adv_flag = 0;
_attribute_data_retention_user u8 ble_adv_channel = 1;

void set_adv_type()
{
	//bls_ll_setAdvEnable(0);
	led_caps_set(0); // Turn off caps lock lights during broadcast.
	directed_adv_flag=0;
	pair_device_info_t device = get_current_adv_device();
	ble_adv_channel=device.device_index;

	if(device.adv_type == BLE_ADDR_RANDOM || device.adv_type == BLE_ADDR_INVALID){
		bls_ll_setAdvDuration(0, 0);
		bls_ll_setAdvParam(MY_ADV_INTERVAL_MIN, MY_ADV_INTERVAL_MAX,
										   ADV_TYPE_CONNECTABLE_UNDIRECTED, USER_OWN_ADDRESS_TYPE,
										   0, NULL,
										   MY_APP_ADV_CHANNEL,
										   ADV_FP_NONE);
	}else{
		if (device.valid_flag == 1)
		{
			directed_adv_flag=1;
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
	bls_ll_setAdvEnable(1);
}


#else
void set_adv_type()
{
	u8 bond_number = blc_smp_param_getCurrentBondingDeviceNumber(); //get bonded device number
	if (bond_number && (pair_flag == 0))							//at least 1 bonding device exist
	{
		smp_param_save_t bondInfo;
		bls_smp_param_loadByIndex(bond_number - 1, &bondInfo);

		if (((bondInfo.peer_addr_type & 0x01) == BLE_ADDR_PUBLIC))
		{
			memcpy(bonded_peer_addr, bondInfo.peer_addr, 6);

			bls_ll_setAdvParam(ADV_INTERVAL_3_75MS, ADV_INTERVAL_3_75MS,
							   ADV_TYPE_CONNECTABLE_DIRECTED_LOW_DUTY, USER_OWN_ADDRESS_TYPE,
							   BLE_ADDR_PUBLIC, bonded_peer_addr,
							   BLT_ENABLE_ADV_ALL,
							   ADV_FP_NONE);
			bls_ll_setAdvDuration(2000000, DIRECT_ADV_TO_UNDIRECT_ENABLE);
		}
		else
		{
			tbl_advData_aaa[2] = 0x04;
			bls_ll_setAdvDuration(0, 0);
			bls_ll_setAdvParam(MY_ADV_INTERVAL_MIN, MY_ADV_INTERVAL_MAX,
							   ADV_TYPE_CONNECTABLE_UNDIRECTED, USER_OWN_ADDRESS_TYPE,
							   0, NULL,
							   MY_APP_ADV_CHANNEL,
							   ADV_FP_NONE);
		}
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
#endif

u8 adving_flag = 0;					//充电状态下。30S广播后无响应，不进入休眠，但是蓝牙灯会熄灭，再次按键响应后蓝牙灯重新闪烁30S
static void device_adv_state_proc() //广播30秒后无应答休眠
{

	if (connect_count)
		return;
	adv_count_poll();
	//	printf("ADV: %d %d\r\n",adv_count,adving_flag);
	if (adv_count < BLE_ADV_TIMER_OUT)   //（广播时间小于30s）
	{
		led_ble_mode_display();
	}
	else
	{
		adving_flag = 1;
		if(!charging_flag)  // 充电器没有插入
			ble_status_aaa = DEEP_SLEEPE_STATUS_AAA;
		else bls_ll_setAdvEnable(0);  //停止广播
	}
}

static u8 ble_stay_flag = 0;
//static u8 BLE_LED_FLAG = 0;
_attribute_data_retention_user u32 ble_tick;
void ble_status_proc_aaa(u8 is_new_key_event)
{
	if (ble_status_aaa == T5S_CONNECTED_STATUS_AAA)
	{
		connect_count = 1;
#if (BLE_CONNECT_ENTER_DEEPSLEEP_AFTER_TIME_OUT_ENABLE_AAA && (!BLE_SNIFF_DEBUG))
#if (BATT_ENABLE_AAA)
		if ((idle_count >= flash_keyboard_param.sleep_time) && (!charging_flag) && (ui_mic_enable == 0) && flash_keyboard_param.sleep_time > 0)
		{
			bls_ll_terminateConnection(HCI_ERR_REMOTE_USER_TERM_CONN);
			ble_status_aaa = DEEP_TERMINATE_STATUS_AAA;
		}
#endif
#endif
#if BLT_SOFTWARE_TIMER_ENABLE

		check_connect_interval();
#endif
	//	connect_params_proc();
	}
	else if (ble_status_aaa == POWER_ON_STATUS_AAA)
	{
		//bls_smp_enableParing (SMP_PARING_DISABLE_TRRIGER);
		ble_stay_flag = 0;
		//BLE_LED_FLAG = 0;
		connect_count = 0;
		// bls_ll_setAdvEnable(0);

#if (BLE_SNIFF_DEBUG)
		bls_ll_setAdvDuration(0, 0);
		bls_ll_setAdvParam(MY_ADV_INTERVAL_MIN, MY_ADV_INTERVAL_MAX,
						   ADV_TYPE_CONNECTABLE_UNDIRECTED, USER_OWN_ADDRESS_TYPE,
						   0, NULL,
						   MY_APP_ADV_CHANNEL,
						   ADV_FP_NONE);
#else
		Switch_Adv_Type = 0;
		set_adv_type();
#endif

		//	set_adv_scanRsp_data();
		// bls_ll_setAdvEnable(1); //adv enable
		// reset_idle_status();
		adv_begin_tick = clock_time() | 1;
		adv_count = 0;
		ble_status_aaa = LOW_ADV_STATUS_AAA;
	}
	else if (ble_status_aaa == LOW_ADV_STATUS_AAA)
	{
		if (!adving_flag)
			device_adv_state_proc();
	}
	else if (ble_status_aaa == BEGIN_CONNECTED_STATUS_AAA)
	{
		connect_count = 1;
		if(conn_step == FIRST_PAIR_DONE_AAA)
		{
			if (clock_time_exceed(connect_begin_tick, 100000))
			{
				ble_status_aaa = OK_CONNECTED_STATUS_AAA;
			}
		}
		else if(conn_step == RECONN_DONE_AAA)
		{
			ble_status_aaa = OK_CONNECTED_STATUS_AAA;
		}
	}
	else if (ble_status_aaa == OK_CONNECTED_STATUS_AAA)
	{
		clear_data_out_fifo();
		get_logic_protocol_manager()->clear_cache();
		ble_status_aaa = T5S_CONNECTED_STATUS_AAA;
		connect_params_proc();
#if BLE_WEB_ENABLE
		pair_device_info_t device = get_current_adv_device();
		if ((device.host_mac[4] == 0x49) && (device.host_mac[5] == 0x4d))
		{
			dongle_web_flag = 1;
		}
		else
		{
			dongle_web_flag = 0;
		}

#endif
		led_ble_connected_display();
	}
	else if (ble_status_aaa == DEEP_SLEEPE_STATUS_AAA)
	{
		if ((ui_ota_is_working == 0) && (!blc_ll_isControllerEventPending()))
		{
			enter_deep_aaa(1);
		}
	}

}
void reset_idle_status()
{
	if (pair_flag)
	{
		return;
	}
	if(adving_flag)
	{
		adving_flag=0; //重新进入广播
		ble_status_aaa = POWER_ON_STATUS_AAA;
	}

	idle_count = 0;
	loop_cnt = 0;
	idle_tick = clock_time();
	adv_begin_tick = idle_tick | 1;
	adv_count = 0;
}

void idle_status_poll()
{
	u8 n;
	n = ((u32)(clock_time() - idle_tick)) / CLOCK_SYS_CLOCK_1S;

	idle_tick += n * CLOCK_SYS_CLOCK_1S;

	idle_count += n;
}
void adv_count_poll()
{
	u8 n;
	n = ((u32)(clock_time() - adv_begin_tick)) / CLOCK_SYS_CLOCK_1S;

	adv_begin_tick += n * CLOCK_SYS_CLOCK_1S;

	adv_count += n;
}

void ble_pm_aaa()
{
	if (sleepLock && keyboard_up_flag && clock_time_exceed(keybord_up_tick, 5*1000*1000))// 5s没有按键按下
	{
#if MCU_NORMAL_SLEEP
		if(		sonix_keyboard_instance()->kleds.mode==0 ||
				sonix_keyboard_instance()->kleds.mode==3 ||
				sonix_keyboard_instance()->kleds.mode==4 ||
				sonix_keyboard_instance()->kleds.mode==5 ||
				sonix_keyboard_instance()->kleds.brightness==0
		)// 如果亮度为0或模式为0,3,4,5
		{
			if (connect_ok && ((ble_status_aaa == T5S_CONNECTED_STATUS_AAA) || (ble_status_aaa == OK_CONNECTED_STATUS_AAA)))
			{
				if(sonix_keyboard_instance()->set_mcu_mode(MCU_SLEEP))
					sleepLock = 0;
			}
		}
#endif
	}
#if (BLE_AUDIO_ENABLE1)
	if (ui_mic_enable || charging_flag || protocol_send_flag || lowbatt_flag)//|| !sleepLock)
	{
		bls_pm_setSuspendMask(SUSPEND_DISABLE);
		return;
	}
#endif

#if BLE_APP_PM_ENABLE
	if (has_web_key_flag || ui_ota_is_working || get_ble_led_condition() ||(loop_cnt < LONG_SUSPEND_TIMER_AAA))
	{
		bls_pm_setManualLatency(0);
		if (suspend_wake_up_enable)
		{
			wheel_set_wakeup_level(0);
			scan_manager_instance()->set_wakeup(0);
		}
		suspend_wake_up_enable = 0;
	}
	else
	{
		//bls_pm_setWakeupSource(PM_WAKEUP_CORE);
		loop_cnt = LONG_SUSPEND_TIMER_AAA + 1;
		if (suspend_wake_up_enable == 0)
		{
			wheel_set_wakeup_level(1);
			scan_manager_instance()->set_wakeup(1);
		}
		suspend_wake_up_enable = 1;
		bls_pm_setWakeupSource(PM_WAKEUP_PAD);
	}

#if (PM_DEEPSLEEP_RETENTION_ENABLE)
	if (suspend_wake_up_enable && (idle_count > 3))
	{
		bls_pm_setSuspendMask(SUSPEND_ADV | DEEPSLEEP_RETENTION_ADV | SUSPEND_CONN | DEEPSLEEP_RETENTION_CONN);
	}
	else
	{
		bls_pm_setSuspendMask(SUSPEND_ADV | SUSPEND_CONN);
	}
#else
	bls_pm_setSuspendMask(SUSPEND_CONN);
#endif
	// bls_pm_setManualLatency(0);
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
	if (protocol_stop_record_cmd == STOP_RECORD)
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

// main loop flow
/////////////////////////////////////////////////////////////////////
static u8 blemode_to_usbmode_flag = 0;
void ui_loop()
{

#if (BLE_AUDIO_ENABLE1)
	dealwith_protocol_cmd();
#endif
	u8 is_new_event = 0;

#if (BLE_AUDIO_ENABLE1)
	user_requestMtuSizeExchange();
	if (ui_mic_enable)
	{
		audio_dispatch();
	}
	check_charging_status();
#endif

	scan_manager_instance()->loop_scan();

#if (BATT_CHECK_ENABLE && BATT_ENABLE_AAA)
	// check Battery capacity
	battery_manager_check_proc();
//	{
//		if(send_battery_value_flag)
//		{
//			get_logic_protocol_manager()->send_notify_data(get_protocol_manager_instance()->event_pack(TYPE_BATTERY, batt_vol_mv_real));
//			send_battery_value_flag = 0;
//		}
//	}

#endif
#if BLT_APP_LED_ENABLE
	led_hw_proc();
#endif
	is_new_event = keyboard_manager_proc();
	whlee_monitor_pro();
	loop_cnt++;
	if (is_new_event)
	{
		reset_idle_status();
	}

	idle_status_poll();
	// 蓝牙状态机切换
	ble_status_proc_aaa(is_new_event);

	if (connect_ok && ((ble_status_aaa == T5S_CONNECTED_STATUS_AAA) || (ble_status_aaa == OK_CONNECTED_STATUS_AAA)))
	{
		if((connect_reject_timer_tick > 0) && clock_time_exceed(connect_reject_timer_tick, 300000)){
			connect_reject_timer_tick = 0;
//			printf("CP\n");
			connect_params_proc();
		}
		data_out_loop_proc();// 数据输出
	}
#if (BATT_ENABLE_AAA)

	low_batt_display();
#endif
#if MODULE_USB_ENABLE
	if(charging_flag)//正在充电
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

#if BLE_WEB_ENABLE
	if(has_web_key_flag)
	{
		if(dongle_web_flag)
		{
			u32 product_no_32;
			memcpy(&product_no_32, device_product_no, 4);
			if(get_logic_protocol_manager()->send_notify_data(get_protocol_manager_instance()->event_pack(TYPE_WEB, product_no_32)) == BLE_SUCCESS)
				has_web_key_flag = 0;
		}
		else
		{
			if(keyboard_data_mode == NKRO)
			{
				open_website_nkro();
			}
			else if(keyboard_data_mode == SIXKRO)
			{
				open_website();
			}
		}
	}
#endif
#if SINCHECK_PCM_DATA_ENABLE
    sincheck_loop();
#endif
#if PROTOCOL_OTA_ENABLE
	ota_loop_proc();
#endif
}

void main_loop(void)
{
	////////////////////////////////////// BLE entry /////////////////////////////////
	blt_sdk_main_loop();

	////////////////////////////////////// UI entry /////////////////////////////////
	if(sonix_irq_buf)
	{
		sonix_irq_buf = 0;
		sonix_keyboard_instance()->sonix_data_scan();// scanning SPI initialization
	}
	if (clock_time_exceed(start_tick, 5000))
	{
		start_tick = clock_time();
		ui_loop();
	}

	////////////////////////////////////// PM Process /////////////////////////////////
	ble_pm_aaa();
}
