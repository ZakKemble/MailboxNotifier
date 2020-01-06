/*
 * Project: Remote Mail Notifier (and GPS Tracker)
 * Author: Zak Kemble, contact@zakkemble.net
 * Copyright: (C) 2020 by Zak Kemble
 * License: 
 * Web: https://blog.zakkemble.net/remote-mail-notifier-and-gps-tracker/
 */

#ifndef __LED_H_
#define __LED_H_

#define LED_NETWORK	0
#define LED_GPS	1

#define LED_RATE_NETWORK_OFF				1,19
#define LED_RATE_NETWORK_NO_SIGNAL			1,1
#define LED_RATE_NETWORK_GSM				10,10
#define LED_RATE_NETWORK_GPRS				1,8

#define LED_RATE_GPS_OFF	0,0
#define LED_RATE_GPS_NOFIX	1,1
#define LED_RATE_GPS_FIX	1,19

void led_init(void);
void led_rate(uint8_t led, uint8_t onTime, uint8_t offTime);
void led_update(void);

#endif
