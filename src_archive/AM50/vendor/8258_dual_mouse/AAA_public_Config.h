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
#include "../../application/rf_frame.h"
#include "gpio_manager.h"
#include "../../vendor/common/user_config.h"
#include "aaa_battery_check.h"
#include "pair_info_manager.h"
#include "data_out_manager.h"


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

#define MS_BTN_LEFT 	0X01
#define MS_BTN_RIGHT 	0X02
#define MS_BTN_MIDDLE 	0X04
#define MS_BTN_K4 		0X08
#define MS_BTN_K5 		0X10

#define MS_BTN_MODE     0X20
#define MS_BTN_PAIR     0X40
#define MS_BTN_CPI 		0X80
#define MS_BTN_DOUBLE_LEFT 		0X100
#define MS_BTN_SIDE1    0x200
#define MS_BTN_SIDE2    0x400
#define MS_BTN_VOICE    0x800
#define MS_BTN_RESET    0x1000

enum BTN_INDEX
{
	VOICE_INDEX = 0x0,
	SIDE_BUTTON_1_INDEX = 0x10000,
	SIDE_BUTTON_2_INDEX = 0x20000
};

//------------tx power define-------------------
#define DEFAULT_NORMAL_TX_POWER   RF_POWER_P3p01dBm
#define DEFAULT_PAIR_TX_POWER	  RF_POWER_P3p01dBm
#define DEFAULT_EMI_TX_POWER      RF_POWER_P3p01dBm
//--------------------------------------------------

//----------------------------------------------------------------------------



typedef struct
{
    u32  dev_mac; //00
    u16  group_id;//04~05
    u16  rf_vid;	//06~07
    u8   bat_type;	//008
    u8	 tx_power;	//09
    u8	 paring_tx_power;//0a
    u8   emi_tx_power;   //00b
    u8	 wheel_direct;     //0c
    u8	 internal_cap;   //0d
    u8   sensor_direct;  //0e
    u8 	 aes_enable; //0f
    u8   pub_key[16];        //10~1f
    u8   device_name[18];	//20~31
} custom_cfg_t;

#if BLE_WEB_ENABLE
extern u8 dongle_ble_web;
#endif
_attribute_data_retention_user extern custom_cfg_t   user_cfg;
//_attribute_data_retention_user extern  u8 device_name_len;
_attribute_data_retention_user extern u8	tbl_advData_aaa[];
_attribute_data_retention_user extern u8 ui_mtu_size_exchange_req;
_attribute_data_retention_user extern u8 mouse_mac_address[];
_attribute_data_retention_user extern u8 mouse_version[];
_attribute_data_retention_user extern u32 device_protocol_version;
extern u8 direct_ble_flag;
_attribute_data_retention_user extern u8	web_flag;

//extern u8 connected_cmd;
_attribute_data_retention_user  extern u8 connect_ok;
_attribute_data_retention_user extern u8 connect_count;

_attribute_data_retention_ extern u32 record_channel;
_attribute_data_retention_user extern u8 protocol_start_record_cmd;
_attribute_data_retention_user extern u8 protocol_stop_record_cmd;
_attribute_data_retention_user extern u8 ui_mic_enable;
_attribute_data_retention_user extern short voice_power;
extern u8 super_mouse_flag;
/*_attribute_data_retention_user extern u8 wireless_charging_flag;*/

#define AUTHENTICATION_FLAG (1)

typedef enum
{
    POWER_ON_ANA_AAA = 0,
    DEEP_SLEEP_ANA_AAA = 1,
    MODE_CHANGE_REBOOT_ANA_AAA = 2,
    BLE_PAIR_REBOOT_ANA_AAA = 3,
    CONN_PARAM_FAIL_REBOOT_ANA_AA = 4,
    CLEAR_FLAG_ANA_AAA = 5,
    MUTI_DEVICE_REBOOT_ANA_AAA = 6,
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
	SPP_KEY_EVENT_AAA=BIT(4),
} key_EVENT_AAA;
typedef enum
{
	OS_WINDOWS_TYPE,
	OS_IOS_TYPE,
	OS_ANDROID,
}OS_TYPE_AAA;

typedef struct
{
    u32 dongle_id; //4
	
    u8 key[12];//12
	
    u8 slave_mac_addr[4];//4
	
    u8 mode;
    u8 mast_id;
	u16 temp1; //24
	
	u8  temp2[8]; //32
		
} FLASH_DEV_INFO_AAA;
#define SAVE_MAX_IN_FLASH  32
_attribute_data_retention_user extern FLASH_DEV_INFO_AAA flash_dev_info;

typedef struct
{
    u32 bin_crc;
    u8 rsv_1[3];
    u8 sensor_type;
    u8 sensor_pd1;
    u8 sensor_pd2;
    u8 sensor_pd3;
    u8 ms_data_len;
#if SHOW_MAST_REAL_MAC_DEBUG
    u8 master_mac[6];
#endif
} OUTPUT_DEV_INFO_AAA;
_attribute_data_retention_user extern OUTPUT_DEV_INFO_AAA output_dev_info;


typedef struct
{
    u8 bt_dpi;
#if DPI_SAVE_FLASH
    u8 d24g_dpi;
    u32 ble_time_out;
    s16 idx;
#endif
} FLASH_DPI_INFO_AAA;

typedef struct
{
	u32 usb_frequency;
	u32 ble_frequency;
	u8 reserve[6];
	s16 idx;
}POLLING_RATE_INFO;

