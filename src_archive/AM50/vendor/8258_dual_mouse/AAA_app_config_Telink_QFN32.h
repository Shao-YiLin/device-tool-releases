#ifndef _TELINK_QFN32_AAA_H_
#define _TELINK_QFN32_AAA_H_

#define WIDE_STRINGIZE(x) L ## x
#define WIDE(x) WIDE_STRINGIZE(x)

#if (PRJ_NAME == Telink_QFN32_PRJ)

#define PWM_LED_ENABLE          1
#define MOTOR_ENABLE             0
#define WATERFALL_LAMP_ENABLE    0
#define CIRCULATION_LAMP_ENABLE  0
#define GRADUAL_LAMP_ENABLE      1
#define BLE_WEB_ENABLE           1
#define SINCHECK_PCM_DATA_ENABLE 1


//-----------------------------for debug-----------------------------
#define BLE_SNIFF_DEBUG 0               //for ti packet sniff   if enable mouse do not enter sleep for ever
#define BLE_OTA_LED_DEBUG 0             //led show  ota result
#define DEBUG_GPIO_AAA 0                //
#define TEST_DRAW_A_SQUARE 0            //when mouse  connected auto draw  a square
#define TEST_LOST_RATE 0                //test 2.4g rf lost rate
#define AUDIO_AUTO_OPEN_TEST_DEBUG 0    //823x not support
#define DEVICE_NAME_INCLUDE_MAC_DEBUG 0 //for debug
#define MUTI_DEVICE_SWITCH_DEBUG 0      //for debug

#define ADC_BATT_DEBUG 0 //enbale : in ble mode ��notify adc value.

// test  mcu current ,in the mode,sensor shut down ,
// when both left and right  button are pressed ��mouse auto draw a square in conencted
#define TEST_MCU_CURRENT_DEBUG 0

#define SHOW_MAST_REAL_MAC_DEBUG 0 //for test get master real addr

#define DEBUG_TOGGLE_GPIO_ENABLE 0
//-----------------------------PM --------------------------------

#define BLE_APP_PM_ENABLE 1
#define PM_DEEPSLEEP_RETENTION_ENABLE 0 //must set 0
#define BLE_ADV_ENTER_DEEPSLEEP_AFTER_TIME_OUT_ENABLE_AAA 1
#define BLE_CONNECT_ENTER_DEEPSLEEP_AFTER_TIME_OUT_ENABLE_AAA 1

#define D24G_ADV_ENTER_DEEPSLEEP_AFTER_TIME_OUT_ENABLE_AAA 0
#define D24G_CONNECT_ENTER_DEEPSLEEP_AFTER_TIME_OUT_ENABLE_AAA 0

#define BLE_ADV_TIMER_OUT 30 //unit 1s

#define BLE_CONNECT_TIME_OUT 600
#define DEFAULT_BLE_FREQUENCY 125
#define DEFAULT_USB_FREQUENCY 1000

#define DEVICE_NAME_AAA "AM50" //
#define DEVICE_NAME_LENGTH (4) // max length is 14
#define DEVICE_NAME_EXTEND_LENGTH (7)

#if PM_DEEPSLEEP_RETENTION_ENABLE
#define _attribute_data_retention_user _attribute_data_retention_
#else
#define _attribute_data_retention_user
#endif

//-----------------------product function referance---------------------------------------

#define MOUSE_PIPE1_DATA_WITH_DID 1 //If you want to fit the old hamster dongle (firmware version :v5.3) it must set 0

#define MOUSE_DATA_LEN_AAA 6 //If you want to fit the old hamster dongle (firmware version :v5.3)  it must set 4

#define OTA_ENABLE_AAA 1

#if OTA_ENABLE_AAA
#define PROTOCOL_OTA_ENABLE 1
#endif

#define EMI_TEST_FUN_ENABLE_AAA 1 //If the flash  is 128k, must be set 0��save 2k

#define BATT_CHECK_ENABLE 1

#define WHEEL_FUN_ENABLE_AAA 1

#define BUTTON_FUN_ENABLE_AAA 1

#define SENSOR_FUN_ENABLE_AAA 1
#define SENSOR_CS_ENABLE 1 //zzh 3805/3220必须打开
#define SENSOR_SHUT_DOWN_ENABLE 0
#define SENSOR_3220_ENABLE 1

#define BLT_APP_LED_ENABLE 1
#define LED_OFF_AAA 0
#define LED_ON_AAA 1

#define DIRECT_ADV_TO_UNDIRECT_ENABLE 1

