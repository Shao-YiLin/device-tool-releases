/********************************************************************************************************
 * @file     aaa_main.c
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

#include "AAA_public_config.h"

_attribute_data_retention_user int dev_info_idx;

extern void user_init_normal();
extern void user_init_deepRetn();

extern void main_loop(void);
extern void uart_irq_proc();
u8 sonix_irq_buf = 0;
u8 sonix_irq_enable = 1;
_attribute_ram_code_ void gpio_irq_handle(void)
{
    if (reg_irq_src & FLD_IRQ_GPIO_EN)
    {
//    	printf("[1]SPI_IRQ\n");
    	reg_irq_src |= FLD_IRQ_GPIO_EN;
    	if(sonix_irq_enable)
    	{
    		if (fun_mode == RF_1M_BLE_MODE)
    			sonix_irq_buf = 1;
    		else if(fun_mode == USB_MODE)
    			sonix_keyboard_instance()->sonix_data_scan();// scanning SPI initialization
    	}
    }
}

_attribute_ram_code_ void irq_handler(void)
{
	gpio_irq_handle();
    if (fun_mode == RF_1M_BLE_MODE)
    {
        irq_blt_sdk_handler();
    }
#if MODULE_USB_ENABLE
    else if (fun_mode == USB_MODE)
    {
        irq_handle_usb_timer();
    }
#endif
}

//len must 4 bytes bei beisu  beacuse *(volatile u32 *)(s_addr + idx) must 4 bytes bei beisu
int flash_info_load_aaa(u32 s_addr, u8 *d_addr, u8 len)
{
	int idx;
	u32 buf;
	for (idx = 0; idx < 4096; idx += len)
	{
		flash_read_page((u32)(s_addr + idx), 4, (u8 *)(&buf));
		if (buf == U32_MAX)
		{
			break;
		}
	}
	idx -= len;
	if (idx < 0)
	{ // no binding
		return idx;
	}
	flash_read_page((u32)(s_addr + idx), len, d_addr);

	if (idx > 4000)
	{ //3k, erase flash
		flash_erase_sector((u32)s_addr);

		sleep_us(10);

		flash_write_page((u32)s_addr, len, d_addr);
		idx = 0;
	}

	return idx;
}

void user_config_ykq()
{
	random_generator_init(); //this is must
	flash_read_page(CFG_ADR_MAC, sizeof(custom_cfg_t), (u8 *)&user_cfg.dev_mac);
	if (user_cfg.dev_mac == U32_MAX)
	{
		generateRandomNum(4, (u8 *)&user_cfg.dev_mac);
		flash_write_page(CFG_ADR_MAC, 4, (u8 *)&user_cfg.dev_mac);
	}

	if (user_cfg.paring_tx_power == U8_MAX)
	{
		user_cfg.paring_tx_power = DEFAULT_PAIR_TX_POWER;
	}
	if (user_cfg.tx_power == U8_MAX)
	{
		user_cfg.tx_power = DEFAULT_NORMAL_TX_POWER;
	}
	if (user_cfg.emi_tx_power == U8_MAX)
	{
		user_cfg.emi_tx_power = DEFAULT_EMI_TX_POWER;
	}

	for (u8 i = 0; i < 18; i++)
	{
		if (user_cfg.device_name[i] != 0xff)
		{
			device_name_len++;
			tbl_advData_aaa[13 + i] = user_cfg.device_name[i];
		}
		else
		{
			break;
		}
	}
}

static u8 switch_state_flag = 0;
static u8 empty_array[15] = {0};
static u8 switch_state_count = 0;

static void switch_off_proc()
{
	if(switch_state_flag==0)//0代表开机状态
	{
		sleep_ms(20);
		if(gpio_read(PIN_ON_OFF_STATE))// 关机
		{
			printf("CLOSE\n");
			switch_state_flag = 1;// 关机
			switch_state_count = 0;
			if(battery_is_charging())
			{
				// 发送按键抬起事件，蓝牙会自动处理，所以只处理USB
				if (fun_mode == USB_MODE)
				{
					usb_notify_data_proc(OUT_KEYBOARD_REPORT, empty_array,14);
					usb_notify_data_proc(OUT_CONSUMER_REPORT, empty_array, 2);
				}
				// 关闭主机灯光
				extern void close_all_led();
				close_all_led();
				// 从机RGB灯光亮度变为0
				sonix_keyboard_instance()->kleds.brightness=0;
				sonix_keyboard_instance()->set_rgb_param(SET_LEDS_BRIGHTNESS, sonix_keyboard_instance()->kleds.brightness, FALSE);
				sleep_ms(1);
				if(sonix_keyboard_instance()->get_sonix_param(GET_LEDS_BRIGHTNESS))
					printf("SetSu\n");
				if(sonix_keyboard_instance()->kleds.brightness!=0)
					switch_state_flag = 0;
			}
		}
	}
	else
		check_charging_status();
}

static u32 switch_on_light_tick = 0;
static u8 switch_on_light_flag = 0;
extern void reset_web_flag();

static void switch_on_proc()
{
	if(switch_state_flag==1)// 1代表关机状态
	{
		sleep_ms(20);
		if(!gpio_read(PIN_ON_OFF_STATE))// 开机
		{
			printf("OPEN\n");
			switch_state_flag = 0;// 切换为开机状态
			if(battery_is_charging())// 如果充电器插入开机需要设置灯光
			{
				mouse_wheel_process();// 先清除滚轮编码器缓存
				led_caps_set(caps_flag & 2);// 还原大写锁定灯光
	//			read_rgb_flash(&sonix_keyboard_instance()->kleds);// 读取Flash中的RGB灯光数据
	//			// 打开背光
	//			printf("brightness:%d\n",sonix_keyboard_instance()->kleds.brightness);
	//			u8 br = sonix_keyboard_instance()->kleds.brightness;
	//			sonix_keyboard_instance()->set_rgb_param(SET_LEDS_BRIGHTNESS,sonix_keyboard_instance()->kleds.brightness, FALSE);
	//			sleep_ms(1);
	//			if(sonix_keyboard_instance()->get_sonix_param(GET_LEDS_BRIGHTNESS))
	//				printf("SetSu\n");
	//
	//			if(sonix_keyboard_instance()->kleds.brightness!=br)
	//			{
	//				switch_state_flag = 1;
	//				if(++switch_state_count>5)
	//					start_reboot();
	//			}
			}
			// 开机背光
			sonix_keyboard_instance()->set_rgb_param(SET_LEDS_BRIGHTNESS,3, FALSE);
			sonix_keyboard_instance()->set_rgb_param(SET_LEDS_MODE,0XFF, FALSE);
			sleep_ms(2);
			sonix_keyboard_instance()->get_sonix_param(GET_LEDS_MODE);// 获取模式
			sonix_keyboard_instance()->get_sonix_param(GET_LEDS_BRIGHTNESS);// 获取模式
			switch_on_light_tick = clock_time()|1;
			switch_on_light_flag = 1;
	//		start_reboot();
		}
	}
	else
	{

		if(switch_on_light_tick)// 开机1.5s
		{
			if(clock_time_exceed(switch_on_light_tick, 3 * 500*1000))
			{
				switch_on_light_tick = 0;
				read_rgb_flash(&sonix_keyboard_instance()->kleds);// 读取Flash中的RGB灯光数据
				sonix_keyboard_instance()->set_rgb_param(SET_LEDS_BRIGHTNESS,sonix_keyboard_instance()->kleds.brightness, FALSE);
				sonix_keyboard_instance()->set_rgb_param(SET_LEDS_MODE,sonix_keyboard_instance()->kleds.mode, FALSE);
			}
			else if(switch_on_light_flag == 1)
			{
				if(sonix_keyboard_instance()->kleds.mode != 0xFF || sonix_keyboard_instance()->kleds.brightness != 3)// 确保能打开背光灯
				{
					sonix_keyboard_instance()->set_rgb_param(SET_LEDS_BRIGHTNESS,3, FALSE);
					sonix_keyboard_instance()->set_rgb_param(SET_LEDS_MODE,0xFF, FALSE);
					sleep_ms(2);
					sonix_keyboard_instance()->get_sonix_param(GET_LEDS_MODE);// 获取模式
					sonix_keyboard_instance()->get_sonix_param(GET_LEDS_BRIGHTNESS);// 获取亮度
				}
				else//(sonix_keyboard_instance()->kleds.mode == 0xFF && sonix_keyboard_instance()->kleds.brightness == 3)
				{
					switch_on_light_flag = 0;
				}
			}
			if(sonix_irq_buf)// 接受按键数据避免堵塞通道导致数据错误
			{
				sonix_irq_buf = 0;
				u8 data[34];
				sonix_keyboard_instance()->get_sonix_datas(data);// scanning SPI initialization
			}
		}
	}
}

static u32 led_start_tick = 0;
void all_led_on(){
//	gpio_write(PIN_BLE1_LED , 1);
//	gpio_write(PIN_BLE2_LED , 1);
//	gpio_write(PIN_BLE3_LED , 1);
	gpio_write(PIN_BAT_LED  , 0);
	switch_on_light_tick = clock_time()|1;
	switch_on_light_flag = 1;
	led_start_tick = clock_time();
}
void all_led_off(){
	if(led_start_tick > 0 && clock_time_exceed(led_start_tick, 1000000)){
		led_start_tick = 0;
//		gpio_write(PIN_BLE1_LED , 0);
//		gpio_write(PIN_BLE2_LED , 0);
//		gpio_write(PIN_BLE3_LED , 0);
		gpio_write(PIN_BAT_LED  , 1);
	}
}

int deepRetWakeUp;
static u8 led_on_flag = 0;

_attribute_ram_code_ int main(void) //must run in ramcode
{

	DBG_CHN0_LOW; //debug
	blc_pm_select_internal_32k_crystal();
	//blc_pm_select_external_32k_crystal();

	cpu_wakeup_init();

	deepRetWakeUp = pm_is_MCU_deepRetentionWakeup(); //MCU deep retention wakeUp

	gpio_init(!deepRetWakeUp); //analog resistance will keep available in deepSleep mode, so no need initialize again

#if (SIGNATURE)
	blt_firmware_signature_check();
#endif

	clock_init(SYS_CLK_48M_Crystal);

	if (!deepRetWakeUp)
	{
		deep_flag = ANALOG_READ(DEEP_ANA_REG0,0);

		user_config_ykq();
		dev_info_idx = flash_info_load_aaa(CFG_DEVICE_MODE_ADDR, (u8 *)&flash_dev_info.dongle_id, sizeof(FLASH_DEV_INFO_AAA));
		if (dev_info_idx < 0)
		{
			printf("dev_info_idx\r\n");
			//flash_dev_info.mode = USB_MODE;
			flash_dev_info.mode = RF_1M_BLE_MODE;
			flash_dev_info.mast_id = 0;
			flash_dev_info.slave_mac_addr[0] = 0;
			flash_dev_info.slave_mac_addr[1] = 0;
			flash_dev_info.slave_mac_addr[2] = 0;
			flash_dev_info.slave_mac_addr[3] = 0;
			flash_dev_info.dongle_id = 0;
			//	            save_dev_info_flash();//old
		}
		else
		{
			printf("dev mode :%d\n",flash_dev_info.mode);
		}
		flash_keyboard_param.idx = flash_info_load_aaa(KEYBOARD_PARAM_ADDR, (u8 *)&flash_keyboard_param.numberKro, sizeof(FLASH_KEYBOARD_INFO));
		if(flash_keyboard_param.idx < 0)
		{
			flash_keyboard_param.numberKro = NKRO;
			flash_keyboard_param.wheel_func = AUDIO_MANAGER;
			flash_keyboard_param.sleep_time = AUTO_DEEP_SLEEP_TIME;
		}

		//gpio_write(PIN_FN_LOCK_LED, flash_keyboard_param.fn_lock);
		if (user_cfg.internal_cap != 0xff)
		{
			blc_app_setExternalCrystalCapEnable(1);
		}
		blc_app_loadCustomizedParameters(); //load customized freq_offset cap value

#if IS_SINGLE_GPIO_CHANGE_MODE
		three_switch_mode_change();
#endif

#if (BLE_SNIFF_DEBUG)
		flash_dev_info.mode = RF_1M_BLE_MODE; //for test mode
#endif
		fun_mode = flash_dev_info.mode;
		keyboard_data_mode = flash_keyboard_param.numberKro;
	}

	if (!deepRetWakeUp)
	{
		if (deep_flag == POWER_ON_ANA_AAA)
		{
			power_tick = clock_time() | 1;
		}
		if (deep_flag == MODE_CHANGE_REBOOT_ANA_AAA)
		{
			write_deep_ana0(CLEAR_FLAG_ANA_AAA);
		}
	}
	if((!deep_flag) && (!battery_is_charging()))
		led_on_flag = 1;
	hw_init();
	sonix_keyboard_instance()->init();// scanning SPI initialization

	if (fun_mode == RF_1M_BLE_MODE)
	{
		rf_drv_init(RF_MODE_BLE_1M);
		if (deepRetWakeUp)
		{
			user_init_deepRetn();
		}
		else
		{
			user_init_normal();
		}

	}
#if MODULE_USB_ENABLE
    else if (fun_mode == USB_MODE) // usb init
    {
        usb_user_init();
    }
#endif
	if(led_on_flag)
		all_led_on();
	irq_enable();

	printf("RE\n");

	while (1)
	{
#if (MODULE_WATCHDOG_ENABLE)
		wd_clear(); //clear watch dog
#endif
		if(gpio_read(PIN_ON_OFF_STATE))// 关机
		{
			switch_off_proc();
		}
		else// 开机
		{
			switch_on_proc();

			if (fun_mode == RF_1M_BLE_MODE)
			{
				if(led_start_tick)
					all_led_off();
				else if(switch_on_light_tick == 0)
					main_loop();
			}
#if MODULE_USB_ENABLE
			else if (fun_mode == USB_MODE) // usb mode  main_loop
			{
//				if(led_start_tick)
//					all_led_off();
//				else
				if(switch_on_light_tick == 0)
					usb_mode_loop();
			}
		}
#endif

		deepRetWakeUp = 0;
	}
}
