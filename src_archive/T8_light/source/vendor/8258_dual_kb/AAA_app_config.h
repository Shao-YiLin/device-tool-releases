/********************************************************************************************************
 * @file     AAA_app_config.h
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

#pragma once

/* Enable C linkage for C++ Compilers: */
#if defined(__cplusplus)
extern "C"
{
#endif
//---------------------clock References-------------------------------------------------
#include "../../application/print/u_printf.h"

#define CLOCK_SYS_CLOCK_HZ  	48000000

#define CLOCK_HZ 16000000 // identify
	enum
	{
		CLOCK_SYS_CLOCK_1S = CLOCK_HZ,
		CLOCK_SYS_CLOCK_1MS = (CLOCK_SYS_CLOCK_1S / 1000),
		CLOCK_SYS_CLOCK_1US = (CLOCK_SYS_CLOCK_1S / 1000000),
	};

	//---------------------------watchdog References-------------------------------------

#define MODULE_WATCHDOG_ENABLE 1
#define WATCHDOG_INIT_TIMEOUT 500 //ms

//-----------------------------cfg address in flash--------------------------------
#define CFG_ADR_MAC 0x76000			  //4 BYTES
#define CFG_ADDR_ACCESS_CODE0 0x76006 //2 BYTES

#define CUST_CAP_INFO_ADDR 0x77000
#define CUST_TP_INFO_ADDR 0x77040
#define CUST_32KPAD_CAP_INFO_ADDR 0x77080
#define CUST_CAP_INFO_ADDR_2M 0x77020 //2M MODE
#define CUST_TP_INFO_ADDR_2M 0x77060

#define CFG_DEVICE_MODE_ADDR 0x79000
#define KEYBOARD_PARAM_ADDR 0x7a000

//-----------------------------DEEP SAVE FLG -----------------------------
#define PAIR_ANA_FLG 0X01				 // BIT 0
#define CLEAR_PAIR_ANA_FLAG 0XFE

#define FUN_MODE_ANA_FLG 0X06 //BIT 1,BIT2
#define CLEAR_FUN_MODE_ANA_FLAG 0xF9

	//---------------------------security References----------------------------------------

#define BLE_REMOTE_SECURITY_ENABLE 1

	//-------------------------------24g-----------------------

	enum
	{
		RF_1M_BLE_MODE = 0,
		RF_2M_2P4G_MODE = 1,
		USB_MODE = 2,
	};
	//-------------------------------function -------------------------------------------------------------

#define IOS_TYPE_CHAECK_FUN_ENABLE 0

#define CONSUMER_FUN_ENABLE_AAA 1
#define CONSUMER_REPORT_BYTE_AAA 1
#define SYSTEM_KEY_FUN_ENABLE_AAA 1

#define HAS_SOFT_POWER_KEY_AAA 0
#define HAS_BACK_LIGHT_AAA 0
#define HAS_BLINK_LED_WHEN_NEW_DATA 0

#define HAS_NUM_LED_AAA 0
#define HAS_SCROLL_LED_AAA 0
#define HAS_CAPS_LED_AAA 0
#define HAS_DEVICE_LED_AAA 0
#define HAS_PAIR_LED_AAA 0

#define BACK_LIGHT_ON 0
#define BACK_LIGHT_OFF 1

#define IS_SINGLE_GPIO_PAIR_BUTTON 0

#if IS_SINGLE_GPIO_PAIR_BUTTON
#define GPIO_PAIR_BTN GPIO_PC0
#endif

	//---------------------project---------------------------

#define Telink_QFN32_PRJ 0
#define WANG_HONG_TAI_BEI_PRJ 1
#define SUO_AI_PRJ 2

#define PRJ_NAME Telink_QFN32_PRJ

	//----------------------for debug--------------

#define BLE_SNIFF_DEBUG 0	 //for ti packet sniff   if enable mouse do not enter sleep for ever
#define BLE_OTA_LED_DEBUG 0	 //led show  ota result
#define DEBUG_GPIO_AAA 0	 //
#define TEST_DRAW_A_SQUARE 0 //when mouse  connected auto draw  a square
#define TEST_LOST_RATE 0	 //test 2.4g rf lost rate
#define AUDIO_AUTO_OPEN_TEST_DEBUG 0

	//-------------------------------

#if UART_PRINT_DEBUG_ENABLE
//#if (UART_PRINT)

#define PRINT_BAUD_RATE 115200
#define DEBUG_INFO_TX_PIN GPIO_PD7
#define PD7_OUTPUT_ENABLE 1
#define PULL_WAKEUP_SRC_PD7 PM_PIN_PULLUP_1M
#define PRINT_DEBUG_INFO 0
#endif


#define REPORT_ID_KEYBOARD_INPUT_AAA 1		  //!< Keyboard input report ID
#define REPORT_ID_CONSUME_CONTROL_INPUT_AAA 2 //!< Consumer Control input report ID
#define REPORT_ID_MOUSE_INPUT_AAA 3			  //!< Mouse input report ID
#define REPORT_ID_GAMEPAD_INPUT_AAA 4		  //!< Gamepad  input report ID
#define REPORT_ID_LED_OUT_AAA 0				  //!< LED output report ID
#define REPORT_ID_FEATURE_AAA 0				  //!< Feature report ID
#define REPORT_ID_USER_FEATURE_AAA 5
#define REPORT_ID_SYSTEM_INPUT_AAA 6 //!< Gamepad  input report ID
#define REPORT_ID_SPP_INPUT_AAA 7	 //!< Gamepad  input report ID

#define HID_REPORT_ID_KEYBOARD_INPUT                    1   //!< Keyboard input report ID
#define HID_REPORT_ID_CONSUME_CONTROL_INPUT             2   //!< Consumer Control input report ID
#define HID_REPORT_ID_MOUSE_INPUT                       3   //!< Mouse input report ID
#define HID_REPORT_ID_GAMEPAD_INPUT						4   //!< Gamepad  input report ID
#define HID_REPORT_ID_LED_OUT                           0   //!< LED output report ID
#define HID_REPORT_ID_FEATURE                           0   //!< Feature report ID
#define HID_REPORT_ID_USER_FEATURE						5

#define HID_REPORT_ID_SYSTEM_INPUT						6   //!< Gamepad  input report ID
#define HID_REPORT_ID_SPP_INPUT							7   //!< Gamepad  input report ID
#define HID_REPORT_ID_MADAO_OUTPUT						8   //!< Gamepad  input report ID

//-----------------------device information-----------------------
#if (PRJ_NAME == Telink_QFN32_PRJ)
#include "AAA_app_config_Telink_QFN32.h"
#endif

#define RX_FIFO_SIZE 64
#define RX_FIFO_NUM 8

#define TX_FIFO_SIZE 40
#define TX_FIFO_NUM 16

#define RF_TX_FIFO_ALLOW_NUM (TX_FIFO_NUM - 2 - 4) //14-5 audio -1 mouse =8

//--------------Debug gpio-------------------
#if DEBUG_GPIO_AAA

#define PIN_DEBUG_D0 GPIO_PD0
#define PD0_INPUT_ENABLE 0
#define PD0_OUTPUT_ENABLE 1
#define PD0_DATA_OUT 0
#define DEBUG_D0_PIN_HIGH gpio_write(GPIO_PD0, 1);
#define DEBUG_D0_PIN_LOW gpio_write(GPIO_PD0, 0);

#define PIN_DEBUG_B2 GPIO_PB2
#define PB2_INPUT_ENABLE 0
#define PB2_OUTPUT_ENABLE 1
#define PB2_DATA_OUT 0
#define DEBUG_B2_PIN_HIGH gpio_write(GPIO_PB2, 1);
#define DEBUG_B2_PIN_LOW gpio_write(GPIO_PB2, 0);

#define PIN_DEBUG_D1 GPIO_PD1
#define PD1_INPUT_ENABLE 0
#define PD1_OUTPUT_ENABLE 1
#define PD1_DATA_OUT 0
#define DEBUG_D1_PIN_HIGH gpio_write(GPIO_PD1, 1);
#define DEBUG_D1_PIN_LOW gpio_write(GPIO_PD1, 0);

#else
#define DEBUG_D0_PIN_HIGH
#define DEBUG_D0_PIN_LOW
#define DEBUG_D1_PIN_HIGH
#define DEBUG_D1_PIN_LOW
#define DEBUG_B2_PIN_HIGH
#define DEBUG_B2_PIN_LOW
#endif

//-------------------------------------
///////////////////////////////////// ATT  HANDLER define ///////////////////////////////////////
#define HID_MOUSE_ATT_ENABLE 1
#define HID_KEYBOARD_ATT_ENABLE 1

	typedef enum

	{
		ATT_H_START = 0,

		/*********Gap*************************************************************************************/
		GenericAccess_PS_H,			   //UUID: 2800, 	VALUE: uuid 1800
		GenericAccess_DeviceName_CD_H, //UUID: 2803, 	VALUE:  			Prop: Read | Notify
		GenericAccess_DeviceName_DP_H, //UUID: 2A00,   VALUE: device name
		GenericAccess_Appearance_CD_H, //UUID: 2803, 	VALUE:  			Prop: Read
		GenericAccess_Appearance_DP_H, //UUID: 2A01,	VALUE: appearance
		CONN_PARAM_CD_H,			   //UUID: 2803, 	VALUE:  			Prop: Read
		CONN_PARAM_DP_H,			   //UUID: 2A04,   VALUE: connParameter

		/*****gatt *****************************************************************************************/
		GenericAttribute_PS_H, //UUID: 2800,	VALUE: uuid 1801
		//GenericAttribute_ServiceChanged_CD_H,	//UUID: 2803,	VALUE:				Prop: Indicate
		//GenericAttribute_ServiceChanged_DP_H,	//UUID: 2A05,	VALUE: service change
		//GenericAttribute_ServiceChanged_CCB_H,	//UUID: 2902,	VALUE: serviceChangeCCC

		/****device information******************************************************************************************/
		DeviceInformation_PS_H,		  //UUID: 2800, 	VALUE: uuid 180A
		DeviceInformation_pnpID_CD_H, //UUID: 2803, 	VALUE:  			Prop: Read
		DeviceInformation_pnpID_DP_H, //UUID: 2A50,	VALUE: PnPtrs

		/*********battery service *************************************************************************************/
		BATT_PS_H,				//UUID: 2800,	VALUE: uuid 180f
		BATT_LEVEL_INPUT_CD_H,	//UUID: 2803,	VALUE:				Prop: Read | Notify
		BATT_LEVEL_INPUT_DP_H,	//UUID: 2A19	VALUE: batVal
		BATT_LEVEL_INPUT_CCB_H, //UUID: 2902,	VALUE: batValCCC

		/**********HID************************************************************************************/
		HID_PS_H, //UUID: 2800, 	VALUE: uuid 1812

		//HID_INCLUDE_H,							//UUID: 2802,	VALUE: include

		//protocol
		HID_PROTOCOL_MODE_CD_H, //UUID: 2803, 	VALUE:  			Prop: read | write_without_rsp
		HID_PROTOCOL_MODE_DP_H, //UUID: 2A4E,	VALUE: protocolMode



#if HID_MOUSE_ATT_ENABLE
		HID_BOOT_MOUSE_REPORT_INPUT_CD_H,  //UUID: 2803,	VALUE:				Prop: Read | Notify
		HID_BOOT_MOUSE_REPORT_INPUT_DP_H,  //UUID: 2A22,	VALUE: bootKeyInReport
		HID_BOOT_MOUSE_REPORT_INPUT_CCB_H, //UUID: 2902,	VALUE: bootKeyInReportCCC

		HID_MOUSE_REPORT_INPUT_CD_H,  //UUID: 2803, 	VALUE:  			Prop: Read | Notify
		HID_MOUSE_REPORT_INPUT_DP_H,  //UUID: 2A4D, 	VALUE: reportConsumerIn
		HID_MOUSE_REPORT_INPUT_CCB_H, //UUID: 2902, 	VALUE: reportConsumerInCCC
		HID_MOUSE_REPORT_INPUT_REF_H, //UUID: 2908    VALUE: REPORT_ID_CONSUMER, TYPE_INPUT
#endif

#if HID_KEYBOARD_ATT_ENABLE
		HID_NORMAL_KB_REPORT_INPUT_CD_H,  //UUID: 2803, 	VALUE:  			Prop: Read | Notify
		HID_NORMAL_KB_REPORT_INPUT_DP_H,  //UUID: 2A4D, 	VALUE: reportKeyIn
		HID_NORMAL_KB_REPORT_INPUT_CCB_H, //UUID: 2902, 	VALUE: reportKeyInInCCC
		HID_NORMAL_KB_REPORT_INPUT_REF_H, //UUID: 2908    VALUE: REPORT_ID_KEYBOARD, TYPE_INPUT
		HID_BOOT_KB_REPORT_INPUT_CD_H,	//UUID: 2803, 	VALUE:  			Prop: Read | Notify
		HID_BOOT_KB_REPORT_INPUT_DP_H,	//UUID: 2A22, 	VALUE: bootKeyInReport
		HID_BOOT_KB_REPORT_INPUT_CCB_H, //UUID: 2902, 	VALUE: bootKeyInReportCCC

		HID_BOOT_KB_REPORT_OUTPUT_CD_H, //UUID: 2803, 	VALUE:  			Prop: Read | write| write_without_rsp
		HID_BOOT_KB_REPORT_OUTPUT_DP_H, //UUID: 2A32, 	VALUE: bootKeyOutReport

		HID_CONSUME_REPORT_INPUT_CD_H,	//UUID: 2803, 	VALUE:  			Prop: Read | Notify
		HID_CONSUME_REPORT_INPUT_DP_H,	//UUID: 2A4D, 	VALUE: reportConsumerIn
		HID_CONSUME_REPORT_INPUT_CCB_H, //UUID: 2902, 	VALUE: reportConsumerInCCC
		HID_CONSUME_REPORT_INPUT_REF_H, //UUID: 2908    VALUE: REPORT_ID_CONSUMER, TYPE_INPUT

	

		HID_NORMAL_KB_REPORT_OUTPUT_CD_H,  //UUID: 2803, 	VALUE:  			Prop: Read | write| write_without_rsp
		HID_NORMAL_KB_REPORT_OUTPUT_DP_H,  //UUID: 2A4D, 	VALUE: reportKeyOut
		HID_NORMAL_KB_REPORT_OUTPUT_REF_H, //UUID: 2908    VALUE: REPORT_ID_KEYBOARD, TYPE_OUTPUT
#endif
		HID_REPORT_MAP_CD_H, //UUID: 2803, 	VALUE:  			Prop: Read
		HID_REPORT_MAP_DP_H, //UUID: 2A4B, 	VALUE: reportKeyIn
		//HID_REPORT_MAP_EXT_REF_H,				//UUID: 2907 	VALUE: extService

		HID_INFORMATION_CD_H, //UUID: 2803, 	VALUE:  			Prop: read
		HID_INFORMATION_DP_H, //UUID: 2A4A 	VALUE: hidInformation

		//control point
		HID_CONTROL_POINT_CD_H, //UUID: 2803, 	VALUE:  			Prop: write_without_rsp
		HID_CONTROL_POINT_DP_H, //UUID: 2A4C 	VALUE: controlPoint

/************Ota**********************************************************************************/
#if OTA_ENABLE_AAA

		OTA_PS_H,		  //UUID: 2800, 	VALUE: telink ota service uuid
		OTA_CMD_OUT_CD_H, //UUID: 2803, 	VALUE:  			Prop: read | write_without_rsp
		OTA_CMD_OUT_DP_H, //UUID: telink ota uuid,  VALUE: otaData
		OTA_CMD_INF_CD_H, //UUID: 2901, 	VALUE: otaName
		OTA_CMD_INF_DP_H,
#endif

		/******madao primary service****************************************************************************/
		PROTOCOL_PS_H,

		//tx
		PROTOCOL_TX_CD_H,
		PROTOCOL_TX_DP_H,
		PROTOCOL_TX_CCB_H,
		//rx
		PROTOCOL_RX_CD_H,
		PROTOCOL_RX_DP_H,

		/**********************************************************************************************/
		SPP_PS_H, //UUID: 2800,	VALUE: telink audio service uuid

		SPP_INPUT_CD_H,	 //UUID: 2803,	VALUE:				Prop: Read | Notify
		SPP_INPUT_DP_H,	 //UUID: telink mic uuid,  VALUE: micData
		SPP_INPUT_CCB_H, //UUID: 2A19	VALUE: micDataCCC

		ATT_END_H,

	} ATT_HANDLE;

#include "../common/default_config.h"

/* Disable C linkage for C++ Compilers: */
#if defined(__cplusplus)
}
#endif
