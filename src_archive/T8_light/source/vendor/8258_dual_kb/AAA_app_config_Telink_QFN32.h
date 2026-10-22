
#ifndef _AAA_APP_CONFIG_TELINK_QFN32_AAA_H_
#define _AAA_APP_CONFIG_TELINK_QFN32_AAA_H_

#if (PRJ_NAME == Telink_QFN32_PRJ)

#define AUTO_CHECK_OS_TYPE 0
#define BLE_WEB_ENABLE	1
#define Microsoft_Swift_Pairing_ENABLE 1 //if enable then device_name_len<=12

//-----------------------------PM --------------------------------

#define BLE_APP_PM_ENABLE 1
#define PM_DEEPSLEEP_RETENTION_ENABLE 0
#define BLE_ADV_ENTER_DEEPSLEEP_AFTER_TIME_OUT_ENABLE_AAA 1
#define BLE_CONNECT_ENTER_DEEPSLEEP_AFTER_TIME_OUT_ENABLE_AAA 1

#if PM_DEEPSLEEP_RETENTION_ENABLE
#define _attribute_data_retention_user _attribute_data_retention_
#else
#define _attribute_data_retention_user
#endif

#define DEVICE_NAME_LENGTH (2) // max length is 14
#define DEVICE_NAME_EXTEND_LENGTH (7)

#define DEVICE_NAME_AAA "T8" //58:8258  32: QFN32
#define DEVICE_NAME_L L"T8" //58:8258  32: QFN32


#define DEVICE_PRODUCT_NO "3006"

#define PRODUCT_NO_LENGTH 4

#define MOUSE_PIPE1_DATA_WITH_DID 1 //If you want to fit the old hamster dongle (firmware version :v5.3) it must set 0

#define MOUSE_DATA_LEN_AAA 6 //4 :8bit sensor  6:16bit sensor

#define OTA_ENABLE_AAA 1

#if OTA_ENABLE_AAA
#define PROTOCOL_OTA_ENABLE 1
#endif

#define EMI_TEST_FUN_ENABLE_AAA 1

#define BATT_CHECK_ENABLE 1

#define SINCHECK_PCM_DATA_ENABLE 1

#define WHEEL_FUN_ENABLE_AAA 1
#define S1_FUN_ENABLE_AAA 1
#define BLT_APP_LED_ENABLE 1
#define LED_OFF_AAA 0
#define LED_ON_AAA 1

#define IS_SINGLE_GPIO_CHANGE_MODE 0

#define CHECK_CHARGE_ENABLE 0

#define COMBE_PACKET_IN_24G_MODE_ENABLE 0

#define DO_TASK_WHEN_RF_IN_24G_MODE_ENALBE 0

#define DIRECT_ADV_TO_UNDIRECT_ENABLE 1

#define BATT_ENABLE_AAA 1

#define BLE_AUDIO_ENABLE1 1

// NKRO
//#define BLE_NKRO_ENABLE 0
//#if BLE_NKRO_ENABLE
//#define BLE_ARR_NUM 14
//#else
//#define BLE_ARR_NUM 8
//#endif

#define ENCODER_TYPE 1 /* 0: adpcm  1: ico */

#if (BLE_AUDIO_ENABLE1)
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

//麦克风
#define GPIO_AMIC1_BIAS GPIO_PC4

#define MIC_P GPIO_PC0
#define PULL_WAKEUP_SRC_PC0 PM_PIN_PULLDOWN_100K

#define MIC_M GPIO_PC1
#define PULL_WAKEUP_SRC_PC1 PM_PIN_PULLDOWN_100K

#endif

#define ENTER_PAIR_WHEN_NEVER_PAIRED_ENABLE 1 //For the better production of the factory
// aes mehtod
//0:no aes encryption   no key					old method
//1:aes128 encryption   User burn fixed key   	old method
//2:aes128 encryption	Dynamic random key
//attention :Dongle&KB must be set to the same encryption mode
#define AES_METHOD 2
//---------------------------gpio---------------------------------------------

#if BLT_APP_LED_ENABLE

//蓝牙指示灯
#define PIN_BLE1_LED GPIO_PB0
#define PB0_INPUT_ENABLE 0
#define PB0_OUTPUT_ENABLE 1
#define PB0 _DATA_OUT 0
#define PULL_WAKEUP_SRC_PB0 PM_PIN_PULLDOWN_100K

#define PIN_BLE2_LED GPIO_PA0
#define PA0_INPUT_ENABLE 0
#define PA0_OUTPUT_ENABLE 1
#define PA0_DATA_OUT 0
#define PULL_WAKEUP_SRC_PA0 PM_PIN_PULLDOWN_100K

#define PIN_BLE3_LED GPIO_PA1
#define PA1_INPUT_ENABLE 0
#define PA1_OUTPUT_ENABLE 1
#define PA1_DATA_OUT 0
#define PULL_WAKEUP_SRC_PA1 PM_PIN_PULLDOWN_100K

// 大小写指示灯
#if !UART_PRINT_DEBUG_ENABLE
#define PIN_CAPS_LED GPIO_PD7
#define PD7_INPUT_ENABLE 0
#define PD7_OUTPUT_ENABLE 1
#define PD7_DATA_OUT 0
#define PULL_WAKEUP_SRC_PD7 PM_PIN_PULLDOWN_100K
#endif

// 语音指示灯
#define PIN_VOICE_LED GPIO_PD2
#define PD2_INPUT_ENABLE 0
#define PD2_OUTPUT_ENABLE 1
#define PD2_DATA_OUT 0
#define PULL_WAKEUP_SRC_PD2 PM_PIN_PULLDOWN_100K

