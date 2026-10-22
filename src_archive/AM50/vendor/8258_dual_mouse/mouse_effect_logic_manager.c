#include "../../common/string.h"
#include "common/types.h"
#include "mouse_effect_logic_manager.h"
#include "../../drivers/8258/flash.h"
#include "../../third/crc/mblue_crc.h"
//#include "../../application/print/u_printf.h"
#include "AAA_public_Config.h"
#if PWM_LED_ENABLE
#include "RGB/RGB_config_file.h"
#endif
#define MAX_DATA_DEVICE_SIZE (32)
#define VERSION_LEN 4
#define NULL_BYTE_FLAG (0xff)
extern u32 byteToInt(u8 *data, int len);
extern void vibration_clientware_set();
u8 flash_write_flag1 = 0;
u8 flash_write_flag2 = 0;
/**
 * @brief  获取当前可写入数据的flash的开始偏移量地址
 * @note
 * @retval
 */
#define FLASH_MAX_WRITE_SIZE 16
#define FLASH_MAX_READ_SIZE 64
#define MAX_LEN 4096
static void md_flash_read_page(unsigned long addr, unsigned long len, unsigned char *buf){

        if(len <= FLASH_MAX_READ_SIZE)
        {
                flash_read_page(addr, len, buf);
                return;
        }

        unsigned char *p = buf;
        int num_package = (len + FLASH_MAX_READ_SIZE - 1) / FLASH_MAX_READ_SIZE;
        for(int i=0; i<num_package; i++)
        {
        	if (i == (num_package - 1))
        	{
        		flash_read_page(addr + i * FLASH_MAX_READ_SIZE,len - FLASH_MAX_READ_SIZE * i, p);
        	}
        	else
        	{
                flash_read_page(addr + i * FLASH_MAX_READ_SIZE, FLASH_MAX_READ_SIZE, p);
//              sleep_us(10);
                p += FLASH_MAX_READ_SIZE;
        	}
        }
}

static void md_flash_write_page(unsigned long addr, unsigned long len, unsigned char *buf){

        if(len <= FLASH_MAX_WRITE_SIZE)
        {
                flash_write_page(addr, len, buf);
                return;
        }

        unsigned char *p = buf;
        int num_package = (len + FLASH_MAX_WRITE_SIZE - 1) / FLASH_MAX_WRITE_SIZE;
        for(int i=0; i<num_package; i++)
        {
        	if (i == (num_package - 1))
        	{
        		flash_write_page(addr + i*FLASH_MAX_WRITE_SIZE,len - FLASH_MAX_WRITE_SIZE*i, p);
        	}
        	else
        	{
                flash_write_page(addr + i*FLASH_MAX_WRITE_SIZE, FLASH_MAX_WRITE_SIZE, p);
    //          sleep_us(10);
                p += FLASH_MAX_WRITE_SIZE;
        	}

        }
}
static u32 writable_addr(u32 flash_sector_address)
{
	mouse_propertie_param_arrays device_info;
	u32 offset_addr = 0;
	{
		for (u32 i = 0; i < MAX_DATA_DEVICE_SIZE; i++)
		{
			offset_addr = i * sizeof(mouse_propertie_param_arrays);
			md_flash_read_page(flash_sector_address + offset_addr, 1, (u8 *)&device_info.mouse_flag);
			if (device_info.mouse_flag == NULL_BYTE_FLAG)
			{
				return offset_addr;
			}
		}
		return MAX_LEN;
	}
}
static u32 get_writable_addr(u32 flash_sector_address,u32 flash_sector_address1)
{
//	mouse_propertie_param_arrays device_info,device_info1;
	if(flash_write_flag1 == 0 && flash_write_flag2 == 0)
	{
		u32 offset_addr = writable_addr(flash_sector_address);
		u32 offset_addr1 = writable_addr(flash_sector_address1);
		if(offset_addr == MAX_LEN && offset_addr1 == MAX_LEN)//两扇区都满   1  3 3
		{
			return offset_addr + offset_addr1;
		}
		if(offset_addr1 == 0)//2扇区空           3   1 1   2 1   3 1
		{
			flash_write_flag1 = 1;
			flash_write_flag2 = 0;
			return offset_addr;
		}
		else if(offset_addr>0 && offset_addr1 == MAX_LEN)//2扇区满 1扇区有数据不满          1    2 3
		{
			flash_write_flag1 = 1;
			flash_write_flag2 = 0;
			flash_need_erase_flag2 = 1;
			return offset_addr;
		}
		else if(offset_addr == MAX_LEN && offset_addr1 > 0)//1扇区满 2扇区有数据不满           1   3 2
		{
			flash_write_flag1 = 0;
			flash_write_flag2 = 1;
			flash_need_erase_flag1 = 1;
			return offset_addr1;
		}
		else if((offset_addr > 0 && offset_addr < MAX_LEN) && (offset_addr1 > 0 && offset_addr1 < MAX_LEN)) //  1 2 2
		{
			return MAX_LEN * 3;
		}
		else										// 2      1 2  1 3
		{
			flash_write_flag1 = 0;
			flash_write_flag2 = 1;
			return offset_addr1;
		}
	}
	if(flash_write_flag1)
	{
		u32 offset_addr = writable_addr(flash_sector_address);
		if (offset_addr < MAX_LEN)
		{
			return offset_addr;
		}
		else//当前扇区已写满
		{
			flash_need_erase_flag1 = 1;
			flash_write_flag1 = 0;
			flash_write_flag2 = 1;
		}
	}

	if(flash_write_flag2)
	{
		u32 offset_addr = writable_addr(flash_sector_address1);
		if (offset_addr < MAX_LEN)
		{
			return offset_addr;
		}
		else//当前扇区已写满
		{
			flash_need_erase_flag2 = 1;
			flash_write_flag1 = 1;
			flash_write_flag2 = 0;

			u32 offset_addr = writable_addr(flash_sector_address);
			return offset_addr;
		}
	}

}


