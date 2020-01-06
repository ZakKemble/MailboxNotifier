/*
 * Project: Remote Mail Notifier (and GPS Tracker)
 * Author: Zak Kemble, contact@zakkemble.net
 * Copyright: (C) 2020 by Zak Kemble
 * License: 
 * Web: https://blog.zakkemble.net/remote-mail-notifier-and-gps-tracker/
 */

#include "common.h"

// Calibration registers are read everytime a sensor is read, maybe read calibration regs once and store in memory?

// TODO
// Sometimes the I2C bus glitches out and the SDA line is stuck low. I'm not sure if its the BME280 or the A9G. When this happens I2C read/writes will fail with error 2 (I2C_ERROR_RESOURCE_BUSY)
// Maybe I2C_Close() then I2C_Init() fixes it?
// It might be caused by GSM interference
// Maybe stronger pullup resistors will fix it, they're 10k at the mo

#define ADDR	0x76

#define REG_CALI_DIG_T1	0x88
#define REG_CALI_DIG_T2	0x8A
#define REG_CALI_DIG_T3	0x8C

#define REG_CALI_DIG_P1	0x8E
#define REG_CALI_DIG_P2	0x90
#define REG_CALI_DIG_P3	0x92
#define REG_CALI_DIG_P4	0x94
#define REG_CALI_DIG_P5	0x96
#define REG_CALI_DIG_P6	0x98
#define REG_CALI_DIG_P7	0x9A
#define REG_CALI_DIG_P8	0x9C
#define REG_CALI_DIG_P9	0x9E

#define REG_CALI_DIG_H1	0xA1
#define REG_CALI_DIG_H2	0xE1
#define REG_CALI_DIG_H3	0xE3
#define REG_CALI_DIG_H4	0xE4
#define REG_CALI_DIG_H5	0xE5
#define REG_CALI_DIG_H6	0xE7

#define REG_STATUS		0xF3
#define REG_CTRL		0xF4
#define REG_CTRL_HUM	0xF2
#define REG_TEMPERATURE	0xFA
#define REG_PRESSURE	0xF7
#define REG_HUMIDITY	0xFD

#define OVERSAMPLE_X16_TEMPERATURE	0b10100000
#define OVERSAMPLE_X16_PRESSURE		0b00010100
#define OVERSAMPLE_X16_HUMIDITY		0b00000101
#define MODE_SLEEP	0x00
#define MODE_FORCE	0x01
#define MODE_NORMAL	0x03

static int32_t t_fine;

static void i2cWrite(void* buff, uint8_t len)
{
	I2C_Error_t res = I2C_Transmit(I2C2, ADDR, buff, len, I2C_DEFAULT_TIME_OUT);
	if(res != I2C_ERROR_NONE)
		PRINTD("I2C2 write err: %d", res);
}

static void i2cRead(void* buff, uint8_t len)
{
	I2C_Error_t res = I2C_Receive(I2C2, ADDR, buff, len, I2C_DEFAULT_TIME_OUT);
	if(res != I2C_ERROR_NONE)
		PRINTD("I2C2 read err: %d", res);
}

static uint8_t read8(uint8_t reg)
{
	i2cWrite(&reg, 1);
	i2cRead(&reg, 1);
	return reg;
}

static uint16_t read16(uint8_t reg)
{
	uint8_t data[2];
	i2cWrite(&reg, 1);
	i2cRead(data, 2);
	return (data[0]<<8) | data[1];
}

static uint16_t read16_LE(uint8_t reg)
{
	uint16_t temp = read16(reg);
	return (temp>>8) | (temp<<8);
}

static int16_t readS16_LE(uint8_t reg)
{
	return (int16_t)read16_LE(reg);
}

static uint32_t read24(uint8_t reg)
{
	uint8_t data[3];
	i2cWrite(&reg, 1);
	i2cRead(data, 3);
	return (data[0]<<16) | (data[1]<<8) | data[2];
}

void bme280_init()
{
	uint8_t data[2];

//	data[0] = 0xE0;
//	data[1] = 0xB6;
//	i2cWrite(data, 2);
//	OS_Sleep(300);

	data[0] = REG_CTRL_HUM;
	data[1] = OVERSAMPLE_X16_HUMIDITY;
	i2cWrite(data, 2);

	data[0] = REG_CTRL;
	data[1] = OVERSAMPLE_X16_TEMPERATURE | OVERSAMPLE_X16_PRESSURE | MODE_SLEEP;
	i2cWrite(data, 2);
}

void bme280_startConvertion()
{
	uint8_t data[2];
	data[0] = REG_CTRL;
	data[1] = OVERSAMPLE_X16_TEMPERATURE | OVERSAMPLE_X16_PRESSURE | MODE_FORCE;
	i2cWrite(data, 2);
}

