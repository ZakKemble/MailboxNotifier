/*
 * Project: Remote Mail Notifier (and GPS Tracker)
 * Author: Zak Kemble, contact@zakkemble.net
 * Copyright: (C) 2020 by Zak Kemble
 * License: 
 * Web: https://blog.zakkemble.net/remote-mail-notifier-and-gps-tracker/
 */

#ifndef __SMS_H_
#define __SMS_H_

typedef void (*SMSList_Callback_t)(SMS_Message_Info_t* messageInfo);
typedef void (*SMSNewMessage_Callback_t)(SMS_Encode_Type_t encodeType, uint32_t contentLength, uint8_t* header, uint8_t* content);

void sms_init(void);
uint8_t sms_clearAll(void);
void sms_listCallback(SMSList_Callback_t callback);
void sms_newMessageCallback(SMSNewMessage_Callback_t callback);
void sms_info(void);
uint8_t sms_send(char* number, char* message);
void sms_event(API_Event_t* pEvent);

#endif
