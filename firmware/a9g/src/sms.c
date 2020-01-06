/*
 * Project: Remote Mail Notifier (and GPS Tracker)
 * Author: Zak Kemble, contact@zakkemble.net
 * Copyright: (C) 2020 by Zak Kemble
 * License: 
 * Web: https://blog.zakkemble.net/remote-mail-notifier-and-gps-tracker/
 */

#include "common.h"

// BUG: Unicode SMSs will leak memory when listing stored messages
// BUG: Unicode SMSs are not listed when listing stored messages
// BUG: If a message is received from a number that begins with a space then the number will not be stored (number field will be blank when listing messages)
// Avoid listing messages

static SMSList_Callback_t smsList_callback;
static SMSNewMessage_Callback_t smsNewMessage_callback;
//static uint8_t unicodeBug;

void sms_init()
{
	if(!SMS_SetFormat(SMS_FORMAT_TEXT, SIM0))
		DBG_SMS("Set format error");

	SMS_Parameter_t smsParam = {
		.fo = 17 ,
		.vp = 167,
		.pid= 0  ,
		.dcs= 0// 8  ,//0:English 7bit, 4:English 8 bit, 8:Unicode 2 Bytes
	};

	if(!SMS_SetParameter(&smsParam,SIM0))
		DBG_SMS("Set parameter error");

	if(!SMS_SetNewMessageStorage(SMS_STORAGE_SIM_CARD))
		DBG_SMS("Set message storage fail");
}

uint8_t sms_clearAll()
{
	// 0 = there are still some SMSs to delete
	// 1 = all SMSs deleted

	SMS_Storage_Info_t storageInfo;
	
	if(SMS_GetStorageInfo(&storageInfo, SMS_STORAGE_SIM_CARD))
	{
		DBG_SMS("SIM card storage: %u/%u", storageInfo.used, storageInfo.total);

		// Delete starting from first SMS to however many SMSs are stored (only properly works if there are no holes in stored message IDs)
		uint8_t i = 1;
		for(;i<storageInfo.used;i++)
		{
			// SMS_DeleteMessage() always returns true even when deleting non-existing SMSs?
			if(SMS_DeleteMessage(i, SMS_STATUS_ALL, SMS_STORAGE_SIM_CARD))
				DBG_SMS("Delete %u success", i);
			else
				DBG_SMS("Delete %u fail", i);
		}

		// Check to make sure all deleted
		// If still some remaining, then continue until max ID stored in SIM card
		for(;
			SMS_GetStorageInfo(&storageInfo, SMS_STORAGE_SIM_CARD) &&
			i<storageInfo.total+1 &&
			storageInfo.used > 0
		;i++)
		{
			// SMS_DeleteMessage() always returns true even when deleting non-existing SMSs?
			if(SMS_DeleteMessage(i, SMS_STATUS_ALL, SMS_STORAGE_SIM_CARD))
				DBG_SMS("Delete %u success", i);
			else
				DBG_SMS("Delete %u fail", i);
		}

		// Check to see if all SMSs have been deleted
		if(SMS_GetStorageInfo(&storageInfo, SMS_STORAGE_SIM_CARD))
		{
			if(storageInfo.used > 0)
			{
				DBG_SMS("Still %u SMSs left!", storageInfo.used);
				return 0;
			}
			
			return 1;
		}
	}

	DBG_SMS("Error getting storage info");
	return 0;
}

void sms_listCallback(SMSList_Callback_t callback)
{
	smsList_callback = callback;
}

void sms_newMessageCallback(SMSNewMessage_Callback_t callback)
{
	smsNewMessage_callback = callback;
}

uint8_t sms_send(char* number, char* message)
{
	uint8_t* unicode = NULL;
	uint32_t unicodeLen;
	uint8_t ret = 0;

	DBG_SMS("Sending to %s...", number);

	if(!SMS_LocalLanguage2Unicode(message, strlen(message), CHARSET_UTF_8, &unicode, &unicodeLen))
		DBG_SMS("Local to unicode fail");
	else
	{
		if(!SMS_SendMessage(number, unicode, unicodeLen, SIM0))
			DBG_SMS("Send fail");
		else
		{
			DBG_SMS("Send begin success");
			ret = 1;
		}

		OS_Free(unicode);
	}
	
	return ret;
}

