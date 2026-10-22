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
#include "../../third/voice/voice_process.h"
#include "audio_manager.h"
#include "battery_manager.h"
#include "mouse_effect_logic_manager.h"
#include "OTA_manager.h"
#include "sensor/sensor_proxy.h"
#if PWM_LED_ENABLE
#include "RGB/RGB_config_file.h"
#endif
#if MODULE_USB_ENABLE
#include "USB_manager.h"
_attribute_ram_code_ void irq_handle_usb_timer()
{
	unsigned int irq_src = irq_get_src();
	//unsigned short rf_irq_src = rf_irq_src_get();

	if (irq_src & FLD_IRQ_TMR1_EN) //  1ms ok
	{
		if (reg_tmr_sta & FLD_TMR_STA_TMR1)
		{
			reg_tmr_sta = FLD_TMR_STA_TMR1; //clear irq status
		}
	}
}

static u8 USB_CMD1 = 0;
static u8 usbmode_to_blemode_flag = 0;
static u8 usbmode_to_blemode_flag1 = 0;
static u32 usbmode_to_blemode_tick = 0;
static u8 data_out_notify_flag = 0;
static void usbmode_to_blemode()
{
	if (USB_INSERT_FLAG)
		USB_CMD1 = 1;
	if (USB_CMD1)
	{
		if(battery_is_charging())
		{
			if(usb_has_suspend_irq)
			{
				if(data_out_notify_flag >= 2)
				{
					write_deep_ana0(DEEP_SLEEP_ANA_AAA);
					usb_resume_host();
					start_reboot();
				}
			}
			else {
				if (ui_mic_enable)
				{
					rgb_front()->set_single_colour(rgb_front(), voice_rgb_by_value(0,255,255));
					rgb_back()->set_single_colour(rgb_back(), voice_rgb_by_value(0,255,255));
					audio_dispatch();
				}
				else
				{
					if(battery_is_full())
					{
						rgb_front()->set_single_colour(rgb_front(), charge_full_rgb);
						rgb_back()->set_single_colour(rgb_back(), charge_full_rgb);
					}
					else
					{
						setting_rgb_data rgb_value = get_rgb_value_by_index(1);
						rgb_front()->set_breathing_colour(rgb_front(), RECHARGE_RGB_PRIORITY, 0xff, 3000, 1000, 1000, (rgb_data *)&rgb_value, 1);
						rgb_back()->set_breathing_colour(rgb_back(), RECHARGE_RGB_PRIORITY, 0xff, 3000, 1000, 1000, (rgb_data *)&rgb_value, 1);
					}
				}
				if (data_out_notify_flag)
				{
					data_out_notify_flag = 0;
				}
			}
		}
		else {
			if (clock_time_exceed(usbmode_to_blemode_tick, 100000))
			{
				usbmode_to_blemode_tick = clock_time();
				usbmode_to_blemode_flag1 ++;
			}
			if(usbmode_to_blemode_flag1 > 5)
			{
				printf("usbmode_to_blemode\r\n");
				write_deep_ana0(DEEP_SLEEP_ANA_AAA);
				flash_dev_info.mode = RF_1M_BLE_MODE;
				save_dev_info_flash();
				usb_resume_host();/////////NEW
				start_reboot();
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
		if(battery_is_charging())
		{
			if (usbmode_to_blemode_flag > 50)
			{
				write_deep_ana0(DEEP_SLEEP_ANA_AAA);
				flash_dev_info.mode = RF_1M_BLE_MODE;
				printf("usbmode_to_blemode\r\n");
				save_dev_info_flash();
				usb_resume_host();
				start_reboot();
			}
		}
		else {
			if (usbmode_to_blemode_flag > 5)
			{
				write_deep_ana0(DEEP_SLEEP_ANA_AAA);
				flash_dev_info.mode = RF_1M_BLE_MODE;
				printf("usbmode_to_blemode\r\n");
				save_dev_info_flash();
				usb_resume_host();
				start_reboot();
			}
		}

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
			flash_dev_info.mode = USB_MODE;
			save_dev_info_flash();
			usb_resume_host();
			printf("ble to usb\n");
			//start_reboot();
			while(1);
		}
	}
	else
	{
		blemode_to_usbmode_flag = 0;
	}
}

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
		case OUT_MOUSE_REPORT:
			data_out_notify_flag ++;
			return usb_mouse_hid_report(data, size);
		default: return 1;
	}
	return 1;
}

void on_timer_wakeup(){
	timer0_set_mode(TIMER_MODE_SYSCLK, 0, CLOCK_SYS_CLOCK_1MS * DEFAULT_USB_FREQUENCY / flash_polling_rate.usb_frequency);
	timer_start(TIMER0);
}