/**
 * @brief  将结构体数据写入flash中 ,addr:flash地址
 * @note
 * @retval
 */
static void set_valid_info_to_flash(mouse_propertie_param_arrays *device, u32 addr)
{
	device->version++;
	device->valid = calculate_crc32(&device->version, VERSION_LEN);
	md_flash_write_page(addr, sizeof(mouse_propertie_param_arrays), (u8 *)device);
}

/**
 * @brief  判断两个flash中的数据，将最新的有效数据赋给mouse_propertie_param
 * @note
 * @retval
 */
static void get_valid_info(mouse_propertie_param_arrays *device_info)//, mouse_propertie_param_arrays *device_info1
{

	if (device_info->valid == calculate_crc32(&device_info->version, VERSION_LEN) )
	{
		memcpy(&mouse_propertie_param, device_info, sizeof(mouse_propertie_param_arrays));
	}
	else //两个扇区的数据都有问题或者都为空
	{
		/////////////////////motor/////////////////////////////////////
		mouse_propertie_param.conn_vibration_param.on_off = 1;
		mouse_propertie_param.conn_vibration_param.on_time = 100;
		/////////////////////side button//////////////////////////////
		mouse_propertie_param.side_button1_param.flag = 2; //0:默认功能; 1:标准键盘按键; 2:自定义功能转化按键（上位机语音翻译，转写等）
		mouse_propertie_param.side_button2_param.flag = 2; //0:默认功能; 1:标准键盘按键; 2:自定义功能转化按键（上位机语音翻译，转写等）
		mouse_propertie_param.side_button1_param.data[0] = MS_BTN_K5;
		mouse_propertie_param.side_button2_param.data[0] = MS_BTN_K4;
		/////////////////////rgb//////////////////////////////////////
		mouse_propertie_param.audio_RGB_param.rgb_data.rgb_red = 0;
		mouse_propertie_param.audio_RGB_param.rgb_data.rgb_green = 255;
		mouse_propertie_param.audio_RGB_param.rgb_data.rgb_blue = 255;

		mouse_propertie_param.RGB_theme = RGB_CLOSE_MODE;
		mouse_propertie_param.breathe_RGB_param.cycle_time = 3000;
		mouse_propertie_param.breathe_RGB_param.rgb_data_len = 1;
		mouse_propertie_param.breathe_RGB_param.rgb_data[0].rgb_red = 0;
		mouse_propertie_param.breathe_RGB_param.rgb_data[0].rgb_green = 255;
		mouse_propertie_param.breathe_RGB_param.rgb_data[0].rgb_blue = 0;
	}
}

/**
 * @brief  判断两个扇区的数据，并将数据写入到对应的扇区
 * @note	若扇区没有数据或读取的数据错误，则将数据写入到该扇区，若两个扇区都有数据，且数据正确，则将数据写入到数据版本低的扇区
 * @retval
 */
static void set_valid_info(mouse_propertie_param_arrays *device_info,  mouse_propertie_param_arrays *device)//mouse_propertie_param_arrays *device_info1,
{

	if(flash_write_flag1)
	{
		set_valid_info_to_flash(device, MOUSE_DEVICE_DATA_ADDR + device_info->offset_addr);
		return;
	}

	if(flash_write_flag2)
	{
		set_valid_info_to_flash(device, MOUSE_DEVICE_DATA_ADDR1 + device_info->offset_addr);
		return;
	}
}

/**
 * @brief  初始化mouse_propertie_param的值（从flash读）
 * @note
 * @retval
 */
