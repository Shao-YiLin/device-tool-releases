/*
 * OTA_Telink_platform_define.h
 *
 *  Created on: 2021-12-6
 *      Author: xdhuang
 */

#ifndef OTA_TELINK_PLATFORM_DEFINE_H_
#define OTA_TELINK_PLATFORM_DEFINE_H_

#include "../../drivers/8258/flash.h"
#include "../../common/string.h"
#include "../../drivers/8258/timer.h"
#define START_ADDR_0 0x00000
#define START_ADDR_2 0x20000
#define START_OFFSET_ADDR 0x8
#define VALUE_SECTOR_FLAG 0x4b
#define TOTAL_FIRMWARE_SIZE 0x18
#define NULL_BYTE_FLAG (0xff)
#define THE_SIZE_OF_SINGLE_SECTOR (4096)
#define PART_FIRMWARE_SIZE 32
#define MAX_NUNBER_OF_SECTORS 32

static inline unsigned long current_time()
{
	return clock_time() | 1;
}
static inline unsigned int current_time_exceed(unsigned int ref, unsigned int us)
{
	return ((unsigned int)(clock_time() - ref) > us * 16);
}
static inline void erase_flash_sector(unsigned long add)
{
	return flash_erase_sector(add);
}
static inline void flash_read_program(unsigned long addr, unsigned long len, void *buf)
{
	return flash_read_page(addr, len, (unsigned char *)buf);
}

static inline void flash_write_program(unsigned long addr, unsigned long len, void *buf)
{
	return flash_write_page(addr, len, (unsigned char *)buf);
}

static inline void erase_firmware_sector(unsigned long addr, unsigned long index)
{
	erase_flash_sector(addr + index * THE_SIZE_OF_SINGLE_SECTOR);
}
#endif /* OTA_TELINK_PLATFORM_DEFINE_H_ */
