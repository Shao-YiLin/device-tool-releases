/*
 * sonix_keyboard_manager.c
 *
 *  Created on: 2022-11-14
 *      Author: ylshao
 *
 *
 */
#include "sonix_keyboard_manager.h"
#include "../../third/crc/mblue_crc.h"


/*
 * 通过修改当前数组的个数、顺序定义键盘灯光灯光，修改后请在头文件种改变RGB_MODE_NUM的值
 * Define keyboard lights by changing the number and order of the current array.
 * Change the value of RGB_MODE_NUM in the header file
*/
static rgb_mode keyboardLedsMode[] = {
		LIGHT_NULL,
		COLOR_BREATH,
		TRANSVERSE_SCANNING,
		TRIGGER_SINGLE,
		TRIGGER_SQUARE_SWEEP,
		TRIGGER_TRANSVERSE,
		TRANSVERSE_RANIBOW,
		LONGITUDINAL_RANIBOW,
		SPECTRAL_CYCLE,
		RAINBOW_G,
		RAINBOW_WINDDMILL,
		RAINBOW_RIPPLE,
		DAZZLING_COLORFUL,
		SINE_COLORFUL,
		COLORFUL_RAINDROP,
		COLORFUL_HEAVY_RAINDROP,
		PHANTOM_SCANNING,
		FIVE_COLOR_STACK,
};

// ----------- Flash operation --------
static u16 frgbIndex = 0;

void read_rgb_flash(rgb_param *prgb)
{
	u8 loop_count = 0;

	if(prgb==NULL || frgbIndex<=0 || frgbIndex>MAX_RGB_SIZE)
		return;
	do{
		flash_read_page(SONIX_RGB_INFO_ADDR + (frgbIndex-1)*sizeof(rgb_param), sizeof(rgb_param), (unsigned char *)prgb);
		loop_count++;
	}while((prgb->mode<0||prgb->mode>RGB_MODE_NUM||prgb->speed<0||prgb->speed>3||prgb->brightness<0||prgb->brightness>3)&&loop_count<8);
	if(loop_count>=8)// flash错误
	{
		rgb_param default_prgb = {
				.mode = DEFAULT_MODE,
				.speed = DEFAULT_SPEED,
				.brightness = DEFAULT_BRIGHTNESS,
				.valid_flag = 1
		};
		frgbIndex = MAX_RGB_SIZE+1;// 表示需要擦除Flash
		write_rgb_flash(default_prgb);//写入默认值
	}
//	printf("[R%d]M:%d,S:%d,B:%d\n",frgbIndex,prgb.mode,prgb.speed,prgb.brightness);
}

void write_rgb_flash(rgb_param prgb)
{
	if(prgb.mode<0||prgb.mode>RGB_MODE_NUM||prgb.speed<0||prgb.speed>3||prgb.brightness<0||prgb.brightness>3)
		return;
	if(frgbIndex>=MAX_RGB_SIZE)
	{
		flash_erase_sector(SONIX_RGB_INFO_ADDR);
		frgbIndex = 0;
	}
	flash_write_page(SONIX_RGB_INFO_ADDR + frgbIndex*sizeof(rgb_param), sizeof(rgb_param), (unsigned char *)&prgb);
	frgbIndex++;
//	printf("[W%d]M:%d,S:%d,B:%d\n",frgbIndex,prgb.mode,prgb.speed,prgb.brightness);
}

static void get_frgbIndex()
{
    u32 offset_addr = 0;
    rgb_param prgb;
	frgbIndex = 0;
	for(int i = 0;i<MAX_RGB_SIZE;i++)
	{
		offset_addr = i * sizeof(rgb_param);
		flash_read_page(SONIX_RGB_INFO_ADDR + offset_addr, sizeof(rgb_param), (u8 *)&prgb);
		if (prgb.valid_flag == 0xff)
			break;
		frgbIndex++;
	}
}

/**
 * @brief The manipulation of lights during initialization
 *
 * @param none
 * @return none
 */
