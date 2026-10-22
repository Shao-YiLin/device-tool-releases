/********************************************************************************************************
 * @file     aaa_public_config.h
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

#ifndef _AAA_CONFIG_H_
#define _AAA_CONFIG_H_

#include "tl_common.h"
#include "drivers.h"
#include "stack/ble/ble.h"

#include "../../vendor/common/user_config.h"
#include "../../third/fifo/mblue_fifo.h"
#include "aaa_battery_check.h"
#include "aaa_public_KeyValue.h"
#include "pair_info_manager.h"
#include "data_out_manager.h"
#include "sonix_keyboard_manager.h"
#include "battery_manager.h"
#include "keyboard_scan.h"
#include "application/usbstd/usbkeycode.h"
#include "AAA_app_config_Telink_QFN32.h"


typedef struct
{
	/** Minimum value for the connection event (interval. 0x0006 - 0x0C80 * 1.25 ms) */
	u16 Min;
	/** Maximum value for the connection event (interval. 0x0006 - 0x0C80 * 1.25 ms) */
	u16 Max;
	/** Number of LL latency connection events (0x0000 - 0x03e8) */
	u16 latency;
	/** Connection Timeout (0x000A - 0x0C80 * 10 ms) */
	u16 timeout;
} gap_periConnectParams_t;

//--------------------function defein----------------------------------------

#define MS_BTN_LEFT 0X01
#define MS_BTN_RIGHT 0X02
#define MS_BTN_MIDDLE 0X04
#define MS_BTN_K4 0X08
#define MS_BTN_K5 0X10

#define MS_BTN_MODE 0X20
#define MS_BTN_PAIR 0X40
#define MS_BTN_CPI 0X80

//------------tx power define-------------------
#define DEFAULT_NORMAL_TX_POWER RF_POWER_P0p04dBm
#define DEFAULT_PAIR_TX_POWER RF_POWER_P0p04dBm
#define DEFAULT_EMI_TX_POWER RF_POWER_P0p04dBm

#define ANALOG_READ(reg,x) (analog_read(reg)&BIT(x)?1:0)
#define ANALOG_WRITE(reg,value,x) ((value==1)?analog_write(reg,analog_read(reg)|BIT(x)):analog_write(reg,analog_read(reg)&(~BIT(x))))

typedef struct
{
	u32 dev_mac;		//00
	u16 group_id;		//04~05
	u16 rf_vid;			//06~07
	u8 bat_type;		//008
	u8 tx_power;		//09
	u8 paring_tx_power; //0a
	u8 emi_tx_power;	//00b
	u8 wheel_direct;	//0c
	u8 internal_cap;	//0d
	u8 sensor_direct;	//0e
	u8 aes_enable;		//0f
	u8 pub_key[16];		//10~1f
	u8 device_name[18]; //20~31

} custom_cfg_t;
_attribute_data_retention_user extern custom_cfg_t user_cfg;
_attribute_data_retention_user extern u8 device_name_len;
 _attribute_data_retention_user extern u8 tbl_advData_aaa[];


_attribute_data_retention_user extern u8 connect_ok;
_attribute_data_retention_user extern u16 batt_vol_mv_real;
_attribute_data_retention_user extern u8 keyboard_mac_address[];
_attribute_data_retention_user extern u8 keyboard_version[];
_attribute_data_retention_user extern u8 protocol_start_record_cmd;
_attribute_data_retention_user extern u8 protocol_stop_record_cmd;
extern u8 device_product_no[4];
extern u32 ifly_android_flag;
extern u8 charging_flag;
extern u8 USB_INSERT_FLAG;
extern u8 usb_has_suspend_irq;
u8 fifo_buffer[128];
struct mblue_fifo scan_data_fifo;

extern u8 sonix_irq_buf;
extern u8 sonix_irq_enable;

extern const u8 nkroReportMap[];
extern const u8 sixkroReportMap[];
extern int nkroReportMap_size;
extern int sixkroReportMap_size;

extern const attribute_t my_Attributes[];
extern const attribute_t my_Attributes_nkro[] ;