// 电量&充电指示灯
/* RED */
#define PIN_BAT_LED GPIO_PD1
#define PD1_INPUT_ENABLE 0
#define PD1_OUTPUT_ENABLE 1
#define PD1_DATA_OUT 1
#define PULL_WAKEUP_SRC_PD1 PM_PIN_PULLUP_1M
/* GREEN */
#define PIN_FILLED_LED GPIO_PD5
#define PD5_INPUT_ENABLE 0
#define PD5_OUTPUT_ENABLE 1
#define PD5_DATA_OUT 1
#define PULL_WAKEUP_SRC_PD5 PM_PIN_PULLUP_1M

#endif

// 开关机状态标志
#define PIN_ON_OFF_STATE GPIO_PD4
#define PD4_INPUT_ENABLE 1
#define PD4_OUTPUT_ENABLE 0
#define PD4_DATA_OUT 0
#define PULL_WAKEUP_SRC_PD4 PM_PIN_PULLUP_1M

/* 滚轮 */
#if WHEEL_FUN_ENABLE_AAA
#define PIN_WHEEL_1 GPIO_PB6
#define PB6_INPUT_ENABLE 1
#define PB6_OUTPUT_ENABLE 0
#define PB6_DATA_OUT 0
#define PULL_WAKEUP_SRC_PB6 PM_PIN_PULLUP_1M

#define PIN_WHEEL_2 GPIO_PB7
#define PB7_INPUT_ENABLE 1
#define PB7_OUTPUT_ENABLE 0
#define PB7_DATA_OUT 0
#define PULL_WAKEUP_SRC_PB7 PM_PIN_PULLUP_1M

#define WHEEL_ADDRES_D2 0X02
#define WHEEL_ADDRES_D3 0X03

/* 按键 */
// 滚轮按键
#define WHEEL_BUTTON_ENABLE 1
#if WHEEL_BUTTON_ENABLE

#define PIN_BTN_WHEEL GPIO_PC7
#define PC7_INPUT_ENABLE 1
#define PC7_OUTPUT_ENABLE 0
#define PC7_DATA_OUT 1
#define PULL_WAKEUP_SRC_PC7 1

#endif

#endif
// S1按键
#if S1_FUN_ENABLE_AAA
// 行作为输入下拉，如果读到1表示按键按下
#define PIN_BTN_S1_ROW GPIO_PB5
#define PB5_INPUT_ENABLE 1
#define PB5_OUTPUT_ENABLE 0
#define PULL_WAKEUP_SRC_PB5 PM_PIN_PULLDOWN_100K

// 列输入上拉
#define PIN_BTN_S1_COL GPIO_PB4
#define PB4_INPUT_ENABLE 1
#define PB4_OUTPUT_ENABLE 0
#define PB4_DATA_OUT 1
#define PULL_WAKEUP_SRC_PB4 PM_PIN_PULLUP_10K
#endif

/* 充电 */
//是否在充电（usb插入）
#define GPIO_BATT_CHECK GPIO_PC6
#define PC6_INPUT_ENABLE 1
#define PC6_OUTPUT_ENABLE 0
#define PC6_DATA_OUT 0
#define PULL_WAKEUP_SRC_PC6 0

#define PB3_INPUT_ENABLE 1
#define PB3_OUTPUT_ENABLE 0
#define PB3_DATA_OUT 0
#define PULL_WAKEUP_SRC_PB3 PM_PIN_PULLUP_10K

//是否充满电（充满低电平）
#define GPIO_LI_CHARG GPIO_PB2
#define PB2_INPUT_ENABLE 1
#define PB2_OUTPUT_ENABLE 0
#define PB2_DATA_OUT 0
#define PULL_WAKEUP_SRC_PB2 PM_PIN_PULLUP_10K

// 默认状态为低电平，高电平有效
// SN32中断
#define GPIO_SN32_IRQ GPIO_PD3
//#define PD3_INPUT_ENABLE 0
//#define PD3_OUTPUT_ENABLE 1
//#define PD3_DATA_OUT 1
//#define PULL_WAKEUP_SRC_PD3 PM_PIN_PULLDOWN_100K

// SN32复位
#define GPIO_SN32_INT GPIO_PD0
#define PD0_INPUT_ENABLE 0
#define PD0_OUTPUT_ENABLE 1
#define PD0_DATA_OUT 0
#define PULL_WAKEUP_SRC_PD0 PM_PIN_PULLDOWN_100K

// 片选信号
#define SPI_PIN_CS GPIO_PD6
#define PD6_INPUT_ENABLE 0
#define PD6_OUTPUT_ENABLE 1
#define PD6_DATA_OUT 1
#define PULL_WAKEUP_SRC_PD6 PM_PIN_PULLUP_10K

//////////////////////////////////////////////////
// protocol ble attr section start ==>
#define PROTOCOL_SERVICE_UUID 0xeae0
//#define PROTOCOL_TX_UUID 0xeae2
//#define PROTOCOL_RX_UUID 0xeae1
// protocol ble attr section end <==
//////////////////////////////////////////////////

#if (BATT_CHECK_ENABLE)
//telink device: you must choose one gpio with adc function to output high level(voltage will equal to vbat), then use adc to measure high level voltage
//use PB7 output high level, then adc measure this high level voltage
#define GPIO_VBAT_DETECT GPIO_PB1
#define PB1_FUNC AS_GPIO
#define PB1_INPUT_ENABLE 0
#define ADC_INPUT_PCHN B1P //corresponding  ADC_InputPchTypeDef in adc.h
#endif

#endif

#endif