static void sonix_flash_init()
{
	rgb_param default_prgb = {
			.mode = DEFAULT_MODE,
			.speed = DEFAULT_SPEED,
			.brightness = DEFAULT_BRIGHTNESS,
			.valid_flag = 1
	};
	// 初始化设置默认灯光
	get_frgbIndex();
	if(frgbIndex == 0)
	{
		write_rgb_flash(default_prgb);
	}
	else
		read_rgb_flash(&default_prgb);
	sonix_keyboard_instance()->kleds.valid_flag = 1;
	// 避免开机白光闪烁，所以初始化的时候灯光模式不同也不重新设置了（白光开启时关机）
	if(!sonix_keyboard_instance()->get_sonix_param(GET_LEDS_MODE))// || default_prgb.mode != sonix_keyboard_instance()->kleds.mode)
	{
		sonix_keyboard_instance()->get_sonix_param(GET_LEDS_MODE);
//		sonix_keyboard_instance()->kleds.mode = default_prgb.mode;
//		sonix_keyboard_instance()->set_rgb_param(SET_LEDS_MODE,sonix_keyboard_instance()->kleds.mode, FALSE);// no need to set flash
	}
	if(!sonix_keyboard_instance()->get_sonix_param(GET_LEDS_SPEED) || default_prgb.speed != sonix_keyboard_instance()->kleds.speed)
	{
		sonix_keyboard_instance()->kleds.speed = default_prgb.speed;
		sonix_keyboard_instance()->set_rgb_param(SET_LEDS_SPEED,sonix_keyboard_instance()->kleds.speed, FALSE);
	}
	if(!sonix_keyboard_instance()->get_sonix_param(GET_LEDS_BRIGHTNESS))// || default_prgb.brightness != sonix_keyboard_instance()->kleds.brightness)
	{
		sonix_keyboard_instance()->get_sonix_param(GET_LEDS_BRIGHTNESS);
//		sonix_keyboard_instance()->kleds.brightness = default_prgb.brightness;
//		sonix_keyboard_instance()->set_rgb_param(SET_LEDS_BRIGHTNESS,sonix_keyboard_instance()->kleds.brightness, FALSE);
	}
//	if (!gpio_read(PIN_ON_OFF_STATE) && default_prgb.brightness != sonix_keyboard_instance()->kleds.brightness)// 开机状态sonix亮度为0
//	{
//		printf("SET RGB,%d,%d\n",sonix_keyboard_instance()->kleds.brightness,default_prgb.brightness);
//		sonix_keyboard_instance()->kleds.brightness = default_prgb.brightness;
//		sonix_keyboard_instance()->set_rgb_param(SET_LEDS_BRIGHTNESS,sonix_keyboard_instance()->kleds.brightness, FALSE);
//	}
	sonix_keyboard_instance()->get_sonix_param(GET_SONIX_VERSION);
}

static u8 _data_response_flag = 0;
static u8 _mcu_mode = 0;
/**
 * @brief read sonix data while IRQ ping is high
 *
 * @param data cmd address
 * @return unsigned char
 */
_attribute_ram_code_ static u8 read_sonix_datas(u8 *data)
{
    unsigned char value = 0;
    unsigned char data_len = 0;

    gpio_write(SPI_PIN_CS,0); //CS level is low
    sleep_us(15);// Requires at least 15us of low level
    reg_spi_ctrl |= FLD_SPI_DATA_OUT_DIS;
	/***when the read_bit was set 1,you can read 0x800008 to take eight clock cycle***/
	reg_spi_ctrl |= FLD_SPI_RD; //enable read,0x09-bit3 : 0 for read ,1 for write
	value = reg_spi_data; //take 8 clock cycles
	while(reg_spi_ctrl & FLD_SPI_BUSY ); //wait reading finished
	gpio_write(SPI_PIN_CS,1);//CS level is high

	/***read data***/
	while(gpio_read(GPIO_SN32_IRQ) && data_len<34)
	{
		gpio_write(SPI_PIN_CS,0); //CS level is low
	    sleep_us(9);// The low signal time before each byte in a set of data is short
		data[data_len++] = reg_spi_data; //take 8 clock cycles
		while(reg_spi_ctrl & FLD_SPI_BUSY ); //wait reading finished
		gpio_write(SPI_PIN_CS,1);//CS level is high

	}

	return data_len;
}

/**
 * @brief write sonix data
 *
 * @param data cmd address
 * @return unsigned char
 */