_attribute_data_retention_ extern u32 record_channel;
#define AUTHENTICATION_FLAG (1)
typedef enum
{
	POWER_ON_ANA_AAA = 0,
	DEEP_SLEEP_ANA_AAA = 1,
	MODE_CHANGE_REBOOT_ANA_AAA = 2,
	BLE_PAIR_REBOOT_ANA_AAA = 3,
	CONN_PARAM_FAIL_REBOOT_ANA_AA = 4,
	CLEAR_FLAG_ANA_AAA = 5,
} ANA_STATUS_AAA;

typedef enum
{
	POWER_ON_STATUS_AAA = 0,
	HIGH_ADV_STATUS_AAA = 1,
	LOW_ADV_STATUS_AAA = 2,
	BEGIN_CONNECTED_STATUS_AAA = 3,
	OK_CONNECTED_STATUS_AAA = 4,
	T5S_CONNECTED_STATUS_AAA = 5,
	DEEP_SLEEPE_STATUS_AAA = 6,
	DEEP_TERMINATE_STATUS_AAA = 7,
	MODE_CHANGE_TERMINATE_STATUS_AAA = 8,
	CONN_PARAM_FAIL_TERMINATE_STATUS_AAA = 9,
	MUTI_DEVICE_CHANGE_STATUS_AAA = 10,
} BLE_STATUS_AAA;

typedef enum
{
	KEY_PRESS_EVENT_AAA = BIT(0),
	NEW_KEY_EVENT_AAA = BIT(1),
	SENSOR_DATA_EVENT_AAA = BIT(2),
	WHEEL_DATA_EVENT_AAA = BIT(3),
	SPP_KEY_EVENT_AAA = BIT(4),
} key_EVENT_AAA;
typedef enum
{
	UNKNOW_OS_TYPE = 0,
	APPLE_OS_TYPE = 1,
	ANDROID_OS_TYPE = 2,
	WINDOWS_OS_TYPE = 3,
} PC_OS_TYPE_AAA;

//typedef enum
//{
//	HAS_MOUSE_REPORT = BIT(0),
//	HAS_KEYBOARD_REPORT = BIT(1),
//	HAS_CONSUMER_REPORT = BIT(2),
//	HAS_SYSYTEM_REPORT = BIT(3),
//	HAS_SPP_REPORT = BIT(4),
//	HAS_PROT_REPORT = BIT(5),
//} HAS_REPORT_TYPE_AAA;
typedef enum
{
	FN_PRESS_AAA = BIT(0),
	T_BIND_PRESS_AAA = BIT(1),
	MOUSE_KEY_PRESS_AAA = BIT(2),
	T_ESC_PRESS_AAA = BIT(3),
	T_Q_PRESS_AAA = BIT(4),
	T_W_PRESS_AAA = BIT(5),
	T_E_PRESS_AAA = BIT(6),
	T_R_PRESS_AAA = BIT(7),
	T_T_PRESS_AAA = BIT(8),
	T_Y_PRESS_AAA = BIT(9),
	T_U_PRESS_AAA = BIT(10),
	T_I_PRESS_AAA = BIT(11),
	T_O_PRESS_AAA = BIT(12),
	T_F1_PRESS_AAA = BIT(13),
	T_SPACE_PRESS_AAA = BIT(14),
} SPECIAL_KEY_PRESS_FLAG_AAA;

typedef enum
{
	MULTIPIPE_1 = 0,
	MULTIPIPE_1_DOT_5 = 1,
	MULTIPIPE_2_DOT_0 = 2,
	MULTIPIPE_2_DOT_5 = 3,
	MULTIPIPE_3_DOT_0 = 4,
} SENSOR_MULTIPIPE_XY;

typedef enum
{
	SIXKRO = 0,
	NKRO = 1,
} CONFLICT_NUM;

enum
{
	AUDIO_MANAGER = 0,
	BLIGHT_MANAGER = 1,
};

#define BUF_SIZE_KEYBOARD_AAA 8
#if CONSUMER_REPORT_BYTE_AAA

