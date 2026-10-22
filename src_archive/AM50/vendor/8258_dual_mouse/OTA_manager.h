/*
 * OTA_manager.h
 *
 *  Created on: 2021-9-27
 *      Author: xdhuang
 */

extern unsigned char init_ota_register(unsigned char index);
extern unsigned char send_ota_data(unsigned char *data, unsigned char len);
extern unsigned char enable_ota_data(unsigned char en);
void ota_loop_proc();

