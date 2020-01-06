/*
 * Project: Remote Mail Notifier (and GPS Tracker)
 * Author: Zak Kemble, contact@zakkemble.net
 * Copyright: (C) 2020 by Zak Kemble
 * License: 
 * Web: https://blog.zakkemble.net/remote-mail-notifier-and-gps-tracker/
 */

#include "common.h"

static Network_PDP_Context_t context = {
	.apn		= APN_NAME,
	.userName	= APN_USER,
	.userPasswd	= APN_PASS
};

static uint8_t enable;
static uint8_t busy;
static uint8_t bugLockout; // BUG: Module bugs out if GPRS is deactivated then activated again (work-around found: https://github.com/Ai-Thinker-Open/GPRS_C_SDK/issues/329#issuecomment-557505459)

static void onNetworkStatusChange(Network_Status_t status)
{
	switch(status)
	{
		case NETWORK_STATUS_OFFLINE:
			DBG_GPRS("CB GPRS offline");
			break;
		case NETWORK_STATUS_REGISTERING:
			DBG_GPRS("CB Registering...");
			break;
		case NETWORK_STATUS_REGISTERED:
			DBG_GPRS("CB Registered");
			break;
		case NETWORK_STATUS_DETACHED:
			DBG_GPRS("CB Detached");
			break;
		case NETWORK_STATUS_ATTACHING:
			DBG_GPRS("CB Attaching...");
			break;
		case NETWORK_STATUS_ATTACHED:
			DBG_GPRS("CB Attached");
			break;
		case NETWORK_STATUS_DEACTIVED:
			DBG_GPRS("CB Deactivated");
			break;
		case NETWORK_STATUS_ACTIVATING:
			DBG_GPRS("CB Activating...");
			break;
		case NETWORK_STATUS_ACTIVATED:
			DBG_GPRS("CB Activated");
			break;
		case NETWORK_STATUS_ATTACH_FAILED:
			DBG_GPRS("CB Attach failed");
			break;
		case NETWORK_STATUS_ACTIVATE_FAILED:
			DBG_GPRS("CB Activate failed");
			break;
		default:
			DBG_GPRS("CB UNKNOWN %d", status);
			break;
	}
}

void gprs_init()
{
	Network_SetStatusChangedCallback(onNetworkStatusChange);
}

static void processConnection(void)
{
	busy = 0;

	if(bugLockout)
		return;

	uint8_t attachStatus;
	uint8_t activeStatus;
	bool attachRet = Network_GetAttachStatus(&attachStatus);
	bool activeRet = Network_GetActiveStatus(&activeStatus);
	
	if(!attachRet)
		DBG_GPRS("get attach staus fail");

	if(!activeRet)
		DBG_GPRS("get active staus fail");
	
	DBG_GPRS("Status - Attach: %d, Active: %d", attachStatus, activeStatus);

	if(!attachStatus && !activeStatus && enable)
	{
		if(!Network_StartAttach())
			DBG_GPRS("start network attach fail");
		else
		{
			DBG_GPRS("Start attach");
			busy = 1;
		}
	}
	else if(attachStatus && !activeStatus)
	{
		if(enable)
		{
			if(!Network_StartActive(context))
				DBG_GPRS("start network active fail");
			else
			{
				DBG_GPRS("Start active");
				busy = 1;
			}
		}
		else
		{
			// BUG: Do not detach, otherwise GSM will automatically re-register after de-registering
			//if(!Network_StartDetach())
			//	DBG_GPRS("start network detach fail");
			//else
			//{
			//	DBG_GPRS("Start detach");
			//	busy = 1;
			//}
		}
	}
	else if(attachStatus && activeStatus && !enable)
	{
		uint8_t i = 0;
		while(!Network_StartDeactive(i) && i < 10)
			i++;
		if(i >= 10)
			DBG_GPRS("Start deactive fail");
		else
		{
			DBG_GPRS("Start deactive: %d", i);
			busy = 1;
		}
	}
}

void gprs_connect()
{
	if(!bugLockout)
	{
		enable = 1;
		if(!busy)
			processConnection();
	}
	else
		mail_sendEvent(MAILBOX_EVENT_GPRS_FAIL, 0, 0, NULL, NULL);
}

void gprs_disconnect()
{
	enable = 0;
	if(!busy)
		processConnection();
}

void gprs_event(API_Event_t* pEvent)
{
	switch(pEvent->id)
	{
		case API_EVENT_ID_NETWORK_ATTACHED:
			DBG_GPRS("network attach success");
			processConnection();
			break;
		case API_EVENT_ID_NETWORK_ATTACH_FAILED:
			DBG_GPRS("network attach fail");
			processConnection();
			mail_sendEvent(MAILBOX_EVENT_GPRS_FAIL, 0, 0, NULL, NULL);
			break;
		case API_EVENT_ID_NETWORK_ACTIVATED:
			DBG_GPRS("network activate success");
			led_rate(LED_NETWORK, LED_RATE_NETWORK_GPRS);
			char ip[16];
			Network_GetIp(ip, 16);
			DBG_GPRS("IP: %s", ip);
			processConnection();
			mail_sendEvent(MAILBOX_EVENT_GPRS_CONNECTED, 0, 0, NULL, NULL);
			break;
		case API_EVENT_ID_NETWORK_ACTIVATE_FAILED:
			DBG_GPRS("network activate fail");
			processConnection();
			mail_sendEvent(MAILBOX_EVENT_GPRS_FAIL, 0, 0, NULL, NULL);
			break;
		case API_EVENT_ID_NETWORK_DETACHED:
			DBG_GPRS("Detached");
			processConnection();
			//bugLockout = 1;
			//mail_sendEvent(MAILBOX_EVENT_GPRS_DISCONNECTED, 0, 0, NULL, NULL);
			break;
		case API_EVENT_ID_NETWORK_DEACTIVED:
			DBG_GPRS("Deactivated");
			led_rate(LED_NETWORK, LED_RATE_NETWORK_GSM);
			processConnection();
			bugLockout = 1;
			mail_sendEvent(MAILBOX_EVENT_GPRS_DISCONNECTED, 0, 0, NULL, NULL);
			break;

		case API_EVENT_ID_NETWORK_GOT_TIME:
			DBG_GPRS("got time");
			break;
		case API_EVENT_ID_NETWORK_CELL_INFO:
			break;
		case API_EVENT_ID_NETWORK_AVAILABEL_OPERATOR:
			break;
		default:
			break;
    }
}
