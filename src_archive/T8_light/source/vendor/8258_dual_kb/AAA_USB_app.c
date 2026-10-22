/*
 * @Descripttion: 
 * @version: 
 * @Author: xdhuang
 * @Date: 2021-05-20 13:31:09
 * @LastEditors: xdhuang
 * @LastEditTime: 2021-05-28 10:14:39
 * @Copyright: Hefei Madao Information Technology Co., Ltd
 */

#include "AAA_public_config.h"
#include "logic_protocol_manager.h"
#include "keyboard_scan.h"
#include "../../third/voice/voice_process.h"
#include "audio_manager.h"
#include "OTA_manager.h"
#if MODULE_USB_ENABLE
#include "USB_manager.h"
_attribute_ram_code_ void irq_handle_usb_timer()
{
	unsigned int irq_src = irq_get_src();
	unsigned short rf_irq_src = rf_irq_src_get();

	if (irq_src & FLD_IRQ_TMR1_EN) //  1ms ok
	{
		if (reg_tmr_sta & FLD_TMR_STA_TMR1)
		{
			reg_tmr_sta = FLD_TMR_STA_TMR1; //clear irq status
		}
	}
}

void go_to_wireless_mode()
{
	write_deep_ana0(DEEP_SLEEP_ANA_AAA);
	analog_write(DEEP_ANA_REG1, 0);
	flash_dev_info.mode = RF_1M_BLE_MODE;
	save_dev_info_flash();
	usb_resume_host();/////////NEW
	start_reboot();
}

extern void Set_web_flag();
static u8 USB_CMD1 = 0;
static u8 usbmode_to_blemode_flag = 0;
static u32 usbmode_to_blemode_tick = 0;
static u8 data_out_notify_flag = 0;
static u8 usbmode_to_blemode_count = 0;
static u8 set_sonix_sleep_usb = 1;
static u32 set_sonix_sleep_usb_tick = 0;

static void usbmode_to_blemode()
{
	if (USB_INSERT_FLAG)
		USB_CMD1 = 1;
	if (USB_CMD1)
	{
		if(charging_flag)
		{
			usbmode_to_blemode_count = 0;
			if(usb_has_suspend_irq)
			{
				if(set_sonix_sleep_usb && clock_time_exceed(set_sonix_sleep_usb_tick, 1*1000*1000))// 1s的延时消抖
				{
					printf("SLEEP\n");
					if(sonix_keyboard_instance()->set_mcu_mode(MCU_SLEEP))
						set_sonix_sleep_usb = 0;
				}
				if(data_out_notify_flag >= 2)
				{
					write_deep_ana0(DEEP_SLEEP_ANA_AAA);
					usb_resume_host();
					sleep_ms(10);
					usb_dp_pullup_en (0);
					sleep_ms(1000);
					usb_dp_pullup_en (1);
				}
			}
			else {
				ble_indicator_light_on();
				if(set_sonix_sleep_usb == 0)
				{
//					printf("OPLI%d\n",sonix_keyboard_instance()->kleds.brightness);
					set_sonix_sleep_usb = 1;
					u32 set_sleep_usb_tick = clock_time();
					while(!sonix_keyboard_instance()->get_sonix_param(GET_LEDS_BRIGHTNESS))
					{
						printf("G L F\n");
						if(clock_time_exceed(set_sleep_usb_tick, 60*1000*1000))// 如果一分钟都没有设置成功，就重启
							start_reboot();
					}
				}
				if (data_out_notify_flag)
				{
					data_out_notify_flag = 0;
				}
				set_sonix_sleep_usb_tick = clock_time();
			}
		}
		else {
			if (clock_time_exceed(usbmode_to_blemode_tick, 10000))// 增加时间，解决拔出usb后键盘未完全掉电导致转换到ble模式的问题
			{
				usbmode_to_blemode_tick = clock_time();
				usbmode_to_blemode_count++;
			}
			if(usbmode_to_blemode_count > 5)
			{
				go_to_wireless_mode();
			}

		}
	}
	else//解决USB充电被误认为有线接入
	{
		if (clock_time_exceed(usbmode_to_blemode_tick, 100000))
		{
			usbmode_to_blemode_tick = clock_time();
			usbmode_to_blemode_flag++;
		}
		if(charging_flag)
		{
			if (usbmode_to_blemode_flag > 50)
			{
				go_to_wireless_mode();
			}
		}
		else {
			if (usbmode_to_blemode_flag > 5)
			{
				go_to_wireless_mode();
			}
		}

	}
}

u8 caps_flag;
static void loop_capslock_led()
{
	if(caps_flag != host_keyboard_status)
	{
		led_caps_set(host_keyboard_status & 2);
		analog_write(DEEP_ANA_REG1, host_keyboard_status);
		caps_flag = host_keyboard_status;
	}
}

static u32 blemode_to_usbmode_tick = 0;
static u8 blemode_to_usbmode_flag = 0;
void blemode_to_usbmode()
{
	if (clock_time_exceed(blemode_to_usbmode_tick, 5000))
	{
		blemode_to_usbmode_tick = clock_time();
		blemode_to_usbmode_flag++;
	}
//	if(reg_ctrl_ep_irq_sta)
//		printf("ccc : %x\r\n", reg_ctrl_ep_irq_sta);
	if (reg_ctrl_ep_irq_sta & FLD_CTRL_EP_IRQ_SETUP)
	{
		if (blemode_to_usbmode_flag > 5)
		{
			write_deep_ana0(DEEP_SLEEP_ANA_AAA);
			analog_write(DEEP_ANA_REG1, 0);
			flash_dev_info.mode = USB_MODE;
			save_dev_info_flash();
			usb_resume_host();
			printf("ble to usb\n");
			start_reboot();
//			while(1);
		}
	}
	else
	{
		blemode_to_usbmode_flag = 0;
	}
}

