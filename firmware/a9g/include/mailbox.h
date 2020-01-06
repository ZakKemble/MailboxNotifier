/*
 * Project: Remote Mail Notifier (and GPS Tracker)
 * Author: Zak Kemble, contact@zakkemble.net
 * Copyright: (C) 2020 by Zak Kemble
 * License: 
 * Web: https://blog.zakkemble.net/remote-mail-notifier-and-gps-tracker/
 */

#ifndef __MAILBOX_H_
#define __MAILBOX_H_

// API_EVENT_ID_MAX = 45

#define TRK_EVENT_GPS_STATE		API_EVENT_ID_MAX + 1
#define TRK_EVENT_GSM_STATE		API_EVENT_ID_MAX + 2
#define TRK_EVENT_GPRS_STATE	API_EVENT_ID_MAX + 3
// 4
#define TRK_EVENT_TICK			API_EVENT_ID_MAX + 5
#define MAILBOX_EVT_BEGIN		API_EVENT_ID_MAX + 6
#define TRK_EVENT_PROC_ACTION	API_EVENT_ID_MAX + 7
#define TRK_EVENT_REQ_BAL		API_EVENT_ID_MAX + 8
#define TRK_EVENT_REQ_GPRS		API_EVENT_ID_MAX + 9
#define TRK_EVENT_REQ_GSM		API_EVENT_ID_MAX + 10
#define MAILBOX_EVENT_GSM_CONNECTED			API_EVENT_ID_MAX + 11
#define MAILBOX_EVENT_GSM_DISCONNECTED		API_EVENT_ID_MAX + 12
#define MAILBOX_EVENT_GSM_LOST				API_EVENT_ID_MAX + 13
#define MAILBOX_EVENT_GPRS_CONNECTED		API_EVENT_ID_MAX + 14
#define MAILBOX_EVENT_GPRS_DISCONNECTED		API_EVENT_ID_MAX + 15
#define MAILBOX_EVENT_GPRS_FAIL				API_EVENT_ID_MAX + 16
#define MAILBOX_EVT_HTTPREADY	API_EVENT_ID_MAX + 17
#define MAILBOX_EVT_GOTBAL	API_EVENT_ID_MAX + 18
#define MAILBOX_EVT_MAILCOMM_RESPONSE	API_EVENT_ID_MAX + 19
#define MAILBOX_EVT_HTTP_BEGIN	API_EVENT_ID_MAX + 20
#define MAILBOX_EVT_HTTP_DNSFAIL	API_EVENT_ID_MAX + 21
#define MAILBOX_EVT_HTTP_CONNECTED	API_EVENT_ID_MAX + 22
#define MAILBOX_EVT_HTTP_RECV	API_EVENT_ID_MAX + 23
#define MAILBOX_EVT_HTTP_CLOSE	API_EVENT_ID_MAX + 24
#define MAILBOX_EVT_HTTP_ERROR	API_EVENT_ID_MAX + 25

#define MAILBOX_EVT_	API_EVENT_ID_MAX + 19

typedef uint32_t millis_t;

void mailbox_eventDispatch(API_Event_t* event);

void mailbox_task(void *pData);
void mail_sendEvent(uint32_t id, uint32_t param1, uint32_t param2, void* pParam1, void* pParam2);
millis_t millis(void);

#endif
