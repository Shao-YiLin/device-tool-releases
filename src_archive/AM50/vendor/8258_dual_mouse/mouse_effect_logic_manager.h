/*
 * @Descripttion: 
 * @version: 
 * @Author: hanxu
 * @Date: 2021-04-20 11:21:46
 * @LastEditors: hanxu
 * @LastEditTime: 2021-04-20 14:03:51
 */

#define MOUSE_DEVICE_DATA_ADDR (0x7f000)
#define MOUSE_DEVICE_DATA_ADDR1 (0x7e000)

typedef struct
{
	unsigned char rgb_red;
	unsigned char rgb_green;
	unsigned char rgb_blue;
} setting_rgb_data;

typedef struct
{
	unsigned char rgb_data_len;
	setting_rgb_data rgb_data[7]; //21byte
	unsigned int cycle_time;
} setting_rgb_info;

typedef struct
{
	setting_rgb_data rgb_data;
} setting_rgb_info1;

typedef struct
{
	/**
	 * 0:默认 1：标准按键 2：自定义按键
	 */
	unsigned char flag;
	/**
	 * 0: normal key 1: consume key
	*/
	unsigned char type;
	/**
	 * normal key模式下：8字节的标准键盘协议数据格式;  consume key模式下：2字节的标准键盘
	*/
	unsigned char data[8];
} setting_custom_button_info;

typedef struct
{
	unsigned char on_off;
	unsigned char intensity;
	unsigned int on_time;
} setting_vibration_info;

typedef enum{
	RGB_CLOSE_MODE   = 0, // 关闭
	RGB_NORMAL_MODE  = 1, // 常亮
	RGB_BREATH_MODE  = 3, // 呼吸
	RGB_TRIGGER_MODE = 4  // 触发
}RGB_MODE;
/*
 * setting_rgb_info  26byte
 * setting_rgb_info1  3byte
 * setting_custom_button_info  10byte
 * setting_vibration_info   6byte
 * mouse_propertie_param_arrays  26+6+20+12+12=76byte
 * reserved    128-76=52byte
 */
typedef struct
{
	unsigned char mouse_flag;
	setting_rgb_info breathe_RGB_param;			   // 26byte
	setting_rgb_info1 userdefined_RGB_param;	   // 3byte
	setting_rgb_info1 audio_RGB_param;			   //	3byte
	setting_custom_button_info side_button1_param; // 10byte
	setting_custom_button_info side_button2_param; // 10byte
	setting_vibration_info conn_vibration_param;   // 6byte
	setting_vibration_info audio_vibration_param;  // 6byte
	unsigned char RGB_theme;
	unsigned char RGB_index;
	unsigned char reserved[49];
	unsigned int version;
	unsigned int offset_addr;
	unsigned int valid; //version的crc校验位
} mouse_propertie_param_arrays;
extern mouse_propertie_param_arrays mouse_propertie_param;

void init_mouse_device_data();
u8 updata_mouse_device_data(mouse_propertie_param_arrays *device);
void set_rgb_param(u8 *data, u8 data_len);
void set_side_button_param(u8 *data, u8 data_len);
setting_rgb_data get_rgb_value_by_index(u8 index);
u8 get_index_by_rgb_value(setting_rgb_data rgb_value);
u8 flash_need_erase_flag1;
u8 flash_need_erase_flag2;