void sms_info()
{
	uint8_t addr[32];
	SMS_Server_Center_Info_t sca;
	sca.addr = addr;
	SMS_GetServerCenterInfo(&sca);
	DBG_SMS("service center address: %s, type: %d", sca.addr, sca.addrType);

	SMS_Storage_Info_t storageInfo;

	SMS_GetStorageInfo(&storageInfo, SMS_STORAGE_SIM_CARD);
	DBG_SMS("SIM card storage: %u/%u", storageInfo.used, storageInfo.total);

	SMS_GetStorageInfo(&storageInfo, SMS_STORAGE_FLASH);
	DBG_SMS("Flash storage: %u/%u", storageInfo.used, storageInfo.total);
}

void sms_event(API_Event_t* pEvent)
{
    switch(pEvent->id)
    {
		case API_EVENT_ID_SMS_SENT:
			DBG_SMS("Send message success");
			break;
		case API_EVENT_ID_SMS_RECEIVED:
		{
			DBG_SMS("Received message:");
			SMS_Encode_Type_t encodeType = pEvent->param1;
			uint32_t contentLength = pEvent->param2;
			uint8_t* header = pEvent->pParam1;
			uint8_t* content = pEvent->pParam2;

			DBG_SMS("Header: %s", header);
			DBG_SMS("Length: %d", contentLength);
			if(encodeType == SMS_ENCODE_TYPE_ASCII)
			{
				DBG_SMS("Content (ASCII): %s", content);

			}
			else
			{
				//unicodeBug = 1;

				uint8_t tmp[500];
				memset(tmp, 0, 500);
				for(int i=0;i<contentLength;i+=2)
					sprintf(tmp + strlen(tmp), "\\u%02x%02x", content[i], content[i+1]);
				DBG_SMS("Content (unicode): %s", tmp); //you can copy this string to http://tool.chinaz.com/tools/unicode.aspx and display as Chinese
				uint8_t* gbk = NULL;
				uint32_t gbkLen = 0;
				if(!SMS_Unicode2LocalLanguage(content, contentLength, CHARSET_UTF_8, &gbk, &gbkLen))
					DBG_SMS("Convert unicode to UTF8 fail");
				else
				{
					memset(tmp, 0, 500);
					for(int i=0;i<gbkLen;i+=2)
						sprintf(tmp + strlen(tmp), "%02x%02x ", gbk[i], gbk[i+1]);
					DBG_SMS("Content (UTF8): %s", tmp); //you can copy this string to http://m.3158bbs.com/tool-54.html# and display as Chinese
					
					
				}
				OS_Free(gbk);
			}

			if(smsNewMessage_callback != NULL)
				smsNewMessage_callback(encodeType, contentLength, header, content);
		}
			break;
		case API_EVENT_ID_SMS_LIST_MESSAGE:
		{
			// BUG: https://github.com/Ai-Thinker-Open/GPRS_C_SDK/issues/370
			// BUG: https://github.com/Ai-Thinker-Open/GPRS_C_SDK/issues/371

			SMS_Message_Info_t* messageInfo = (SMS_Message_Info_t*)pEvent->pParam1;
			if(smsList_callback != NULL)
				smsList_callback(messageInfo);
			OS_Free(messageInfo->data);
/*
			DBG_SMS(
				"MSG header: index: %d, status: %d, number type: %d, number: %s, time:\"%u/%02u/%02u,%02u:%02u:%02u+%02d\"",
				messageInfo->index,
				messageInfo->status,
				messageInfo->phoneNumberType,
				messageInfo->phoneNumber,
				messageInfo->time.year,
				messageInfo->time.month,
				messageInfo->time.day,
				messageInfo->time.hour,
				messageInfo->time.minute,
				messageInfo->time.second,
				messageInfo->time.timeZone
			);
			DBG_SMS("MSG content: len: %d, data: %s", messageInfo->dataLen, messageInfo->data);
			//need to free data here
			OS_Free(messageInfo->data);
			if(SMS_DeleteMessage(messageInfo->index, SMS_STATUS_ALL, SMS_STORAGE_SIM_CARD))
				DBG_SMS("Message deleted");
			else
				DBG_SMS("Message delete failed");
*/
			break;
		}
		case API_EVENT_ID_SMS_ERROR:
			DBG_SMS("Error occured! cause: %d", pEvent->param1);
			// TODO send error event
        default:
            break;
    }
}