void init_mouse_device_data()
{
	flash_need_erase_flag1 = 0;
	flash_need_erase_flag2 = 0;
	mouse_propertie_param_arrays device_info, device_info1;//
	u32 offset_addr = get_writable_addr(MOUSE_DEVICE_DATA_ADDR,MOUSE_DEVICE_DATA_ADDR1);
//	u32 offset_addr1 = get_writable_addr(MOUSE_DEVICE_DATA_ADDR1);
	if(offset_addr >= 2*MAX_LEN)
	{
		if(offset_addr == 2*MAX_LEN)
		{
			md_flash_read_page(MOUSE_DEVICE_DATA_ADDR + MAX_LEN -sizeof(mouse_propertie_param_arrays), sizeof(mouse_propertie_param_arrays), (u8 *)&device_info);
			md_flash_read_page(MOUSE_DEVICE_DATA_ADDR1 + MAX_LEN -sizeof(mouse_propertie_param_arrays), sizeof(mouse_propertie_param_arrays), (u8 *)&device_info1);
		}
		else
		{
			u32 offset_addr = writable_addr(MOUSE_DEVICE_DATA_ADDR);
			u32 offset_addr1 = writable_addr(MOUSE_DEVICE_DATA_ADDR1);
			md_flash_read_page(MOUSE_DEVICE_DATA_ADDR + offset_addr -sizeof(mouse_propertie_param_arrays), sizeof(mouse_propertie_param_arrays), (u8 *)&device_info);
			md_flash_read_page(MOUSE_DEVICE_DATA_ADDR1 + offset_addr1 -sizeof(mouse_propertie_param_arrays), sizeof(mouse_propertie_param_arrays), (u8 *)&device_info1);
		}
		if (device_info.version > device_info1.version)
		{
			flash_write_flag1 = 1;
			flash_write_flag2 = 0;
			flash_need_erase_flag2 = 1;
			get_valid_info(&device_info);
		}
		if(device_info.version < device_info1.version)
		{
			flash_write_flag1 = 0;
			flash_write_flag2 = 1;
			flash_need_erase_flag1 = 1;
			get_valid_info(&device_info1);
		}
		return;
	}
	else if(offset_addr)
	{
		if (flash_write_flag1)
		{
			md_flash_read_page(MOUSE_DEVICE_DATA_ADDR + offset_addr - sizeof(mouse_propertie_param_arrays), sizeof(mouse_propertie_param_arrays), (u8 *)&device_info);
		}
		if (flash_write_flag2)
		{
			md_flash_read_page(MOUSE_DEVICE_DATA_ADDR1 + offset_addr - sizeof(mouse_propertie_param_arrays), sizeof(mouse_propertie_param_arrays), (u8 *)&device_info);
		}
	}
	get_valid_info(&device_info);//&device_info1
}

/**
 * @brief  向flash写数据
 * @note 若flash中最后一次写入的custom_param_arrays数据与当前custom_param_arrays数据不一致，则在后追加一组custom_param_arrays数据，
 * 若扇区写满则清空扇区重新写入一组
 * @retval
 */
u8 updata_mouse_device_data(mouse_propertie_param_arrays *device)
{
	mouse_propertie_param_arrays device_info, device_info1;
	u32 offset_addr = get_writable_addr(MOUSE_DEVICE_DATA_ADDR,MOUSE_DEVICE_DATA_ADDR1);
	if(offset_addr == MAX_LEN)
	{
		return 0;
	}
//	u32 offset_addr1 = get_writable_addr(MOUSE_DEVICE_DATA_ADDR1);
	if (offset_addr)
	{
		if(flash_write_flag1)
		{
			md_flash_read_page(MOUSE_DEVICE_DATA_ADDR + offset_addr - sizeof(mouse_propertie_param_arrays), sizeof(mouse_propertie_param_arrays), (u8 *)&device_info);
			if (memcmp(&device_info, device, sizeof(mouse_propertie_param_arrays)) == 0)
				return 1;
		}
		if (flash_write_flag2)
		{
			md_flash_read_page(MOUSE_DEVICE_DATA_ADDR1 + offset_addr - sizeof(mouse_propertie_param_arrays), sizeof(mouse_propertie_param_arrays), (u8 *)&device_info1);
			if (memcmp(&device_info1, device, sizeof(mouse_propertie_param_arrays)) == 0)
				return 1;
		}
	}


	device_info.offset_addr = offset_addr;
//	device_info1.offset_addr = offset_addr1;

	set_valid_info(&device_info,  device);//&device_info1,
	return 1;
}

setting_rgb_data rgb_param[5] = {{0,255,255},{255,0,0},{0,255,0},{255,0,255},{255,255,0}};//冰蓝色、时尚红、青春绿、活力紫、热情黄
setting_rgb_data get_rgb_value_by_index(u8 index)
{
	return rgb_param[index];
}

