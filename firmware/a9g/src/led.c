/*
 * Project: Remote Mail Notifier (and GPS Tracker)
 * Author: Zak Kemble, contact@zakkemble.net
 * Copyright: (C) 2020 by Zak Kemble
 * License: 
 * Web: https://blog.zakkemble.net/remote-mail-notifier-and-gps-tracker/
 */

#include "common.h"

#define LED_COUNT	2

typedef struct {
	GPIO_LEVEL pin;
	uint8_t state;
	uint8_t timer;
	uint8_t onPeriod;
	uint8_t offPeriod;
} led_t;

static led_t leds[LED_COUNT];

void led_init()
{
	GPIO_EnablePower(GPIO_PIN25, true);
	GPIO_EnablePower(GPIO_PIN30, true);

	GPIO_config_t gpioLEDs = {
		.mode			= GPIO_MODE_OUTPUT,
		.pin			= GPIO_PIN30,
		.defaultLevel	= GPIO_LEVEL_HIGH
	};
	GPIO_Init(gpioLEDs);
	gpioLEDs.pin = GPIO_PIN25;
	GPIO_Init(gpioLEDs);
	
	leds[0].pin = GPIO_PIN25;
	leds[0].state = GPIO_LEVEL_HIGH;
	leds[1].pin = GPIO_PIN30;
	leds[1].state = GPIO_LEVEL_HIGH;
}

void led_rate(uint8_t led, uint8_t onPeriod, uint8_t offPeriod)
{
	leds[led].onPeriod = onPeriod;
	leds[led].offPeriod = offPeriod;
}

void led_update()
{
	for(uint8_t i=0;i<LED_COUNT;i++)
	{
		leds[i].timer++;
		GPIO_LEVEL oldState = leds[i].state;
		
		if(leds[i].onPeriod == 0)
			leds[i].state = GPIO_LEVEL_LOW;
		else if(leds[i].offPeriod == 0)
			leds[i].state = GPIO_LEVEL_HIGH;
		else
		{
			if(leds[i].state == GPIO_LEVEL_HIGH && leds[i].timer > leds[i].onPeriod)
				leds[i].state = GPIO_LEVEL_LOW;
			else if(leds[i].state == GPIO_LEVEL_LOW && leds[i].timer > leds[i].offPeriod)
				leds[i].state = GPIO_LEVEL_HIGH;
		}

		if(oldState != leds[i].state)
		{
			leds[i].timer = 0;
			GPIO_Set(leds[i].pin, leds[i].state);
		}
	}
/*
	static uint8_t tickCount;
	tickCount++;
	if(tickCount < 10)
		return;
	tickCount = 0;

	static GPIO_LEVEL ledFlash;
	ledFlash = (ledFlash == GPIO_LEVEL_HIGH) ? GPIO_LEVEL_LOW : GPIO_LEVEL_HIGH;
	GPIO_Set(GPIO_PIN25, ledFlash);
	GPIO_Set(GPIO_PIN30, ledFlash);
*/
}
