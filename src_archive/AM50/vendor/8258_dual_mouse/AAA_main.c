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

_attribute_data_retention_user OUTPUT_DEV_INFO_AAA output_dev_info =
    {
        0,                  //u32 bin_crc;
        {2, 4, 4},          //u8
        0,                  //u8 sensor_type;
        0,                  //u8 sensor_pd1;
        0,                  //u8 sensor_pd2;
        0,                  //u8 sensor_pd3;
        MOUSE_DATA_LEN_AAA, //u8 ms_data_len;
};

#if PWM_LED_ENABLE
unsigned int A_1 = 0;
unsigned int A_2 = 0;
unsigned int A_3 = 0;
_attribute_ram_code_ void pwm_irq_handle(void)
{
    if (reg_pwm_irq_sta & FLD_IRQ_PWM1_FRAME)
    {
        reg_pwm_irq_sta |= FLD_IRQ_PWM1_FRAME;
        A_1++;
    }
    // if (reg_pwm_irq_sta & FLD_IRQ_PWM2_FRAME)
    // {
    //     reg_pwm_irq_sta |= FLD_IRQ_PWM2_FRAME;
    //     A_2++;
    // }
    // if (reg_pwm_irq_sta & FLD_IRQ_PWM3_FRAME)
    // {
    //     reg_pwm_irq_sta |= FLD_IRQ_PWM3_FRAME;
    //     A_3++;
    // }
}
#endif

extern void on_timer_irq_proc();
_attribute_ram_code_ void irq_handler(void)
{
    irq_blt_sdk_handler();
#if PWM_LED_ENABLE
    pwm_irq_handle();
#endif
    on_timer_irq_proc();
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
    if (idx < 0) // no binding
    {
        return idx;
    }
    flash_read_page((u32)(s_addr + idx), len, d_addr);

    if (idx > 4000) //3k, erase flash
    {
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
}

int deepRetWakeUp;

_attribute_ram_code_ int main(void) //must run in ramcode
{
    blc_pm_select_internal_32k_crystal();

    bls_ota_set_fwSize_and_fwBootAddr(188, 0x40000);

    cpu_wakeup_init();


    deepRetWakeUp = pm_is_MCU_deepRetentionWakeup(); //MCU deep retention wakeUp

    gpio_init(!deepRetWakeUp); //analog resistance will keep available in deepSleep mode, so no need initialize again

    clock_init(SYS_CLK_32M_Crystal);

    u32 bin_size = 0;
    flash_read_page(0x18, 4, (u8 *)&bin_size);
    flash_read_page((bin_size - 4), 4, (u8 *)&output_dev_info.bin_crc);
    init_ota_register(1);
    if (!deepRetWakeUp)
    {
        deep_flag = analog_read(DEEP_ANA_REG0);

        user_config_ykq();
        dev_info_idx = flash_info_load_aaa(CFG_DEVICE_MODE_ADDR, (u8 *)&flash_dev_info.dongle_id, SAVE_MAX_IN_FLASH);
        if (dev_info_idx < 0)
        {
            flash_dev_info.mode = RF_1M_BLE_MODE;
            flash_dev_info.mast_id = 0;
            flash_dev_info.slave_mac_addr[0] = 0;
            flash_dev_info.slave_mac_addr[1] = 0;
            flash_dev_info.slave_mac_addr[2] = 0;
            flash_dev_info.slave_mac_addr[3] = 0;
            flash_dev_info.dongle_id = 0;
        }
        else
        {
        	if((flash_dev_info.mode < RF_1M_BLE_MODE) || (flash_dev_info.mode > USB_MODE))
        	{

            	flash_erase_sector(CFG_DEVICE_MODE_ADDR);
                flash_dev_info.mode = RF_1M_BLE_MODE;
                flash_dev_info.mast_id = 0;
                flash_dev_info.slave_mac_addr[0] = 0;
                flash_dev_info.slave_mac_addr[1] = 0;
                flash_dev_info.slave_mac_addr[2] = 0;
                flash_dev_info.slave_mac_addr[3] = 0;
                flash_dev_info.dongle_id = 0;
        	}

        }
        blc_app_loadCustomizedParameters();   //load customized freq_offset cap value
#if DPI_SAVE_FLASH
        flash_dpi_info.idx = flash_info_load_aaa((MOUSE_DPI_ADDR), (u8 *)&flash_dpi_info.bt_dpi, sizeof(flash_dpi_info));
        if (flash_dpi_info.idx < 0)
#endif
        {
            flash_dpi_info.bt_dpi = 1; //default 1200cpi
            flash_dpi_info.ble_time_out = BLE_CONNECT_TIME_OUT;
        }
        else
        {
        	if(flash_dpi_info.bt_dpi < 0)
        	{
                flash_dpi_info.bt_dpi = 1; //default 1200cpi
                flash_dpi_info.ble_time_out = BLE_CONNECT_TIME_OUT;
        	}
        }
        flash_polling_rate.idx = flash_info_load_aaa(MOUSE_POLLING_RATE_ADDR, (u8 *)&flash_polling_rate, sizeof(POLLING_RATE_INFO));
        if(flash_polling_rate.idx < 0)
        {
        	flash_polling_rate.ble_frequency = 125;
        	flash_polling_rate.usb_frequency = DEFAULT_USB_FREQUENCY;
        }
        else
        {
        	if((flash_polling_rate.ble_frequency > 500) || (flash_polling_rate.ble_frequency < DEFAULT_BLE_FREQUENCY))
        		flash_polling_rate.ble_frequency = DEFAULT_BLE_FREQUENCY;
        	if((flash_polling_rate.usb_frequency > DEFAULT_USB_FREQUENCY) || (flash_polling_rate.usb_frequency < DEFAULT_BLE_FREQUENCY))
				flash_polling_rate.usb_frequency = DEFAULT_USB_FREQUENCY;
        }
    }

    hw_init();

    //#if (BATT_CHECK_ENABLE) //battery check must do before OTA relative operation
    //    user_batt_check_init();
    //#endif

    if (flash_dev_info.mode == RF_1M_BLE_MODE)
    {
        rf_drv_init(RF_MODE_BLE_1M);
		user_init_normal();
    }
#if MODULE_USB_ENABLE
    else if (flash_dev_info.mode == USB_MODE) // usb init
    {
        usb_user_init();
    }
#endif
    blc_pm_setDeepsleepRetentionType(DEEPSLEEP_MODE_RET_SRAM_LOW32K);

    irq_enable();
//	if(!deep_flag)
//		all_led_on();
    while (1)
    {
#if (MODULE_WATCHDOG_ENABLE)
        wd_clear(); //clear watch dog
#endif
        if (flash_dev_info.mode == RF_1M_BLE_MODE)
        {
			main_loop();
        }
#if MODULE_USB_ENABLE
        else if (flash_dev_info.mode == USB_MODE) // usb mode  main_loop
        {
			usb_mode_loop();
        }
#endif
    }
}