#define BUF_SIZE_CONSUMER_AAA 1
#else
#define BUF_SIZE_CONSUMER_AAA 4
#endif



typedef struct
{
	u32 dongle_id;
	u8 key[12]; //12
	u8 slave_mac_addr[4];
	u8 mode;
	u8 mast_id;
	u16 temp1;	 //24
	u8 temp2[8]; //32
} FLASH_DEV_INFO_AAA;

typedef struct
{
    u8 numberKro;
    u8 wheel_func;
    u32 sleep_time;
    s16 idx;
} FLASH_KEYBOARD_INFO;

_attribute_data_retention_user extern FLASH_KEYBOARD_INFO flash_keyboard_param;

typedef struct
{
	u8 ctrl_key;
	u8 keycode;
	u8 next_interval; //uint 10
} y_kbdata_t;

_attribute_data_retention_user extern u8 directed_adv_flag;
_attribute_data_retention_user extern u8 binding_master_addr[];
_attribute_data_retention_user extern u8 ble_adv_channel;
_attribute_data_retention_user extern FLASH_DEV_INFO_AAA flash_dev_info;
_attribute_data_retention_user extern int dev_info_idx;

_attribute_data_retention_user extern u8 deep_flag;
_attribute_data_retention_user extern u8 pair_flag;

_attribute_data_retention_user extern u8 suspend_wake_up_enable;
extern u8 dongle_web_flag;

_attribute_data_retention_user extern u8 fun_mode;
_attribute_data_retention_user extern u8 keyboard_data_mode;
_attribute_data_retention_user extern u8 ble_status_aaa;
_attribute_data_retention_user extern u32 power_tick;

_attribute_ram_code_ s8 mouse_wheel_process();

u32 protocol_send_flag;
extern u8 caps_flag;
extern u8 host_keyboard_status;

// ==> config by andy.xu
_attribute_data_retention_user u8 ui_mic_enable;

extern void vMblueIdentificationInit();
extern u8 ucMblueGetFinger(unsigned char *finger);
extern u8 ucCheckIdentificationClientSalt(unsigned char *salt, unsigned int len);
extern void vSetIdentificationClientRandom(unsigned char *random, unsigned int len);
extern void vGetIdentificationSalt(unsigned char *salt);
extern void vSetIdentificationClientFinger(unsigned char *finger, unsigned int len);
extern void vGetIdentificationRandom(unsigned char *challenge);
extern void set_keyboard_up(u8 state);
//extern void vGetSessionKey(unsigned char *pcKey);
extern u32 device_protocol_version;

#if HAS_BACK_LIGHT_AAA
_attribute_data_retention_user extern u8 back_light_switch_flag;

#endif
#if BATT_CHECK_FUN_ENABLE
_attribute_data_retention_user extern u8 low_bat_count;
#endif


extern void user_init_normal();
extern void user_init_deepRetn();
extern void set_os_type(u8 os_type);
extern void main_loop(void);
extern void save_dev_info_flash();
extern void save_keyboard_info_flash();
extern int cccWrite(void *p);
extern void clear_pair_flag();
extern void set_pair_flag();
extern void irq_handle_usb_timer();
void usb_user_init();
void usb_mode_loop();
extern u8 keyboard_manager_proc();
extern void usb_handle_irq();
void dealwith_protocol_cmd();
void usb_init_interrupt();
extern void usbhw_disable_manual_interrupt(int);
u8 ble_notify_data_proc(u16 type, u8 *data, u32 size);
u8 usb_notify_data_proc(u16 type, u8 *data, u32 size);
void usb_resume_host();
void blemode_to_usbmode();
void whlee_monitor_pro();
extern u8 get_ble_led_condition();
extern u8 get_ble_led_state();

void wheel_set_wakeup_level(u8 enable);
void S1_set_wakeup_level(u8 enable);
void set_brightness(u8 mode);
void mode_set(u8 mode);

extern void _keyboard_scan_cb_func(u8 *data, u8 size);

extern void adv_count_poll();
//extern void ble_notify_data_proc_aaa();