void usb_user_init()
{
	//enable USB manual interrupt(in auto interrupt mode,USB device would be USB printer device)
	vMblueIdentificationInit();
	random_generator_init(); //this is must
	usb_init_interrupt();
	init_mouse_device_data();
	//usb_init();
	usb_set_pin_en();
	data_out_fifo_init(usb_notify_data_proc);
#if (ENCODER_TYPE == 1)
	codec_init();
#endif

	memcpy(mouse_mac_address, (u8 *)&user_cfg.dev_mac, 6);
	mouse_mac_address[4] = MACADDR_CHANNEL;
	mouse_mac_address[5] = MACADDR_AUTH_TYPE;

	REG_ADDR8(0x74) = 0x53;
	REG_ADDR16(0x7e) = 0x08d0;
	REG_ADDR8(0x74) = 0x00;
	reg_irq_mask |= FLD_IRQ_USB_PWDN_EN;

#if (USB_MADAO_ENABLE)
	reg_usb_ep1_buf_addr = 0x00; //keyboard in, size: 8
	reg_usb_ep2_buf_addr = 0x08; //mouse in, size: 8
	reg_usb_ep3_buf_addr = 0x10; //primary in, size: 64(0x40)
	reg_usb_ep6_buf_addr = 0x50; //primary out, size: 64(0x40)
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

	usb_dp_pullup_en(1); //open USB enum
	bls_pm_setSuspendMask(SUSPEND_DISABLE);
	ble_adv_channel = CHANNEL_USB;
	charging_batt_count = 1;
	web_flag = analog_read(DEEP_ANA_REG9);
	batt_vol_mv_real = 5000;
	mos_proxy_get_instance()->set_power_mode(OPERATING_MODE);
	on_timer_wakeup();
	authenticate_success_flag = analog_read(DEEP_ANA_REG1);
	if(authenticate_success_flag)
		Set_web_flag();
}

void get_usb_data_report()
{
#if WHEEL_FUN_ENABLE_AAA
    u32 wheel_prepare_tick;
    wheel_prepare_tick = mouse_wheel_prepare_tick();
#endif

#if BUTTON_FUN_ENABLE_AAA
    if (mode_keycode_flag == OS_TYPE_WINDOWS && !((1 << ble_adv_channel) & web_flag)) //未和语音助手进行交互时，(ble_adv_channel & web_flag)为0
    {
        check_web_process_aaa();
    }
    has_new_key_event |= btn_get_value();
#endif
#if SENSOR_FUN_ENABLE_AAA
	if(mos_proxy_get_instance()->query_motion_status())
	{
		ms_data.x = mos_proxy_get_instance()->query_deltax();
		ms_data.y = mos_proxy_get_instance()->query_deltay();
		has_new_key_event |= SENSOR_DATA_EVENT_AAA;
	}
#endif
#if WHEEL_FUN_ENABLE_AAA
    has_new_key_event |= wheel_get_value(wheel_prepare_tick);
#endif
    if(has_new_key_event)
    {
    	ms_data.y = -ms_data.y;
    	push_data_out_fifo(&ms_data.btn, OUT_MOUSE_REPORT, sizeof(mouse_data_t));
    	has_new_key_event = 0;
		ms_data.x = 0;
		ms_data.y = 0;
    }
    if(has_web_key_flag)
    {
    	open_the_website();
    	has_web_key_flag = 0;
    }
}

void on_timer_irq_proc(void){
	if(reg_tmr_sta & FLD_TMR_STA_TMR0){
		reg_tmr_sta = FLD_TMR_STA_TMR0; //clear irq status
		get_usb_data_report();
	}
}

extern u8 receiver_buffer_length;
extern u8 primary_recvData_buf[64];
void usb_mode_loop()
{
	usb_handle_irq();
#if (BLE_AUDIO_ENABLE)
	dealwith_protocol_cmd();

#endif
#if BLT_APP_LED_ENABLE
	led_hw_proc();
#endif
	usbmode_to_blemode();

	if (receiver_buffer_length > 0)
	{
		Set_web_flag();
		get_logic_protocol_manager()->parse(primary_recvData_buf, receiver_buffer_length);
		receiver_buffer_length = 0;
	}
    if(mouse_effect_save_flag)
    {
      if(updata_mouse_device_data(&mouse_propertie_param))
      {
    	  mouse_effect_save_flag = 0;
      }
    }
    if(flash_need_erase_flag1)
    {
//		if (bls_ll_requestConnBrxEventDisable()>300)
//		{
//			bls_ll_disableConnBrxEvent();
			flash_erase_sector(MOUSE_DEVICE_DATA_ADDR);
//			bls_ll_restoreConnBrxEvent();
			flash_need_erase_flag1 = 0;
//		}
    }
    if(flash_need_erase_flag2)
     {
// 		if (bls_ll_requestConnBrxEventDisable()>300)
// 		{
// 			bls_ll_disableConnBrxEvent();
 			flash_erase_sector(MOUSE_DEVICE_DATA_ADDR1);
// 			bls_ll_restoreConnBrxEvent();
 			flash_need_erase_flag2 = 0;

// 		}
     }
	usb_report_web_addr();
	data_out_loop_proc();
#if SINCHECK_PCM_DATA_ENABLE
	sincheck_loop();
#endif
#if PROTOCOL_OTA_ENABLE
	ota_loop_proc();
#endif
#if PWM_LED_ENABLE
	rgb_back()->loop(rgb_back(), 1);
	rgb_front()->loop(rgb_front(), 1);
#endif
}

#endif
