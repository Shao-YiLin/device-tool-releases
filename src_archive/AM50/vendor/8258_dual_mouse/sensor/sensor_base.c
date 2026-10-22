/*
 * sensor_base.c
 *
 *  Created on: 2022-7-12
 *      Author: xdhuang
 */

#include "sensor_base.h"

static void set_sensor_wakeup_level(uint8_t enable)
{
	set_sensor_wakeup(0, enable);
}

static void write_bit_to_sdio(uint8_t bit)
{
	write_sdio_bit(bit);
}

static void sdio_sendbyte(uint8_t data)
{
	uint8_t i;
	uint8_t buf;

	buf = data;
	for (i = 0; i < 8; i++)
	{
		SET_CLK_LOW();
		CLOCK_DELAY();
		write_bit_to_sdio(buf & 0x80);
		SET_CLK_HIGH();
		CLOCK_DELAY();

		buf <<= 1;
	}
}

static uint8_t sdio_readbyte()
{
	uint8_t i;
	uint8_t data;

	data = 0;
	for (i = 0; i < 8; i++)
	{
		SET_CLK_LOW();
		data <<= 1;
		CLOCK_DELAY();
		SET_CLK_HIGH();
		if (read_sdio_bit())
		{
			data |= 1;
		}
		CLOCK_DELAY();
	}
	return data;
}

static uint8_t spi_sdio_read_byte(uint8_t addr)
{
	uint32_t s;
	uint8_t data;
	s = os_enter_lock();

	SET_SDIO_OUTPUT_ENABLE();
	SET_SDIO_INPUT_DISABLE();
	SET_CS_LOW();

	sdio_sendbyte(addr & 0x7f);

	SET_SDIO_INPUT_ENABLE();
	SET_SDIO_OUTPUT_DISABLE();
	data = sdio_readbyte();
	SET_CS_HIGH();
	os_leave_lock(s);
	return data;
}

static void spi_sdio_write_byte(uint8_t addr, uint8_t data)
{
	uint32_t s;

	s = os_enter_lock();

	SET_SDIO_OUTPUT_ENABLE();
	SET_SDIO_INPUT_DISABLE();
	SET_CS_LOW();

	sdio_sendbyte(addr | 0x80);
	sdio_sendbyte(data);

	SET_CS_HIGH();
	SET_SDIO_INPUT_ENABLE();
	SET_SDIO_OUTPUT_DISABLE();

	os_leave_lock(s);
}

static void download_config_table(const uint8_t *ptbl, uint32_t len)
{
	uint32_t i;
    for (i = 0; i < len; i += 2)
    {
    	spi_sdio_write_byte(ptbl[i], ptbl[i+1]);
    }
}

static void _sensor_resync()
{
	SET_CS_LOW();
	SLEEP_DELAY(33000);
	SET_CS_HIGH();
}

static uint16_t _sensor_identity(uint8_t pid1_addr, uint8_t pid2_addr)
{
	uint8_t p1, p2;
	_sensor_resync();
	p1 = spi_sdio_read_byte(pid1_addr);
	p2 = spi_sdio_read_byte(pid2_addr);
	return p1 << 8 | p2;
}

struct sensor_base base_instance =
{
	.set_wakeup_level = set_sensor_wakeup_level,
	.read_byte = spi_sdio_read_byte,
	.write_byte = spi_sdio_write_byte,
	.initial_config = download_config_table,
	.sensor_resync = _sensor_resync,
	.sensor_identity = _sensor_identity
};

struct sensor_base *_base = NULL;
struct sensor_base *base_get_construct()
{
	if(_base == NULL)
	{
		_base = &base_instance;
	}
	return _base;
}
