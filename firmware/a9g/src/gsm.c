/*
 * Project: Remote Mail Notifier (and GPS Tracker)
 * Author: Zak Kemble, contact@zakkemble.net
 * Copyright: (C) 2020 by Zak Kemble
 * License: 
 * Web: https://blog.zakkemble.net/remote-mail-notifier-and-gps-tracker/
 */

#include "common.h"

static uint8_t status; // Used to figure out if we auto-reconnected to GSM network after signal loss

void gsm_init()
{
	Network_DeRegister();
}

void gsm_connect()
{
	Network_Register((uint8_t[]){0,0,0,0,0,0}, NETWORK_REGISTER_MODE_MANUAL);
}

void gsm_disconnect()
{
	Network_DeRegister();
}

void gsm_event(API_Event_t* pEvent)
{
    switch(pEvent->id)
    {
        case API_EVENT_ID_NETWORK_REGISTERED_HOME:
			{
				DBG_GSM("Network register success (home)");
				led_rate(LED_NETWORK, LED_RATE_NETWORK_GSM);
				mail_sendEvent(MAILBOX_EVENT_GSM_CONNECTED, (status == 0), 0, NULL, NULL);
				status = 1;
/*
				bool ret = true;
				uint8_t index = 0;
				uint8_t* operatorId;
				uint8_t* operatorName;
				while(ret)
				{
					ret = Network_GetOperatorInfo(index++,&operatorId,&operatorName);
					if(!ret)
						DBG_GSM("ID:%d %d %d %d %d %d,name:%s",operatorId[0],operatorId[1],operatorId[2],operatorId[3],operatorId[4],operatorId[5],operatorName);
					else
						DBG_GSM("AAA %d", index);
				}
				
				uint8_t* operatorId2;
				Network_GetOperatorIdByName("T-Mobile", &operatorId2);
				DBG_GSM("ID:%d %d %d %d %d %d",operatorId2[0],operatorId2[1],operatorId2[2],operatorId2[3],operatorId2[4],operatorId2[5]);
				
				uint8_t operatorId3[6];
				operatorId3[0] = 2;
				operatorId3[1] = 3;
				operatorId3[2] = 4;
				operatorId3[3] = 3;
				operatorId3[4]= 0;
				operatorId3[5] = 15;
				uint8_t* operatorName2;
				Network_GetOperatorNameById(operatorId3,&operatorName2);
				DBG_GSM("name:%s",operatorName2);

				Network_GetAvailableOperatorReq();
*/
			}
			break;
        case API_EVENT_ID_NETWORK_REGISTERED_ROAMING:
			DBG_GSM("Network register success (roaming)");
			led_rate(LED_NETWORK, LED_RATE_NETWORK_GSM);
			mail_sendEvent(MAILBOX_EVENT_GSM_CONNECTED, (status == 0), 0, NULL, NULL);
			status = 1;
			break;
		case API_EVENT_ID_NETWORK_REGISTER_SEARCHING:
			DBG_GSM("Searching... (no signal?)");
			led_rate(LED_NETWORK, LED_RATE_NETWORK_NO_SIGNAL);
			//status = 0;
			mail_sendEvent(MAILBOX_EVENT_GSM_LOST, 0, 0, NULL, NULL);
			break;
		case API_EVENT_ID_NETWORK_REGISTER_DENIED:
			DBG_GSM("Network register denied");
			led_rate(LED_NETWORK, LED_RATE_NETWORK_NO_SIGNAL);
			//status = 0;
			break;
		case API_EVENT_ID_NETWORK_REGISTER_NO: // Dunno what NO means?
			DBG_GSM("Network register no?");
			//status = 0;
			break;
		case API_EVENT_ID_NETWORK_DEREGISTER:
			DBG_GSM("Network deregistered");
			led_rate(LED_NETWORK, LED_RATE_NETWORK_OFF);
			status = 0;
			mail_sendEvent(MAILBOX_EVENT_GSM_DISCONNECTED, 0, 0, NULL, NULL);
			break;
		case API_EVENT_ID_SIGNAL_QUALITY:
			DBG_GSM("SIG: %d %d", pEvent->param1, pEvent->param2);
			break;



		case API_EVENT_ID_CALL_DIAL:
			break;
        case API_EVENT_ID_CALL_HANGUP:  //param1: is remote release call, param2:error code(CALL_Error_t)
            DBG_GSM("Hang up, remote hang up: %d, err: %d", pEvent->param1, pEvent->param2);
            break;
        case API_EVENT_ID_CALL_INCOMING:   //param1: number type, pParam1:number
            DBG_GSM("Incoming call, num: %s, type: %d",pEvent->pParam1,pEvent->param1);
            if(!CALL_Answer())
                DBG_GSM("Answer fail");
            break;
        case API_EVENT_ID_CALL_ANSWER:  
            DBG_GSM("Answer success");
			AUDIO_SetMode(AUDIO_MODE_LOUDSPEAKER); // Increases mic sensitivity
            break;
        case API_EVENT_ID_CALL_DTMF: //param1: key
		{
            DBG_GSM("DTMF tone: %c", pEvent->param1);
			//CALL_DTMF(pEvent->param1, CALL_DTMF_GAIN_0dB, 100, 0, true);
			//AUDIO_SetMode(AUDIO_MODE_LOUDSPEAKER);
			/*if(pEvent->param1 == '1')
			{
				DBG_GSM("MIC MUTE");
				//AUDIO_MicSetMute(true);
				// BUG: mic will be stuck muted until moduule reset
				// https://github.com/Ai-Thinker-Open/GPRS_C_SDK/issues/331
			}
			else
			{
				DBG_GSM("MIC UNMUTE");
				//AUDIO_MicSetMute(false);
			}*/
		}
            break;
        default:
            break;
    }
}