// ==========sincheck================
extern void sincheck_loop();
extern void start_sincheck();
// ==========web manager==========
void web_data_init();
void web_data_nkro_init();
void open_website();
void open_website_nkro();

extern void Set_web_flag();

extern void enter_deep_aaa(u8);
extern void check_charging_status();

extern int flash_info_load_aaa(u32 s_addr, u8 *d_addr, u8 len);
extern void hw_init();
extern void led_hw_init();

extern u8 mode_keycode_flag; //1-windows;2-android;3-mac
extern u8 web_flag;  //长按语音键弹出网页参数（事件参数）
extern u8 has_web_key_flag;
extern void led_ble_mode_display();
extern void ui_loop();

extern void write_deep_ana0(u8 buf);
extern int protocolWrite(void *src);
extern int keyboardWrite(void *src);
extern void ble_send_key_up();
extern void set_last_device_adv();
extern void add_device_adv(u8 device_index);
extern pair_device_info_t get_current_adv_device();
extern void set_current_device_host_mac(u8 *mac, u8 size);
extern void set_current_device_os_type(u8 os_type);
extern void set_current_device_adv_type(u8 adv_type);
extern void force_save_current_device(bool);
extern u8 save_current_device(u8 *slave_mac, u8 *master_mac, u8 adv_type, u8 os_type);
extern void load_paired_device();
extern unsigned char is_mac_address(unsigned char *addr);
extern void reset_idle_status();

//LED manager <AAA_LED.c>
void ble_indicator_light_on();
void ble_close_all_led();
void led_low_battery_display(u8 lowbatt_flag);//低电
void led_close_channel();
void led_choose_channel();
void led_caps_set(u8 status);
void led_scroll_set(u8 status);
void led_num_set(u8 status);
void led_ble_mode_display();
void led_com_mode_change();
void led_ble_first_connected_display();
void led_ble_connected_display();
void RGB_charging_light();// 正在充电
void RGB_fully_charged_batt_light();// 充满电
void RGB_pull_out_light();// 拔出充电器灯光状态（关闭）
void start_audio_light();
void stop_audio_light();
void led_hw_proc();

#define BATTER_MAX_LEVEL 4350      //满电电压
#define BATTER_MIN_LEVEL 3300		//5%电量电压
#define BATTER_LOW_VOL   3400      //3400mV低电压
#define BATTER_ULTRA_LOW_VOL  3100   //3100mV超低电压

#define DEVICE_CHANNEL_SIZE 3

enum device_channel_index{
	CHANNEL_1 = 1,
	CHANNEL_2 = 2,
	CHANNEL_3 = 3,
	CHANNEL_USB = 4,
};

#define BOUND_DONGLE_MAC	{0x0f, 0x49, 0x4d}

#define TIME_1_SECONDS 1000000
#define BLE_RESET_TIME_MS (3)
#define AUTO_DEEP_SLEEP_TIME (1800)	 //30MIN
//#define AUTO_DEEP_SLEEP_TIME (120)	 //15MIN
#define AUTO_SUSPEND_SLEEP_TIME (5) //enter suspend
#define BLE_ADV_TIMER_OUT 30
#define LOW_POWER_TIME_S  5

#define BONDING_FLASH_ADDR 0x74000

/**
 * 	madao channel
 * 	0x44: madao
 * 	0x49: 讯飞
 * 	0x4a: 中石油
 * 	0xff: 售后专用
 */
#define MACADDR_CHANNEL   0x49

/**
 * madao authentication type
 * 0xff: authenticate & offline esr
 * 0xfd: authenticate
 * 0xed: no
 **/
#define MACADDR_AUTH_TYPE  0xff

// max bonding device size for ble sdk
#define MAX_BONDING_DEVICE_NUM 64

#if Microsoft_Swift_Pairing_ENABLE
#define DEV_POSITION 17
#define USER_DEVICE_NAME_MAX_LEN 14
#else
#define DEV_POSITION 13
#define USER_DEVICE_NAME_MAX_LEN 18
#endif

#endif
