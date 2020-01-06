/*
 * Project: Remote Mail Notifier (and GPS Tracker)
 * Author: Zak Kemble, contact@zakkemble.net
 * Copyright: (C) 2020 by Zak Kemble
 * License: 
 * Web: https://blog.zakkemble.net/remote-mail-notifier-and-gps-tracker/
 */

#ifndef __BME280_H_
#define __BME280_H_

void bme280_init(void);
void bme280_startConvertion(void);
uint8_t bme280_status(void);
int32_t bme280_readTemperature(void);
int32_t bme280_readHumidity(void);
int32_t bme280_readPressure(void);

#endif