_attribute_ram_code_ static void write_sonix_datas(u8 *data, u8 data_len)
{
    int i = 0;
    gpio_write(SPI_PIN_CS,0);//CS level is low
    sleep_us(15);// Requires at least 15us of low level
    reg_spi_ctrl &= ~FLD_SPI_DATA_OUT_DIS; //0x09- bit2 enables spi data output
    reg_spi_ctrl &= ~FLD_SPI_RD; //enable write,0x09-bit3 : 0 for read ,1 for write

    /***write Data***/
    for (i = 0; i < data_len; i++) {
    	reg_spi_data = data[i];
        while(reg_spi_ctrl & FLD_SPI_BUSY); //wait writing finished
    }
    /***pull up CS***/
    gpio_write(SPI_PIN_CS,1);//CS level is high
//    printf("-[write]:");
//    for(int i=0;i<data_len;i++)
//    	printf("%x,",data[i]);
//    printf("\n");
}

static void _set_sonix_scan_cycle(void)
{
	u8 sendTime = 0;
	if (fun_mode == RF_1M_BLE_MODE)
	{
		while(!sonix_keyboard_instance()->set_mcu_mode(MCU_BLE))
		{
			sendTime++;
			if(sendTime >= 5)
				break;
			else
				sleep_ms(1);
		}
	}
	else if(fun_mode == USB_MODE)
	{
		while(!sonix_keyboard_instance()->set_mcu_mode(MCU_USB))
		{
			sendTime++;
			if(sendTime >= 5)
				break;
			else
				sleep_ms(1);
		}
	}
}

/**
 * @brief SPI bus and Songhan related initialization settings.
 *
 * @param none.
 * @return none
 */
static void _init(void)
{
	// spi init
	spi_master_init((unsigned char)(CLOCK_SYS_CLOCK_HZ/(2*2000000)-1),SPI_MODE0);
	spi_master_gpio_set(SPI_GPIO_GROUP_A2A3A4D6);

	/* ------------------- sonix module init ---------------------------------- */
	gpio_set_func(GPIO_SN32_IRQ, AS_GPIO);
	gpio_set_input_en(GPIO_SN32_IRQ,1);
	gpio_set_output_en(GPIO_SN32_IRQ,0);
	gpio_setup_up_down_resistor(GPIO_SN32_IRQ,PM_PIN_UP_DOWN_FLOAT);
	gpio_set_interrupt(GPIO_SN32_IRQ, POL_RISING);// Rising edge trigger（高电平）
	/* --------------- end of sonix module init ------------------------------- */
	// 根据当前模式设置从机扫描周期
	_set_sonix_scan_cycle();

	sonix_flash_init();
}

// 获取当前灯光模式的索引
static u8 _get_light_index(u8 mode)
{
	for(int i=0;i<=RGB_MODE_NUM;i++)
		if(mode==keyboardLedsMode[i])
			return i;
	return DEFAULT_MODE;
}

/**
 * @brief Get the sonix data through the SPI bus,when irq came.
 *		     当中断事件来临时，说明从机有数据传输过来，主要是键扫描的结果或者查询的数据响应
 * @param none.
 * @return none
 */

