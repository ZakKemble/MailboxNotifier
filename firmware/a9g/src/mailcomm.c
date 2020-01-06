/*
 * Project: Remote Mail Notifier (and GPS Tracker)
 * Author: Zak Kemble, contact@zakkemble.net
 * Copyright: (C) 2020 by Zak Kemble
 * License: 
 * Web: https://blog.zakkemble.net/remote-mail-notifier-and-gps-tracker/
 */

#include "common.h"

#define BUFF_LEN (8 + 1) // 1 extra for loopback
static char buff[BUFF_LEN];
static uint8_t buffIdx;

void mailcomm_init()
{
    // UART1 for MCU comms
    UART_Config_t uartConfig = {
        .baudRate = UART_BAUD_RATE_9600,
        .dataBits = UART_DATA_BITS_8,
        .stopBits = UART_STOP_BITS_1,
        .parity   = UART_PARITY_NONE,
        .rxCallback = NULL,
		.errorCallback = NULL,
        .useEvent   = true
    };
    UART_Init(UART1, uartConfig);
}

void mailcomm_request()
{
	buffIdx = 0;
	uint8_t data = MAIL_COMM_REQUEST;
	UART_Write(UART1, &data, 1);
	PRINTD("req action");
}

void mailcomm_keepalive()
{
	buffIdx = 0;
	uint8_t data = MAIL_COMM_KEEPALIVE;
	UART_Write(UART1, &data, 1);
	PRINTD("keepalive");
}

void mailcomm_poweroff(uint8_t status)
{
	buffIdx = 0;
	uint8_t data = (status<<3) | MAIL_COMM_POWEROFF;
	UART_Write(UART1, &data, 1);
	PRINTD("req poweroff %u", status);
}

uint8_t* mailcomm_getBuff()
{
	return buff;
}

static void uartStuff(uint32_t len, uint8_t* data)
{
#if DEBUG
	char dbg[128];
	uint8_t idx = 0;
	for(uint32_t i=0;i<len;i++)
	{
		idx += sprintf(dbg + idx, "%02x ", data[i]);
		if(idx > sizeof(dbg) - 5)
			break;
	}
	PRINTD("UART1: %s", dbg);
#endif

	for(uint32_t i=0;i<len;i++)
	{
		if(buffIdx < BUFF_LEN)
		{
			//PRINTD("UART1: %02x", data[i]);
			buff[buffIdx] = data[i];
			buffIdx++;
		}
	}

	if(buffIdx >= BUFF_LEN)
	{
		buffIdx = 0;
		if(buff[0] == MAIL_COMM_REQUEST && buff[1] == MAIL_COMM_DO)
		{
			PRINTD("Got DO command");
			mail_sendEvent(MAILBOX_EVT_MAILCOMM_RESPONSE, MAIL_COMM_DO, 0, NULL, NULL);
		}
	}
}

void mailcomm_event(API_Event_t* pEvent)
{
    switch(pEvent->id)
    {
		case API_EVENT_ID_UART_RECEIVED:
			if(pEvent->param1 == UART1)
			{
				//PRINTD("UART1: %s", (char*)pEvent->pParam1);
				//PRINTD("UART1: %02x %02x %02x %02x %02x", pEvent->pParam1[0], pEvent->pParam1[1], pEvent->pParam1[2], pEvent->pParam1[3], pEvent->pParam1[4]);
				uartStuff(pEvent->param2, (uint8_t*)pEvent->pParam1);
				
				/*if(((char*)pEvent->pParam1)[0] == 'a')
					gsm_connect();
				else if(((char*)pEvent->pParam1)[0] == 's')
					gsm_disconnect();
				else if(((char*)pEvent->pParam1)[0] == 'd')
					gprs_connect();
				else if(((char*)pEvent->pParam1)[0] == 'f')
					gprs_disconnect();
				else if(((char*)pEvent->pParam1)[0] == 'g')
					sms_send("2732", "BAL");
				else if(((char*)pEvent->pParam1)[0] == 'h')
				{
					http_begin(callback_HTTP);
				}*/
			}
			break;
		default:
			break;
	}
}