#define DOUBLE_CLICK_LEFT_FUN_ENABLE 0

#define BLE_AUDIO_ENABLE 1
#define BLE_LED_ENABLE 1
#define ENCODER_TYPE 1 /* 0: adpcm  1: ico */

////////////////////////// AUDIO CONFIG /////////////////////////////
#if (BLE_AUDIO_ENABLE)
#define BLE_DMIC_ENABLE 0 //0: Amic   1: Dmic
#if (ENCODER_TYPE == 0)
#define ADPCM_PACKET_LEN 128
#define TL_MIC_ADPCM_UNIT_SIZE 248
#define TL_MIC_BUFFER_SIZE 992
#else
#define ADPCM_PACKET_LEN 40
#define TL_MIC_ADPCM_UNIT_SIZE 320
#define TL_MIC_BUFFER_SIZE 1280
#endif

#define GPIO_AMIC1_BIAS GPIO_PC4
//	#define PB4_INPUT_ENABLE	0
//	#define PB4_OUTPUT_ENABLE	1
#endif

#define AUTO_CHECK_OS_TYPE 0
#define Microsoft_Swift_Pairing_ENABLE 1 //if enable then device_name_len<=11

#define BLT_SOFTWARE_TIMER_ENABLE 0

#define MUTI_SENSOR_ENABLE 1
#define DPI_SAVE_FLASH 1
#define ENTER_PAIR_WHEN_NEVER_PAIRED_ENABLE 1 //For the better production of the factory

//aes mehtod
//0:no aes encryption   no key					old method
//1:aes128 encryption   User burn fixed key   	old method
//2:aes128 encryption	Dynamic random key
//attention :Dongle&KB must be set to the same encryption mode
#define AES_METHOD 2
//---------------------------gpio---------------------------------------------
#if SENSOR_FUN_ENABLE_AAA
#define PIN_SIF_MOTION GPIO_PD3
#define PD3_INPUT_ENABLE 1
#define PD3_OUTPUT_ENABLE 0
#define PD3_DATA_OUT 1
#define PULL_WAKEUP_SRC_PD3 PM_PIN_PULLUP_1M

#define PIN_SIF_SCL GPIO_PD6
#define PD6_INPUT_ENABLE 0
#define PD6_OUTPUT_ENABLE 1
#define PD6_DATA_OUT 1
#define PULL_WAKEUP_SRC_PD6 PM_PIN_PULLUP_1M

#define PIN_SIF_SDA GPIO_PD4
#define PD4_INPUT_ENABLE 1
#define PD4_OUTPUT_ENABLE 1
#define PD4_DATA_OUT 1
#define PULL_WAKEUP_SRC_PD4 PM_PIN_PULLUP_10K

#if SENSOR_CS_ENABLE
#define PIN_SENSOR_CS GPIO_PD1
#define PD1_INPUT_ENABLE 0
#define PD1_OUTPUT_ENABLE 1
#define PD1_DATA_OUT 1
#define PULL_WAKEUP_SRC_PD1 PM_PIN_PULLUP_1M
#endif
#endif

#if BUTTON_FUN_ENABLE_AAA

#define BTN_NUM_AAA (7 + (UART_PRINT_DEBUG_ENABLE ? 0 : 1))
#if !UART_PRINT_DEBUG_ENABLE
#define PIN_INFRARED	GPIO_PB1
#define PB1_INPUT_ENABLE 1
#define PB1_OUTPUT_ENABLE 0
#define PB1_DATA_OUT 0
#define PULL_WAKEUP_SRC_PB1 PM_PIN_PULLUP_10K
#else
#define PIN_INFRARED
#endif

#define PIN_BTN_LEFT GPIO_PB5
#define PB5_INPUT_ENABLE 1
#define PB5_OUTPUT_ENABLE 0
#define PB5_DATA_OUT 0
#define PULL_WAKEUP_SRC_PB5 PM_PIN_PULLUP_1M

#define PIN_BTN_RIGHT GPIO_PB3
#define PB3_INPUT_ENABLE 1
#define PB3_OUTPUT_ENABLE 0
#define PB3_DATA_OUT 0
#define PULL_WAKEUP_SRC_PB3 PM_PIN_PULLUP_1M

#define PIN_BTN_MIDDLE GPIO_PB4
#define PB4_INPUT_ENABLE 1
#define PB4_OUTPUT_ENABLE 0
#define PB4_DATA_OUT 0
#define PULL_WAKEUP_SRC_PB4 PM_PIN_PULLUP_1M

