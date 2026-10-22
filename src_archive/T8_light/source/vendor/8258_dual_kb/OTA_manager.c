#include "OTA_manager.h"
#include "OTA_Telink_platform_define.h"

struct ota_data_param{
	unsigned char start_ota_flag;
	unsigned char start_tick;
	unsigned int start_addr;
	unsigned int initial_addr;
	unsigned int ota_index;
	unsigned int ota_end_timer;
	unsigned int ota_start_timer;
};
static struct ota_data_param ota_data_info = {0,0,0,0,0,0,0};

unsigned char init_ota_register(){
	unsigned char value_flag_0 = 0;
	unsigned char value_flag_2 = 0;
	ota_data_info.start_addr = 0xff;
	ota_data_info.initial_addr = 0xff;
	ota_data_info.ota_index = 0;
	ota_data_info.start_ota_flag = 0;
	flash_read_program(START_ADDR_0 + START_OFFSET_ADDR, 1, &value_flag_0);
	flash_read_program(START_ADDR_2 + START_OFFSET_ADDR, 1, &value_flag_2);
	ota_data_info.start_tick = 0;
	if (value_flag_0 == VALUE_SECTOR_FLAG && value_flag_2 != VALUE_SECTOR_FLAG)
	{
		ota_data_info.start_addr = START_ADDR_2;
		ota_data_info.initial_addr = START_ADDR_0;
		ota_data_info.start_ota_flag = 1;
	}
	else if (value_flag_2 == VALUE_SECTOR_FLAG && value_flag_0 != VALUE_SECTOR_FLAG)
	{
		ota_data_info.start_addr = START_ADDR_0;
		ota_data_info.initial_addr = START_ADDR_2;
		ota_data_info.start_ota_flag = 1;
	}
	erase_firmware_sector(ota_data_info.start_addr, ota_data_info.ota_index);
	return ota_data_info.start_ota_flag;
}


unsigned char send_ota_data(unsigned char *data, unsigned char len){
	if(data == 0 || len != PART_FIRMWARE_SIZE || ota_data_info.start_ota_flag == 0)
		return 0;

	if(ota_data_info.ota_index == 0)
	{
		if(*(data + START_OFFSET_ADDR) == VALUE_SECTOR_FLAG)
			*(data + START_OFFSET_ADDR) = NULL_BYTE_FLAG;
		else return 0;
	}
	else if(ota_data_info.ota_index < MAX_NUNBER_OF_SECTORS)//该值随着固件存储范围而改变
	{
		erase_firmware_sector(ota_data_info.start_addr, ota_data_info.ota_index);
	}
	unsigned char part_firmwarr_data[PART_FIRMWARE_SIZE] = {0};
	flash_write_program(ota_data_info.start_addr + ota_data_info.ota_index * PART_FIRMWARE_SIZE, PART_FIRMWARE_SIZE, data);
	flash_read_program(ota_data_info.start_addr + ota_data_info.ota_index * PART_FIRMWARE_SIZE, PART_FIRMWARE_SIZE, part_firmwarr_data);
	if (memcmp(part_firmwarr_data, data, PART_FIRMWARE_SIZE) == 0)
	{
		ota_data_info.ota_index ++;
		return 1;
	}
	return 0;
}

unsigned char enable_ota_data(unsigned char en){
	if (en && ota_data_info.start_ota_flag == 1){
		unsigned char data_value = VALUE_SECTOR_FLAG;
		unsigned char value_flag = 0;
		unsigned char data_null = 0;
		unsigned int firmware_size = 0;
		flash_write_program(ota_data_info.start_addr + START_OFFSET_ADDR, 1, &data_value);
		flash_read_program(ota_data_info.start_addr + START_OFFSET_ADDR, 1, &value_flag);
		flash_read_program(ota_data_info.start_addr + TOTAL_FIRMWARE_SIZE, 4, (unsigned char *)&firmware_size);
		if((value_flag == VALUE_SECTOR_FLAG) && (firmware_size <= ota_data_info.ota_index * PART_FIRMWARE_SIZE)
				&& (firmware_size > (ota_data_info.ota_index -1) * PART_FIRMWARE_SIZE)){
			flash_write_program(ota_data_info.initial_addr + START_OFFSET_ADDR, 1, &data_null);
			ota_data_info.ota_end_timer = current_time();
			ota_data_info.start_ota_flag = 0;
			return 1;
		}
	}
	return 0;
}

void ota_loop_proc()
{
	if (ota_data_info.ota_end_timer > 0 && current_time_exceed(ota_data_info.ota_end_timer, 1000*1000))
	{
		ota_data_info.ota_end_timer = 0;
		while(1); //start reboot
	}
}