static void _set_keyboard_up_usb(u8 state)
{
	if(state)
	{
		u8 data[15] = {0};
		push_data_out_fifo(data, OUT_KEYBOARD_REPORT, 14);
//		u8 up_data[15]={0};
//		if(keybord_down_flag > 0)
//		{
//			keybord_down_flag ++;
//			if(keybord_down_flag > 2 && check_data_out_fifo_empty())
//			{
//				if(push_data_out_fifo(up_data, OUT_KEYBOARD_REPORT, 15) && push_data_out_fifo(up_data, OUT_CONSUMER_REPORT, 2))
//					keybord_down_flag = 0;
//			}
//		}
	}
//	else keybord_down_flag = 1;

}

extern void _keyboard_scan_cb_func(u8 *data, u8 size);

u8 usb_notify_data_proc(u16 type, u8 *data, u32 size)
{
	switch(type)
	{
		case OUT_KEYBOARD_REPORT:
			data_out_notify_flag ++;
			return usb_keyboard_hid_report(data, size);
		case OUT_CONSUMER_REPORT:
			data_out_notify_flag ++;
			return usb_keboard_consumer_report(data, size);
		case OUT_PROT_REPORT:
			if(web_flag & (1 << ble_adv_channel))
				return madao_interface_report(data, size);
		case OUT_SPP_REPORT:
			return open_the_website(data, size);
		default: return 1;
	}
	return 1;
}
static u32 keyscan_loop_cycle;
void usb_user_init()
{
	//enable USB manual interrupt(in auto interrupt mode,USB device would be USB printer device)
	vMblueIdentificationInit();
	random_generator_init(); //this is must
	usb_init_interrupt();
	//usb_init();
	usb_set_pin_en();
	mblue_fifo_construct(&scan_data_fifo, fifo_buffer, 128);
	scan_manager_instance()->init(_keyboard_scan_cb_func, _set_keyboard_up_usb);
	data_out_fifo_init(usb_notify_data_proc);
#if (ENCODER_TYPE == 1)
	codec_init();
#endif

	memcpy(keyboard_mac_address, (u8 *)&user_cfg.dev_mac, 6);
	keyboard_mac_address[4] = MACADDR_CHANNEL;
	keyboard_mac_address[5] = MACADDR_AUTH_TYPE;

	REG_ADDR8(0x74) = 0x53;
	REG_ADDR16(0x7e) = 0x08d0;
	REG_ADDR8(0x74) = 0x00;
	//reg_irq_mask |= FLD_IRQ_USB_PWDN_EN;

#if (USB_MADAO_ENABLE)
	reg_usb_ep1_buf_addr = 0x00; //keyboard in, size: 15
	reg_usb_ep2_buf_addr = 0x08; //mouse in, size: 8
	reg_usb_ep3_buf_addr = 0x16; //primary in, size: 64(0x40)
	reg_usb_ep6_buf_addr = 0x56; //primary out, size: 64(0x40)
	reg_usb_ep8_buf_addr = 0x98; //printer in, size: 8
	reg_usb_ep5_buf_addr = 0xA0; //printer out, size: 8
	reg_usb_ep7_buf_addr = 0xA8;
#else
	reg_usb_ep6_buf_addr = 0x80;
	reg_usb_ep7_buf_addr = 0x60;
#endif
	reg_usb_ep_max_size = (256 >> 3);

	reg_usb_iso_mode &= 0x80;
	reg_usb_ep6_ctrl = 0x01;

	web_data_usb_init();
	//usb_dp_pullup_en(1); //open USB enum
	ble_adv_channel = CHANNEL_USB;
	bls_pm_setSuspendMask(SUSPEND_DISABLE);
	authenticate_success_flag = ANALOG_READ(AUTHENTICATION_REG,1);
	if(authenticate_success_flag)
		Set_web_flag();
	batt_vol_mv_real = 5000;

	keyscan_loop_cycle = clock_time();
}

extern u8 receiver_buffer_length;
extern u8 primary_recvData_buf[64];
void usb_mode_loop()
{
	usb_handle_irq();

#if (BLE_AUDIO_ENABLE1)
	dealwith_protocol_cmd();
	if (ui_mic_enable)
	{
		audio_dispatch();
	}
#endif
	check_charging_status();

	if(clock_time_exceed(keyscan_loop_cycle, 1 * 1000))
	{
		scan_manager_instance()->loop_scan();
		keyscan_loop_cycle = clock_time();
	}

	whlee_monitor_pro();

	keyboard_manager_proc();
#if BLT_APP_LED_ENABLE
	led_hw_proc();
#endif
	usbmode_to_blemode();

	if (receiver_buffer_length > 0)
	{
//		printf("usb:%d\r\n", receiver_buffer_length);
		Set_web_flag();
		//get_logic_protocol_manager()->parse(primary_recvData_buf, receiver_buffer_length);
		get_logic_protocol_manager()->parse(primary_recvData_buf, receiver_buffer_length);
		receiver_buffer_length = 0;
	}

	usb_report_web_addr();
#if BLT_APP_LED_ENABLE
	loop_capslock_led();
#endif
	data_out_loop_proc();
#if SINCHECK_PCM_DATA_ENABLE
    sincheck_loop();
#endif
#if PROTOCOL_OTA_ENABLE
	ota_loop_proc();
#endif
}

#endif