#define PIN_BTN_VOICE GPIO_PA1
#define PA1_INPUT_ENABLE 1
#define PA1_OUTPUT_ENABLE 0
#define PA1_DATA_OUT 0
#define PULL_WAKEUP_SRC_PA1 PM_PIN_PULLUP_1M

#define PIN_BTN_SIDE_BUTTON_2 GPIO_PC3
#define PC3_INPUT_ENABLE 1
#define PC3_OUTPUT_ENABLE 0
#define PC3_DATA_OUT 0
#define PULL_WAKEUP_SRC_PC3 PM_PIN_PULLUP_1M

#define PIN_BTN_SIDE_BUTTON_1 GPIO_PD0
#define PD0_INPUT_ENABLE 1
#define PD0_OUTPUT_ENABLE 0
#define PD0_DATA_OUT 1
#define PULL_WAKEUP_SRC_PD0 PM_PIN_PULLUP_1M

#define GPIO_RESET GPIO_PB2
#define PB2_INPUT_ENABLE 1
#define PB2_OUTPUT_ENABLE 0
#define PB2_DATA_OUT 0
#define PULL_WAKEUP_SRC_PB2 PM_PIN_PULLUP_1M
#define BTN_MATRIX                                                                                                           \
	{                                                                                                                        \
		GPIO_RESET, PIN_BTN_LEFT, PIN_BTN_RIGHT, PIN_BTN_MIDDLE, PIN_BTN_VOICE, PIN_BTN_SIDE_BUTTON_1, PIN_BTN_SIDE_BUTTON_2, PIN_INFRARED \
	}
#endif

#if WHEEL_FUN_ENABLE_AAA
#define PIN_WHEEL_1 GPIO_PB6 
#define PB6_INPUT_ENABLE 1
#define PB6_OUTPUT_ENABLE 0
#define PB6_DATA_OUT 1
#define PULL_WAKEUP_SRC_PB6 PM_PIN_PULLUP_1M

#define PIN_WHEEL_2 GPIO_PB7                 
#define PB7_INPUT_ENABLE 1                   
#define PB7_OUTPUT_ENABLE 0                  
#define PB7_DATA_OUT 1                       
#define PULL_WAKEUP_SRC_PB7 PM_PIN_PULLUP_1M 

#define WHEEL_ADDRES_D2 0X02
#define WHEEL_ADDRES_D3 0X03
#endif

#if BLT_APP_LED_ENABLE
#define PIN_BLE_RED_LED GPIO_PD2 //blue
#define PD2_INPUT_ENABLE 0
#define PD2_OUTPUT_ENABLE 1
#define PD2_DATA_OUT LED_OFF_AAA
#define PULL_WAKEUP_SRC_PD2 PM_PIN_UP_DOWN_FLOAT

#define PIN_BLE_GREEN_LED GPIO_PD5 //green
#define PD5_INPUT_ENABLE 0
#define PD5_OUTPUT_ENABLE 1
#define PD5_DATA_OUT LED_OFF_AAA
#define PULL_WAKEUP_SRC_PD5 PM_PIN_UP_DOWN_FLOAT

#define PIN_BLE_BLUE_LED GPIO_PD7 //red
#define PD7_INPUT_ENABLE 0
#define PD7_OUTPUT_ENABLE 1
#define PD7_DATA_OUT LED_OFF_AAA
#define PULL_WAKEUP_SRC_PD7 PM_PIN_UP_DOWN_FLOAT
#endif

#define GPIO_LI_CHARG GPIO_PA2
#define PA2_INPUT_ENABLE 1
#define PA2_OUTPUT_ENABLE 0
#define PA2_DATA_OUT 0
#define PULL_WAKEUP_SRC_PA2 PM_PIN_PULLUP_1M


#define GPIO_BATT_CHECK GPIO_PC2
#define PC2_INPUT_ENABLE 1
#define PC2_OUTPUT_ENABLE 0
#define PC2_DATA_OUT 0
#define PULL_WAKEUP_SRC_PC2 PM_PIN_PULLUP_1M

#if (BATT_CHECK_ENABLE)
//telink device: you must choose one gpio with adc function to output high level(voltage will equal to vbat), then use adc to measure high level voltage
//use PB7 output high level, then adc measure this high level voltage
#define GPIO_VBAT_DETECT GPIO_PC5
#define PC5_FUNC AS_GPIO
#define PC5_INPUT_ENABLE 0
#define ADC_INPUT_PCHN C5P //corresponding  ADC_InputPchTypeDef in adc.h
#endif
#endif
//#endif

#endif