uint8_t bme280_status()
{
	return read8(REG_STATUS) & (0x08 | 0x01);
}

int32_t bme280_readTemperature()
{
	int32_t adc_T = read24(REG_TEMPERATURE);
	if(adc_T == 0x800000)
		return 0;

	uint16_t dig_T1 = read16_LE(REG_CALI_DIG_T1);
	int16_t dig_T2 = readS16_LE(REG_CALI_DIG_T2);
	int16_t dig_T3 = readS16_LE(REG_CALI_DIG_T3);

	adc_T >>= 4;

	int32_t var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1))) * ((int32_t)dig_T2)) >> 11;
	int32_t var2 = (((((adc_T >> 4) - ((int32_t)dig_T1)) * ((adc_T >> 4) - ((int32_t)dig_T1))) >> 12) * ((int32_t)dig_T3)) >> 14;

	t_fine = var1 + var2;

	int32_t T = (t_fine * 5 + 128) >> 8;
	return T;
}

int32_t bme280_readPressure()
{
	int32_t adc_P = read24(REG_PRESSURE);
	if (adc_P == 0x800000)
		return 0;

	uint16_t dig_P1 = read16_LE(REG_CALI_DIG_P1);
	int16_t dig_P2 = readS16_LE(REG_CALI_DIG_P2);
	int16_t dig_P3 = readS16_LE(REG_CALI_DIG_P3);
	int16_t dig_P4 = readS16_LE(REG_CALI_DIG_P4);
	int16_t dig_P5 = readS16_LE(REG_CALI_DIG_P5);
	int16_t dig_P6 = readS16_LE(REG_CALI_DIG_P6);
	int16_t dig_P7 = readS16_LE(REG_CALI_DIG_P7);
	int16_t dig_P8 = readS16_LE(REG_CALI_DIG_P8);
	int16_t dig_P9 = readS16_LE(REG_CALI_DIG_P9);

	adc_P >>= 4;

	int64_t var1 = ((int64_t)t_fine) - 128000;
	int64_t var2 = var1 * var1 * (int64_t)dig_P6;
	var2 = var2 + ((var1 * (int64_t)dig_P5) << 17);
	var2 = var2 + (((int64_t)dig_P4) << 35);
	var1 = ((var1 * var1 * (int64_t)dig_P3) >> 8) + ((var1 * (int64_t)dig_P2) << 12);
	var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)dig_P1) >> 33;

	if(var1 == 0)
		return 0; // avoid exception caused by division by zero

	int64_t p = 1048576 - adc_P;
	p = (((p << 31) - var2) * 3125) / var1;
	var1 = (((int64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
	var2 = (((int64_t)dig_P8) * p) >> 19;

	p = ((p + var1 + var2) >> 8) + (((int64_t)dig_P7) << 4);

	return (int32_t)p;
}

int32_t bme280_readHumidity()
{
	int32_t adc_H = read16(REG_HUMIDITY);
	if (adc_H == 0x8000)
		return 0;

	uint8_t dig_H1 = read8(REG_CALI_DIG_H1);
	int16_t dig_H2 = readS16_LE(REG_CALI_DIG_H2);
	uint8_t dig_H3 = read8(REG_CALI_DIG_H3);
	int16_t dig_H4 = (read8(REG_CALI_DIG_H4) << 4) | (read8(REG_CALI_DIG_H4 + 1) & 0xF);
	int16_t dig_H5 = (read8(REG_CALI_DIG_H5 + 1) << 4) | (read8(REG_CALI_DIG_H5) >> 4);
	int8_t dig_H6 = (int8_t)read8(REG_CALI_DIG_H6);

	int32_t v_x1_u32r = (t_fine - ((int32_t)76800));

	v_x1_u32r =
		(((((adc_H << 14) - (((int32_t)dig_H4) << 20) -
		(((int32_t)dig_H5) * v_x1_u32r)) +
		((int32_t)16384)) >>
		15) *
		(((((((v_x1_u32r * ((int32_t)dig_H6)) >> 10) *
		(((v_x1_u32r * ((int32_t)dig_H3)) >> 11) +
		((int32_t)32768))) >>
		10) +
		((int32_t)2097152)) *
		((int32_t)dig_H2) +
		8192) >>
		14));

	v_x1_u32r =
		(v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) *
		((int32_t)dig_H1)) >>
		4));

	v_x1_u32r = (v_x1_u32r < 0) ? 0 : v_x1_u32r;
	v_x1_u32r = (v_x1_u32r > 419430400) ? 419430400 : v_x1_u32r;
	return (int32_t)(v_x1_u32r >> 12);
}