u8 get_index_by_rgb_value(setting_rgb_data rgb_value)
{
	for(int i = 0; i < 5; i++)
	{
		if((rgb_param[i].rgb_blue == rgb_value.rgb_blue) && (rgb_param[i].rgb_red == rgb_value.rgb_red) && (rgb_param[i].rgb_green == rgb_value.rgb_green))
			return i;
	}
	return 0;
}

void set_rgb_param(u8 *data, u8 data_len)
{
	u8 rgb_type = *data;
	u32 value = byteToInt(data + 1, 4);
	if(ui_mic_enable == 0 && lowbatt_flag == 0 && charging_batt_count == 0)
	{
		rgb_front()->set_single_colour(rgb_front(), close_all_rgb);
		rgb_back()->set_single_colour(rgb_back(), close_all_rgb);
	}
	switch (rgb_type)
	{
	case 0:
		mouse_propertie_param.RGB_theme = RGB_CLOSE_MODE;
		break;
	case 1: //自定义颜色
		mouse_propertie_param.RGB_theme = RGB_NORMAL_MODE;
		memcpy(&mouse_propertie_param.userdefined_RGB_param.rgb_data, data + 5, 3);
		mouse_propertie_param.RGB_index = get_index_by_rgb_value(mouse_propertie_param.userdefined_RGB_param.rgb_data);
		break;
	case 2: //标准颜色
		memcpy(&mouse_propertie_param.audio_RGB_param.rgb_data, data + 5, 3);
		break;
	case 3:
		mouse_propertie_param.RGB_theme = RGB_BREATH_MODE;
		memset(&mouse_propertie_param.breathe_RGB_param, 0x00, sizeof(mouse_propertie_param.breathe_RGB_param));
		mouse_propertie_param.breathe_RGB_param.rgb_data_len = (data_len - sizeof(rgb_type) - sizeof(value));
		mouse_propertie_param.breathe_RGB_param.cycle_time = value;
		memcpy(&mouse_propertie_param.breathe_RGB_param.rgb_data, data + 5, mouse_propertie_param.breathe_RGB_param.rgb_data_len);
		mouse_propertie_param.breathe_RGB_param.rgb_data_len = (mouse_propertie_param.breathe_RGB_param.rgb_data_len / sizeof(setting_rgb_data));
		break;
	case 4:
		mouse_propertie_param.RGB_theme = RGB_TRIGGER_MODE;
		memset(&mouse_propertie_param.breathe_RGB_param, 0x00, sizeof(mouse_propertie_param.breathe_RGB_param));
		mouse_propertie_param.breathe_RGB_param.cycle_time = value;
		mouse_propertie_param.breathe_RGB_param.rgb_data_len = 1;
		memcpy(&mouse_propertie_param.breathe_RGB_param.rgb_data, data + 5, 3);
		mouse_propertie_param.RGB_index = get_index_by_rgb_value(mouse_propertie_param.breathe_RGB_param.rgb_data[0]);
		if(ui_mic_enable == 0 && lowbatt_flag == 0 && charging_batt_count == 0)
		{
			rgb_back()->set_gradient_colour(rgb_back(), BREATHING_PRIORITY, 1, 0, mouse_propertie_param.breathe_RGB_param.cycle_time,
					(rgb_data *)mouse_propertie_param.breathe_RGB_param.rgb_data, mouse_propertie_param.breathe_RGB_param.rgb_data_len);
			rgb_front()->set_gradient_colour(rgb_front(), BREATHING_PRIORITY, 1, 0, mouse_propertie_param.breathe_RGB_param.cycle_time,
					(rgb_data *)mouse_propertie_param.breathe_RGB_param.rgb_data, mouse_propertie_param.breathe_RGB_param.rgb_data_len);
		}
		break;
	default:
		break;
	}
	mouse_effect_save_flag = 1;
//	updata_mouse_device_data(&mouse_propertie_param);
}

void set_side_button_param(u8 *data, u8 data_len)
{
	u8 button_index = *data;
	switch (button_index)
	{
	case 1:
		memset((u8 *)&mouse_propertie_param.side_button1_param, 0, sizeof(setting_custom_button_info));
		mouse_propertie_param.side_button1_param.flag = *(data + 1);
		mouse_propertie_param.side_button1_param.type = *(data + 2);
		memcpy(mouse_propertie_param.side_button1_param.data, data + 3, data_len - 3);
		break;
	case 2:
		memset((u8 *)&mouse_propertie_param.side_button2_param, 0, sizeof(setting_custom_button_info));
		mouse_propertie_param.side_button2_param.flag = *(data + 1);
		mouse_propertie_param.side_button2_param.type = *(data + 2);
		memcpy(mouse_propertie_param.side_button2_param.data, data + 3, data_len - 3);
		break;
	default:
		break;
	}
	mouse_effect_save_flag = 1;
	//updata_mouse_device_data(&mouse_propertie_param);
}
