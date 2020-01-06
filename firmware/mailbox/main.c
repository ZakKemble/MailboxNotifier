/*
 * Project: Remote Mail Notifier (and GPS Tracker)
 * Author: Zak Kemble, contact@zakkemble.net
 * Copyright: (C) 2020 by Zak Kemble
 * License: 
 * Web: https://blog.zakkemble.net/remote-mail-notifier-and-gps-tracker/
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <avr/io.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>
//#include <util/atomic.h>
#include "mailcomm_defs.h"

// A0 = UPDI
// A1 = A9G Power out
// A2 = Trigger switch in
// A3 = Manual switch in
// A6 = UART out
// A7 = Charge detect in

// Fuses:
// WDTCFG: 0x00 (default)
// BODCFG: 0x0C
// OSCCFG: 0x01 (default)
// SYSCFG0: 0xC4 (default)
// SYSCFG1: 0x07 (default)
// APPEND: 0x00 (default)
// BOOTEND: 0x00 (default)
// LOCKBIT: 0x00 (default)

#define VLOWBATT			3500
#define VCHARGEDBATT		4050

#define TIMEOUT				7500 // 2 mins
#define TIMEOUT_KEEPALIVE	3750 // 60 seconds

#define RETRY_COUNT			5



#define VREF_VAL			1100
#define LOWBATT_VAL			(uint8_t)((((float)VREF_VAL / VLOWBATT) * 255.0) + 0.5)
#define CHARGEDBATT_VAL		(uint8_t)((((float)VREF_VAL / VCHARGEDBATT) * 255.0) + 0.5)

#define STATE_IDLE		0
#define STATE_WAIT		1
#define STATE_POWEROFF	2
#define STATE_DELAY		3

#define PORT_OUT1		_BV(1)
#define PORT_DIR1		_BV(1)
#define PORT_DIR2		_BV(2)
#define PORT_IN2		_BV(2)
#define PORT_IN3		_BV(3)
#define PORT_IN7		_BV(7)

#define BAUD_VAL ((42<<5) | (66>>1)) // 9600 @ 2.66MHz

#define CMDDATA_BUFF	8

#define UART_DIR_RX	0
#define UART_DIR_TX	1

#define PWROFF_UNKNOWN	0
#define PWROFF_FAILURE	1
#define PWROFF_SUCCESS	2

typedef enum
{
	TRIG_IDLE = 0,
	TRIG_WAITACTIVE,
	TRIG_ACTIVE,
	TRIG_WAITDEACTIVE,
	TRIG_DISABLE
} trigState_t;

typedef enum
{
	TRIG_CHANGE_NONE = 0,
	TRIG_CHANGE_ACTIVE,
	TRIG_CHANGE_DEACTIVE
} trigChange_t;

typedef struct {
	uint8_t newMail;
	uint8_t trackMode;
	uint8_t switchStuck;
	uint8_t endCharging;
} reasons_t;

typedef struct {
	trigState_t state;
	uint16_t time;
} trigger_t;

static volatile uint16_t now;
static volatile uint8_t interrupt;
static volatile uint8_t uartDirection;
static volatile uint8_t uartData;
static volatile uint8_t uartNewData;

static volatile uint8_t cmdData[CMDDATA_BUFF];
static volatile uint8_t cmdDataIdx;

static volatile uint8_t vlmDetected;

static uint8_t mcusr_mirror __attribute__ ((section(".noinit,\"aw\",@nobits;"))); // BUG: https://github.com/qmk/qmk_firmware/issues/3657

void get_mcusr(void) __attribute__ ((naked, used, section(".init3")));
void get_mcusr()
{
	mcusr_mirror = RSTCTRL.RSTFR;
	RSTCTRL.RSTFR = 0xFF;
}

static trigChange_t trig_process(trigger_t* trig, uint8_t in, uint16_t now)
{
	if(in)
	{
		if(trig->state == TRIG_IDLE)
		{
			trig->state = TRIG_WAITACTIVE;
			trig->time = now;
		}
		else if(trig->state == TRIG_WAITACTIVE && (uint16_t)(now - trig->time) >= 32) // 500ms
		{
			trig->state = TRIG_ACTIVE;
			return TRIG_CHANGE_ACTIVE;
		}
		else if(trig->state == TRIG_WAITDEACTIVE)
			trig->state = TRIG_ACTIVE;
	}
	else if(trig->state != TRIG_IDLE)
	{
		if(trig->state == TRIG_WAITACTIVE)
			trig->state = TRIG_IDLE;
		else if(trig->state == TRIG_ACTIVE)
		{
			trig->state = TRIG_WAITDEACTIVE;
			trig->time = now;
		}
		else if(trig->state == TRIG_WAITDEACTIVE && (uint16_t)(now - trig->time) >= 32) // 500ms
		{
			trig->state = TRIG_IDLE;
			return TRIG_CHANGE_DEACTIVE;
		}
	}
	
	return TRIG_CHANGE_NONE;
}

int main(void)
{
	// TODO Watchdog
	//WDT.CTRLA = 0x0A; // 4 seconds

	// Brown-out
	//BOD.CTRLA = BOD_ACTIVE_ENWAKE_gc | BOD_SLEEP_ENABLED_gc; // CCP protected, already set by fuse
	BOD.CTRLB = BOD_LVL_BODLEVEL2_gc; // 2.60V
	BOD.VLMCTRLA = BOD_VLMLVL_25ABOVE_gc; // 3.25V
	BOD.INTCTRL = BOD_VLMIE_bm;

	// Clocks
	// Default clock is 16MHz / 6 = 2.66MHz
	//clock_prescale_set(CPU_DIV);

	// Configure pins
	PORTA.OUT = PORT_OUT1;
	PORTA.DIR = PORT_DIR1;
	PORTA.PIN2CTRL = PORT_PULLUPEN_bm | PORT_ISC_FALLING_gc;
	PORTA.PIN3CTRL = PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc; // FALLING/RISING interrupt doesn't work in sleep mode for this pin
	PORTA.PIN7CTRL = PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc;

	// PIT Timer
	RTC.CLKSEL = RTC_CLKSEL0_bm;
	while(RTC.PITSTATUS & RTC_CTRLBUSY_bm);
	RTC.PITINTCTRL = RTC_PI_bm;
	while(RTC.PITSTATUS & RTC_CTRLBUSY_bm);
	RTC.PITCTRLA = RTC_PERIOD1_bm | RTC_PERIOD0_bm | RTC_PITEN_bm; // 16ms
	while(RTC.PITSTATUS & RTC_CTRLBUSY_bm);

	// UART one-wire
	PORTA.PIN6CTRL = PORT_PULLUPEN_bm;
	USART0.BAUD = BAUD_VAL;
	USART0.CTRLA = USART_RXCIE_bm | USART_TXCIE_bm | USART_LBME_bm;
	USART0.CTRLB = USART_RXEN_bm | USART_TXEN_bm | USART_SFDEN_bm | USART_ODME_bm;

	// VREF
	VREF.CTRLA = VREF_ADC0REFSEL_1V1_gc;
	
	// ADC
	ADC0.CTRLA = ADC_RESSEL_8BIT_gc;
	ADC0.CTRLC = ADC_SAMPCAP_bm | ADC_REFSEL_VDDREF_gc | ADC_PRESC_DIV8_gc;
	ADC0.CTRLD = ADC_INITDLY_DLY16_gc;
	ADC0.SAMPCTRL = 8;
	ADC0.MUXPOS = ADC_MUXPOS_INTREF_gc;

	reasons_t reasons = {
		.newMail = 0,
		.trackMode = 0,
		.switchStuck = 0,
		.endCharging = 0
	};

	reasons_t reasonsShadow = {
		.newMail = 0,
		.trackMode = 0,
		.switchStuck = 0,
		.endCharging = 0
	};

	uint16_t successCount = 0;
	uint16_t failureCount = 0;
	uint16_t timeoutCount = 0;
	uint8_t smsBalanceGet = 0;

	trigger_t button = {TRIG_IDLE, 0};
	trigger_t charging = {TRIG_IDLE, 0};
	trigger_t mail = {TRIG_IDLE, 0};

	uint8_t state = STATE_IDLE;

	uint16_t powerOnOffTime = 0;
	uint8_t poweroffDelay = 0;

	uint16_t keepAliveTime = 0;
	
	uint8_t switchStuck = 0;
	uint8_t chargeComplete = 0;

	uint16_t checkStuckTime = 0;
	uint8_t forceCheckStuck = 0;
	
	uint8_t retryCount = 0;
	uint16_t lastRetryTime = 0;
	
	uint8_t clearVlmDetected = 0;

	uartDirection = UART_DIR_RX;

	sei();
	
	while(1)
	{
		cli();
		interrupt = 0;
		uint8_t port = ~PORTA.IN;
		uint16_t tmpNow = now;
		sei();

		// Button press
		if(trig_process(&button, (port & PORT_IN3), tmpNow) == TRIG_CHANGE_ACTIVE)
		{
			reasons.trackMode = !reasons.trackMode;
			if(!reasons.trackMode)
			{
				powerOnOffTime = tmpNow - TIMEOUT + 940; // Give 15 seconds to shutdown
				retryCount = RETRY_COUNT;
			}
		}

		// Charge complete
		if(trig_process(&charging, (port & PORT_IN7), tmpNow) == TRIG_CHANGE_DEACTIVE)
		{
			vlmDetected = 0;

			// Only send a charge complete notification once, then wait until the battery voltage drops below VCHARGEDBATT before allowing another notification
			// NOTE: A charge complete notification is sent when charging stops, even if the battery is not full
			if(!chargeComplete)
				reasons.endCharging = 1;
			chargeComplete = 1;
		}

		// Mail trigger
		if(trig_process(&mail, (port & PORT_IN2), tmpNow) == TRIG_CHANGE_ACTIVE)
			reasons.newMail = 1;

		// Mail switch stuck
		if(
			(port & PORT_IN2) &&
			mail.state == TRIG_ACTIVE &&
			(uint16_t)(tmpNow - mail.time) >= 940 // 15 seconds
		)
		{
			mail.state = TRIG_DISABLE;
			checkStuckTime = tmpNow;
			forceCheckStuck = 0;
			if(!switchStuck)
				reasons.switchStuck = 1;
			switchStuck = 1;
			PORTA.PIN2CTRL &= ~(PORT_PULLUPEN_bm | PORT_ISC_FALLING_gc);
			PORTA.DIR |= PORT_DIR2;
		}

		// Stuck switch stuff
		if(switchStuck && (forceCheckStuck || (uint16_t)(tmpNow - checkStuckTime) >= 128)) // 2 secs
		{
			checkStuckTime = tmpNow;
			forceCheckStuck = 0;

			PORTA.PIN2CTRL |= PORT_PULLUPEN_bm;
			PORTA.DIR &= ~PORT_DIR2;
			//cli();
			//CCP = 0xD8;
			//CLKCTRL.MCLKCTRLB |= 0x08;
			// Ideally we would downclock during this delay, but then UART would mess up
			// TODO only do this check when A9G is powered off?
			_delay_us(25);
			//CCP = 0xD8;
			//CLKCTRL.MCLKCTRLB &= ~0x08;
			//sei();
			if(PORTA.IN & PORT_IN2) // Has pin input gone high? Then the switch has opened and no longer stuck
			{
				reasons.switchStuck = 0;
				switchStuck = 0;
				PORTA.PIN2CTRL |= PORT_ISC_FALLING_gc;
				mail.state = TRIG_IDLE;
			}
			else // Still stuck
			{
				PORTA.PIN2CTRL &= ~PORT_PULLUPEN_bm;
				PORTA.DIR |= PORT_DIR2;
			}
		}
		
		//reasons.newMail = 1;

		switch(state)
		{
			case STATE_DELAY:
			
				// After powring off the GSM module wait for at least 1 second so the capacitors and things discharge before turning it back on
				if(poweroffDelay && (uint16_t)(tmpNow - powerOnOffTime) >= 64)
				{
					poweroffDelay = 0;
					if(retryCount == 0)
						state = STATE_IDLE;
				}

				// Wait 5 seconds if we're doing a retry
				if(retryCount > 0 && (uint16_t)(tmpNow - lastRetryTime) >= 320)
				{
					if(!poweroffDelay)
						state = STATE_IDLE;
				}
				
				if(state == STATE_DELAY)
				{
					// Still delaying? Then sleep for 16ms
					// TODO sleep for 1 second instead?
					cli();
					if(!interrupt)
					{
						//BOD.CTRLA = BOD_ACTIVE_ENWAKE_gc | BOD_SLEEP_DIS_gc;
						SLPCTRL.CTRLA = SLPCTRL_SMODE_PDOWN_gc | SLPCTRL_SEN_bm;
						sei();
						sleep_cpu();
						//BOD.CTRLA = BOD_ACTIVE_ENWAKE_gc | BOD_SLEEP_ENABLED_gc;
					}
					sei();
					break;
				}
				__attribute__ ((fallthrough));
			case STATE_IDLE:
				poweroffDelay = 0;
				if(reasons.newMail == 0 && reasons.endCharging == 0 && reasons.trackMode == 0 && reasons.switchStuck == 0) // Nothing to do
				{
					retryCount = 0;

					cli();
					if(!interrupt)
					{
						uint8_t canDoLongSleep = (
							(mail.state == TRIG_IDLE || mail.state == TRIG_DISABLE)
							&& button.state != TRIG_WAITACTIVE
							&& button.state != TRIG_WAITDEACTIVE
							&& charging.state != TRIG_WAITACTIVE
							&& charging.state != TRIG_WAITDEACTIVE
						);
						
						// Long sleep:
						// Infinite if everything is ok (wake up by pin change interrupt)
						// 32 seconds if switch is stuck (to see if its still stuck)

						if(canDoLongSleep)
						{
							if(!switchStuck)
								RTC.PITCTRLA = 0; // PIT off
							else
								RTC.PITCTRLA = RTC_PERIOD3_bm | RTC_PERIOD2_bm | RTC_PERIOD1_bm | RTC_PITEN_bm; // 32 seconds

							// RTC/PIT update takes around 3ms to complete
							// Downclock CPU to save power
							CCP = 0xD8;
							CLKCTRL.MCLKCTRLB |= 0x08; // Div 48
							while(RTC.PITSTATUS & RTC_CTRLBUSY_bm);
							CCP = 0xD8;
							CLKCTRL.MCLKCTRLB &= ~0x08; // Div 6
						}

						//BOD.CTRLA = BOD_ACTIVE_ENWAKE_gc | BOD_SLEEP_DIS_gc;
						SLPCTRL.CTRLA = SLPCTRL_SMODE_PDOWN_gc | SLPCTRL_SEN_bm;
						sei();
						sleep_cpu();
						//BOD.CTRLA = BOD_ACTIVE_ENWAKE_gc | BOD_SLEEP_ENABLED_gc;

						// Put PIT back to 16ms
						if(canDoLongSleep)
						{
							RTC.PITCTRLA = RTC_PERIOD1_bm | RTC_PERIOD0_bm | RTC_PITEN_bm; // 16ms

							cli();
							CCP = 0xD8;
							CLKCTRL.MCLKCTRLB |= 0x08; // Div 48
							sei();
							while(RTC.PITSTATUS & RTC_CTRLBUSY_bm);
							cli();
							CCP = 0xD8;
							CLKCTRL.MCLKCTRLB &= ~0x08; // Div 6
							sei();
						}

						// The stuck switch timer counter expects the timer to increment every 16ms, but we
						// might have just woken up from a 32 second PIT, so set forceCheckStuck to make it check now
						forceCheckStuck = 1;
					}
					sei();
					break;
				}
				else // We have something to do
				{
					// Check battery voltage
					ADC0.CTRLA |= ADC_ENABLE_bm;
					ADC0.COMMAND = ADC_STCONV_bm;
					while(ADC0.COMMAND); // ~100us
					uint8_t val = ADC0.RESL;
					ADC0.CTRLA &= ~ADC_ENABLE_bm;

					// TODO?
					// Do we need to discard the first conversion after starting the ADC?
					// The ADC has an initialization delay thing which is supposed to let everything settle.
					
					// Remember, reverse logic with these val compares

					// If battery voltage falls below VCHARGEDBATT then the battery is no longer considered fully charged.
					// This will allow a charge complete notification next time charging stops.
					if(val > CHARGEDBATT_VAL)
						chargeComplete = 0;

					if(val < LOWBATT_VAL)
					{
						// Battery is good, turn on GSM module
						if(!vlmDetected)
							clearVlmDetected = 1;
						PORTA.OUT &= ~PORT_OUT1;
						powerOnOffTime = tmpNow;
						keepAliveTime = tmpNow;
						uartNewData = 0;
						state = STATE_WAIT;
						reasonsShadow.newMail = 0;
						reasonsShadow.endCharging = 0;
						//reasonsShadow.trackMode = 0;
						reasonsShadow.switchStuck = 0;
					}
					else
					{
						// Battery too low, clear everything and do nothing
						reasons.newMail = 0;
						reasons.endCharging = 0;
						reasons.trackMode = 0;
						reasons.switchStuck = 0;
						retryCount = 0;
						break;
					}
				}
				__attribute__ ((fallthrough));
			case STATE_WAIT:
				if(!reasons.trackMode && (uint16_t)(tmpNow - powerOnOffTime) >= TIMEOUT)// || (uint16_t)(tmpNow - keepAliveTime) > TIMEOUT_KEEPALIVE) // TODO implement keep-alive stuff
				{
					// Module is taking too long doing stuff, force turn off and retry

					PORTA.OUT |= PORT_OUT1;
					if(timeoutCount < UINT_MAX)
						timeoutCount++;
					
					retryCount++;
					if(retryCount < RETRY_COUNT)
					{
						reasons.newMail |= reasonsShadow.newMail;
						reasons.endCharging |= reasonsShadow.endCharging;
						//reasons.trackMode |= reasonsShadow.trackMode;
						reasons.switchStuck |= reasonsShadow.switchStuck;
					}
					else
					{
						// Tried too many times, clear everything and do nothing
						retryCount = 0;
						reasons.newMail = 0;
						reasons.endCharging = 0;
						//reasons.trackMode = 0;
						reasons.switchStuck = 0;
					}
					lastRetryTime = tmpNow;
					
					state = STATE_POWEROFF;
				}
				else
				{
					cli();
					if(!uartNewData && !interrupt)
					{
						if(uartDirection == UART_DIR_TX)// Idle sleep and wait for UART TX complete interrupt or PIT interrupt
							SLPCTRL.CTRLA = SLPCTRL_SMODE_IDLE_gc | SLPCTRL_SEN_bm;
						else // Standby sleep and wait for UART data or PIT interrupt
							SLPCTRL.CTRLA = SLPCTRL_SMODE_STDBY_gc | SLPCTRL_SEN_bm;

						sei();
						sleep_cpu();
					}
					sei();
					
					// When the A9G is first turned on the inrush current to all the capacitors causes the battery voltage to drop by around 0.8V, even with a soft-start thing in place.
					// This might trigger the VLM thing, so clear it after ~500ms if it wasn't already set before powering on.
					// Maybe I should make the soft-start even more fluffy u.u
					if(clearVlmDetected && (uint16_t)(tmpNow - powerOnOffTime) >= 30)
					{
						vlmDetected = 0;
						clearVlmDetected = 0;
					}

					// Commands processor
					if(uartNewData)
					{
						cli();
						uint8_t data = uartData;
						uartNewData = 0;
						sei();

						uint8_t cmd = data & 0x07;
						data >>= 3;

						//if(state == STATE_WAIT)// && uartDirection == UART_DIR_RX)
						switch(cmd)
						{
							case MAIL_COMM_REQUEST:
								cmdData[0] = MAIL_COMM_DO;
								cmdData[1] = successCount>>8;
								cmdData[2] = successCount;
								cmdData[3] = failureCount>>8;
								cmdData[4] = failureCount;
								cmdData[5] = timeoutCount>>8;
								cmdData[6] = timeoutCount;
								cmdData[7] = (smsBalanceGet == 0)<<5 | vlmDetected<<4 | reasons.newMail<<3 | reasons.endCharging<<2 | reasons.trackMode<<1 | reasons.switchStuck;
								cmdDataIdx = 0;
								USART0.CTRLA |= USART_DREIE_bm;
								reasonsShadow.newMail = reasons.newMail;
								reasonsShadow.endCharging = reasons.endCharging;
								//reasonsShadow.trackMode = reasons.trackMode;
								reasonsShadow.switchStuck = reasons.switchStuck;
								reasons.newMail = 0;
								reasons.endCharging = 0;
								//reasons.trackMode = 0;
								reasons.switchStuck = 0;
								//break;
								__attribute__ ((fallthrough));
							case MAIL_COMM_KEEPALIVE:
								keepAliveTime = tmpNow;
								break;
							case MAIL_COMM_POWEROFF:
								if(data == PWROFF_SUCCESS)
								{
									if(successCount < UINT16_MAX)
										successCount++;

									// Only get balance once every 10 wakes
									// This saves ~8 seconds, increasing battery life
									smsBalanceGet++;
									if(smsBalanceGet >= 10)
										smsBalanceGet = 0;

									retryCount = 0;
								}
								else
								{
									if(failureCount < UINT16_MAX)
										failureCount++;

									if(!reasons.trackMode) // Retry forever if in tracking mode
										retryCount++;

									if(retryCount < RETRY_COUNT)
									{
										reasons.newMail |= reasonsShadow.newMail;
										reasons.endCharging |= reasonsShadow.endCharging;
										//reasons.trackMode |= reasonsShadow.trackMode;
										reasons.switchStuck |= reasonsShadow.switchStuck;
									}
									else
									{
										// Tried too many times, clear everything and do nothing
										retryCount = 0;
										reasons.newMail = 0;
										reasons.endCharging = 0;
										//reasons.trackMode = 0;
										reasons.switchStuck = 0;
									}
									lastRetryTime = tmpNow;
								}
								state = STATE_POWEROFF;
								break;
							default:
								memset((uint8_t*)cmdData, '?', CMDDATA_BUFF);
								cmdData[1] = cmd;
								cmdData[2] = data;
								cmdDataIdx = 0;
								USART0.CTRLA |= USART_DREIE_bm;
								break;
						}
					}
				}
				break;
			case STATE_POWEROFF:
				PORTA.OUT |= PORT_OUT1;
				state = STATE_DELAY;
				poweroffDelay = 1;
				powerOnOffTime = tmpNow;
				break;
			default:
				break;
		}

		wdt_reset();
	}
}

ISR(PORTA_PORT_vect)
{
	PORTA.INTFLAGS = PORT_INT7_bm | PORT_INT3_bm | PORT_INT2_bm;
	interrupt = 1;
}

ISR(USART0_RXC_vect)
{
	uint8_t data = USART0.RXDATAL;
	if(uartDirection == UART_DIR_RX)
	{
		uartData = data;
		uartNewData = 1;
	}
}

ISR(USART0_TXC_vect)
{
	USART0.STATUS = USART_TXCIF_bm; // NOTE: This is not automatically cleared in loopback/one-wire mode!
	if(cmdDataIdx >= CMDDATA_BUFF)
	{
		uartDirection = UART_DIR_RX;
	}
}

ISR(USART0_DRE_vect)
{
	if(cmdDataIdx < CMDDATA_BUFF)
	{
		uartDirection = UART_DIR_TX;
		USART0.TXDATAL = cmdData[cmdDataIdx];
		cmdDataIdx++;

		if(cmdDataIdx >= CMDDATA_BUFF)
			USART0.CTRLA &= ~USART_DREIE_bm;
	}
	//else
	//	USART0.CTRLA &= ~USART_DREIE_bm;
}

ISR(RTC_PIT_vect)
{
	RTC.PITINTFLAGS = RTC_PI_bm;
	++now;
	interrupt = 1;
}

ISR(BOD_VLM_vect)
{
	BOD.INTFLAGS = BOD_VLMIF_bm;
	vlmDetected = 1;
}