_attribute_data_retention_user extern FLASH_DPI_INFO_AAA flash_dpi_info;
extern POLLING_RATE_INFO flash_polling_rate;
typedef struct
{
	u8 ctrl_key;
	u8 keycode;
	u8 next_interval; //uint 10
} y_kbdata_t;

_attribute_data_retention_user extern u8 deep_flag;

_attribute_data_retention_user extern  u8 suspend_wake_up_enable;
_attribute_data_retention_user extern u8 has_new_key_event;

_attribute_data_retention_user extern u32 btn_tick;
_attribute_data_retention_user extern u8 protocol_send_flag;
_attribute_data_retention_user extern u32 start_tick;
extern u8 device_product_no[4];

_attribute_data_retention_user extern u8 ble_status_aaa;
extern u8 lowbatt_flag;

extern _attribute_data_retention_user u8 	 my_batVal[1] ;


_attribute_data_retention_user extern mouse_data_t ms_data;

_attribute_data_retention_user extern u8 has_web_key_flag;

_attribute_data_retention_user extern u32 loop_cnt;

_attribute_data_retention_user extern u16 btn_value;
extern int deepRetWakeUp;
_attribute_data_retention_user extern u8 charging_batt_count;
extern u8 USB_INSERT_FLAG;
extern u8 usb_has_suspend_irq;
extern u8 mouse_effect_save_flag;
// ==> config by andy.xu
_attribute_data_retention_ extern u16 batt_vol_mv_real;

_attribute_data_retention_user extern int dev_info_idx;

extern void on_timer_wakeup();

u8 sensor_operation_flag;
u32 sensor_operation_tick;


extern void debug_loop_toggle();

extern void user_init_normal();

extern void main_loop(void);
extern void save_dev_info_flash();
extern void save_sensor_info();
extern int cccWrite(void *p);


extern void user_reboot(u8 reason);

extern void reset_idle_status();

extern void wheel_set_wakeup_level_deep();
extern _attribute_ram_code_ u8 wheel_get_value(u32 wheel_prepare_tick);
extern void battery_check();
extern void Set_web_flag();

extern u8 ble_notify_data_proc(u16 type, u8 *data, u32 size);
extern  u8 btn_get_value();
extern void btn_set_wakeup_level_deep();
extern void btn_set_wakeup_level_suspend(u8 enable);

extern void wheel_set_wakeup_level_suspend(u8 enable);

extern void check_web_process_aaa();
extern void check_reset();
extern void audio_enable();
extern void audio_disable();
extern void open_website();
extern void write_deep_ana0(u8 buf);

extern int flash_info_load_aaa(u32 s_addr, u8 *d_addr,  u8 len);
extern void hw_init();
extern void led_hw_init();
extern void super_mouse_proc();
extern u32 mouse_wheel_prepare_tick(void);
u8 get_ble_data_report_aaa();
extern void save_polling_rate_info();

extern void set_current_device_adv_type(u8 adv_type);


extern void ui_loop();
void dealwith_protocol_cmd();
void usb_resume_host(void);
void usb_init_interrupt();
void usb_user_init();
void usb_mode_loop();
void blemode_to_usbmode();
extern void usbhw_disable_manual_interrupt(int);
extern void usb_handle_irq();
extern void sincheck_loop();

extern int protocolWrite(void * src);
extern void led_hw_proc();
extern void led_ble_Adv();
extern void led_Connected();
extern u8 get_ble_led_condition();

extern void vMblueIdentificationInit();
extern u8 ucMblueGetFinger(unsigned char *finger);
extern u8 ucCheckIdentificationClientSalt(unsigned char *salt, unsigned int len);
extern void vSetIdentificationClientRandom(unsigned char *random, unsigned int len);
extern void vGetIdentificationSalt(unsigned char *salt);
extern void vSetIdentificationClientFinger(unsigned char *finger, unsigned int len);
extern void vGetIdentificationRandom(unsigned char *challenge);
#if PROTOCOL_OTA_ENABLE
unsigned char identification_client_session(unsigned char *in_data, unsigned int ulLength, unsigned char *out_data);
unsigned int identification_firmware(unsigned char *in_data, unsigned int ulLength, unsigned char *out_data);
unsigned int get_firmware_crc32_sum();
#endif
extern void set_last_device_adv();
extern void add_device_adv(u8 device_index);
extern void set_device_adv(u8 device_index);
extern pair_device_info_t get_current_adv_device();
extern void set_device_adv_info();
extern void set_current_device_host_mac(u8 *mac, u8 size);
extern void set_current_device_os_type(u8 os_type);
extern u8 save_current_device(u8 *slave_mac, u8 *master_mac, u8 adv_type, u8 os_type);
extern void force_save_current_device(u8);
extern void load_paired_device();
extern void clear_last_pair_device();
extern unsigned char is_mac_address(unsigned char *addr);
_attribute_data_retention_user extern u8 ble_adv_channel;
extern u8 mode_keycode_flag; //1-windows;2-android;3-mac
#define BATTER_MAX_LEVEL 4000
#define BATTER_MIN_LEVEL 3300
#define BATTER_LOW_VOL   3400      //3400mv
#define BATTER_ULTRA_LOW_VOL  3100    //3100mv
/*#define BATTER_WIRELESS_CHARGING_VOL  4550    //4550mv*/

#define DEVICE_CHANNEL_SIZE 3

enum device_channel_index{
	CHANNEL_1 = 0x01,
	CHANNEL_2 = 0x02,
	CHANNEL_3 = 0x03,
	CHANNEL_USB = 0x04
};

#define DEV_POSITION 17
#define USER_DEVICE_NAME_MAX_LEN 14

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


#define ENTER_ESTABLISH_MILL_SECOND (1000) // 2s

#endif