static void _sonix_data_scan()
{
	u8 scan_keycode[14] = {0};	// 14 byte data
	u16 crcReceive = 0;			//
	u8 scan_data[34] = {0};// Maximum size of scan data are 2*17byte 0x00
	u8 data_len = 0;
	u8 type;
	/* ---------------------- deal with scanning event ------------------------- */
	data_len = read_sonix_datas(scan_data);

	if(data_len>0)
	{
		type = scan_data[0];
		if(type == NKRO_KEYCODE)// 全键无冲代码
		{
			u8 ack[2] = {NKRO_KEYCODE,0};
			if(data_len>=17 && data_len<=34)// 小于34防止数据溢出
			{
//				printf("KDT:");
//				for(int i=0;i<data_len;i++)
//					printf("%d,",scan_data[i]);
//				printf("\n");
				if(scan_data[data_len-17] == NKRO_KEYCODE)// 如果后15位有效
				{
					memcpy(scan_keycode,scan_data+(data_len-16),14);// 拷贝有效数据
					crcReceive = (scan_data[data_len-2]<<8) + scan_data[data_len-1];
					u16 crcResult = calculate_crc16(scan_keycode,14);// 对数据进行CRC校验
					printf("(crc)%x,%x\n",crcResult,crcReceive);
					if(crcReceive == crcResult)// crc 校验通过
					{
						scan_manager_instance()->set_scan_data(scan_keycode,14);
					}
					else
					{
						write_sonix_datas(ack,2);
					}

				}
				else// 否则使用前15位有效
				{
					memcpy(scan_keycode,scan_data+1,14);// 直接拷贝数据
					crcReceive = (scan_data[15]<<8) + scan_data[16];
					u16 crcResult = calculate_crc16(scan_keycode,14);// 对数据进行CRC校验
					if(crcReceive == crcResult)// crc校验通过
					{
						scan_manager_instance()->set_scan_data(scan_keycode,14);
					}
					else
					{
						write_sonix_datas(ack,2);
					}
				}
			}
			else// 数据不对
			{
				write_sonix_datas(ack,2);
			}
		}
		else if(type == DATA_RESPONSE)// 数据回复，获取完毕之后会将标志位置1
		{
			u8 response_type = scan_data[1];
//			u8 response_datalen = scan_data[2];// 暂且未使用到数据长度
			u8 *response_data = &scan_data[3];
			switch(response_type)
			{
			default:
			break;
			case GET_LEDS_MODE:// 灯光模式
			{
				printf("[-]MODE:%d\n",response_data[0]);
				if(response_data[0] == ALL_LEDS_WHITE)// 白光
				{
					sonix_keyboard_instance()->kleds.mode = 0xff;
					_data_response_flag = 1;
				}
				else if(response_data[0]>=0 && response_data[0]<=18)// 传输过来的数据范围是0~18
				{
					sonix_keyboard_instance()->kleds.mode = _get_light_index(response_data[0]);
					_data_response_flag = 1;
				}
			}
			break;
			case GET_LEDS_SPEED:// 灯光速度
			{
//				printf("[-]SPEED:%d\n",response_data[0]);
				if(response_data[0]>=0 && response_data[0]<=3)
				{
					sonix_keyboard_instance()->kleds.speed = response_data[0];
					_data_response_flag = 1;
				}
			}
			break;
			case GET_LEDS_BRIGHTNESS:// 灯光亮度
			{
//				printf("[-]BRIGHT:%d\n",response_data[0]);
				if(response_data[0]>=0 && response_data[0]<=3)
				{
					sonix_keyboard_instance()->kleds.brightness = response_data[0];
					_data_response_flag = 1;
				}
			}
			break;
			case GET_SONIX_VERSION:// 固件版本号
			{
//				printf("[-]VERSION\n");
				sonix_keyboard_instance()->sonix_version[0] = response_data[1];
				sonix_keyboard_instance()->sonix_version[1] = response_data[0];
				_data_response_flag = 1;
			}
			break;
			case GET_SINGLE_LEDS:// 指示灯状态
			{
//				printf("[-]VERSION\n");
				sonix_keyboard_instance()->sonix_sled_state[0] = response_data[1];
				sonix_keyboard_instance()->sonix_sled_state[1] = response_data[0];
				_data_response_flag = 1;
			}
			break;
			case SET_MCU_MODE:// 睡眠模式才有返回值
			{
				// 获取此标志之后让整个单片机进入休眠模式
//				printf("[-]MCU\n");
				_mcu_mode = response_data[0];
			}
			break;
			}
		}
	}
	/* ----------------------- end of scanning event --------------------------- */
}

static u8 _get_rgb_param(u8 mode)
{
	u8 scan_data[34] = {0};// Maximum size of scan data are 1+14+1byte 0x00
	u8 data_len = 0;
	u8 type;
	/* ---------------------- deal with scanning event ------------------------- */
	sonix_irq_enable = 0;
	write_sonix_datas(&mode,1);
	u32 write_tick = clock_time();
	// Wait for data return, up to 500us
	while(!gpio_read(GPIO_SN32_IRQ))
	{
		if(clock_time_exceed(write_tick, 500)){
			sonix_irq_enable = 1;
			return false;
		}
	}
	data_len = read_sonix_datas(scan_data);
	sonix_irq_enable = 1;
	if(data_len>0)
	{
		type = scan_data[0];
		if(type == DATA_RESPONSE)// 数据回复，获取完毕之后会将标志位置1
		{
			u8 response_type = scan_data[1];
			u8 *response_data = &scan_data[3];
			switch(response_type)
			{
			case GET_LEDS_MODE:// 灯光模式
			{

//				printf("[-]MODE:%d\n",response_data[0]);
				if(response_data[0] == ALL_LEDS_WHITE)// 白光
				{
					return 0xff;
				}
				else if(response_data[0]>=0 && response_data[0]<=18)// 传输过来的数据范围是0~18
				{
					return _get_light_index(response_data[0])+1;
				}
				else
					return false;
			}
			break;
			case GET_LEDS_SPEED:// 灯光速度
			{
//				printf("[-]SPEED:%d\n",response_data[0]);
				if(response_data[0]>=0 && response_data[0]<=3)
				{
					return response_data[0]+1;
				}
				else
					return false;
			}
			break;
			case GET_LEDS_BRIGHTNESS:// 灯光亮度
			{
//				printf("[-]BRIGHT:%d\n",response_data[0]);
				if(response_data[0]>=0 && response_data[0]<=3)
				{
					return response_data[0]+1;
				}
				else
					return false;
			}
			break;
			}
		}
	}
	return false;
}

/**
 * @brief set led parameter include of mode(0~24) , speed(0~3) and brightness(0~3)
 *		    设置LED的参数，包括模式（0~24），速度（0~3），亮度（0~3）
 * @param 	mode SET_LEDS_MODE , SET_LEDS_SPEED, SET_LEDS_BRIGHTNESS
 *			param Set mode's parameter
 *
 * @return 	none.
 */
static void _set_rgb_param(u8 mode , u8 param, bool flash_en)
{
	u8 data[2];
	data[0] = mode;
	if(mode==SET_LEDS_MODE)
	{
		if(param <= RGB_MODE_NUM)
			data[1] = keyboardLedsMode[param];
		else if(param == 0xff)
			data[1] = ALL_LEDS_WHITE;
	}else
		data[1] = param;
	if(flash_en)
	{
//		printf("mode:%x,param:%d\n",mode,param);
		write_rgb_flash(sonix_keyboard_instance()->kleds);// 将数据写入Flash
	}
	write_sonix_datas(data,2);
//	printf("[3]set RGB:%x,%d\n",mode,param);
}
/**
 * @brief set MCU mode, bootloader or sleep
 *
 * @param mode.
 * @return success or failure.
 */
static u8 _set_mcu_mode(mcu_mode mode)
{
//	printf("[3]set MCU\n");
	u8 data[2] = {SET_MCU_MODE, mode};
	_mcu_mode = SET_FAILURE;

	if(mode < MCU_SLEEP  || mode > MCU_USB)
		return _mcu_mode;

	if(mode == MCU_SLEEP)
	{
		sonix_irq_enable = 0;
		write_sonix_datas(data,2);
		u32 write_tick = clock_time();
		// Wait for data return, up to 500us
		while(!gpio_read(GPIO_SN32_IRQ))
		{
			if(clock_time_exceed(write_tick, 500)){
				sonix_irq_enable = 1;
				return false;
			}
		}
		_sonix_data_scan();
		sonix_irq_enable = 1;// 数据不是中断处理，避免在循环中再次操作，因此需要清除标志位
		_mcu_mode =  mode;
	}
	else if(mode == MCU_BOOTLOADER)
	{
		write_sonix_datas(data,2);
		_mcu_mode =  mode;
	}
	else// BLE / USB
	{
		u8 data_len = 0;
		u8 ack[34] = {0};
		sonix_irq_enable = 0;
		write_sonix_datas(data,2);
		u32 write_tick = clock_time();
		// Wait for data return, up to 500us
		while(!gpio_read(GPIO_SN32_IRQ))
		{
			if(clock_time_exceed(write_tick, 500)){
				sonix_irq_enable = 1;
				return false;
			}
		}
		data_len = read_sonix_datas(ack);
		sonix_irq_enable = 1;
		if(data_len == 1 && ack[0] == 1)
			_mcu_mode =  mode;
	}

	return _mcu_mode;
}

/**This function is inaccuracy
 * @brief set led parameter include of mode(0~24) , speed(0~3) and brightness(0~3)
 *		    设置LED的参数，包括模式（0~24），速度（0~3），亮度（0~3）,set result will be return
 * @param 	mode SET_LEDS_MODE , SET_LEDS_SPEED, SET_LEDS_BRIGHTNESS
 *			param Set mode's parameter
 *
 * @return 	Whether the Settings are successful.
 */
static bool _set_rgb_param_ack(u8 mode , u8 param, bool flash_en)
{
	u8 return_param;
	_set_rgb_param(mode, param, FALSE);
	sleep_ms(1);
	return_param = _get_rgb_param(mode-SET_LEDS_MODE+GET_LEDS_MODE);// <<--- always return -1
//	printf("return_param:%d",return_param);
	if(return_param == false || (param != return_param-1 && param != 0xff))
		return false;
	else
	{
		if(flash_en)
		{
			if(mode == SET_LEDS_MODE)
				sonix_keyboard_instance()->kleds.mode = param;
			else if(mode == SET_LEDS_SPEED)
				sonix_keyboard_instance()->kleds.speed = param;
			else if(mode == SET_LEDS_BRIGHTNESS)
				sonix_keyboard_instance()->kleds.brightness = param;

			write_rgb_flash(sonix_keyboard_instance()->kleds);// 将数据写入Flash
		}
		return true;
	}
}


/**
 * @brief Set individual LED on and off
 *
 * @param 	index the index of leds
 *			state on/off -->> 1/0
 *
 * @return 	Whether the Settings are successful.
 */
static bool _set_single_led_ack(u8 index , u8 state)
{
	u8 data[3];
	u8 data_len = 0;
	u8 ack[34] = {0};

	data[0] = SET_SINGLE_LED;
	data[1] = index;
	data[2] = state;

	sonix_irq_enable = 0;
	write_sonix_datas(data,3);
	u32 write_tick = clock_time();
	while(!gpio_read(GPIO_SN32_IRQ))
	{
		if(clock_time_exceed(write_tick, 500)){
			sonix_irq_enable = 1;
			return false;
		}
	}
	data_len = read_sonix_datas(ack);
	sonix_irq_enable = 1;
	if(data_len == 1 && ack[0] == 1)
		return true;
	else
		return false;
}

/**
 * @brief Send the data to the slave machine to get the message, and wait for the data receive to complete
 *
 * @param mode 	GET_LEDS_MODE,GET_LEDS_SPEED,GET_LEDS_BRIGHTNESS,GET_SONIX_VERSION,GET_SINGLE_LEDS
 *
 * @return 	Means get data response true or false.
 */
static bool _get_sonix_param(u8 mode)
{
//	printf("[-]GET:%x\n",mode);
	_data_response_flag = 0;
	sonix_irq_enable = 0;// 数据不是中断处理，避免在循环中再次操作，因此需要加锁
	write_sonix_datas(&mode,1);
	u32 write_tick = clock_time();
	// Wait for data return, up to 500us
	while(!gpio_read(GPIO_SN32_IRQ))
	{
		if(clock_time_exceed(write_tick, 500)){
			sonix_irq_enable = 1;
			return false;
		}
	}
	_sonix_data_scan();
	sonix_irq_enable = 1;

	if(_data_response_flag==1)
		return true;

	return false;
}


void _sonix_reset()
{
	gpio_write(GPIO_SN32_INT,1);
	sleep_ms(6);
	gpio_write(GPIO_SN32_INT,0);
}

static _sonix_keyboard_manager *_instance;
static _sonix_keyboard_manager _instance_logic = {
	.kleds.mode = 0,
	.kleds.speed = 1,
	.kleds.brightness = 1,
	.init = _init,
	.sonix_data_scan = _sonix_data_scan,
	.set_mcu_mode = _set_mcu_mode,
	.set_rgb_param = _set_rgb_param,
	.set_rgb_param_ack = _set_rgb_param_ack,
	.set_single_led_ack = _set_single_led_ack,
	.get_sonix_param = _get_sonix_param,
	.get_sonix_datas = read_sonix_datas,
	.reset = _sonix_reset
};

_sonix_keyboard_manager *sonix_keyboard_instance()
{
	if(_instance == 0)
		_instance = &_instance_logic;
	return _instance;
}
