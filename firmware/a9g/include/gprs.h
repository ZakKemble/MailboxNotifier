/*
 * Project: Remote Mail Notifier (and GPS Tracker)
 * Author: Zak Kemble, contact@zakkemble.net
 * Copyright: (C) 2020 by Zak Kemble
 * License: 
 * Web: https://blog.zakkemble.net/remote-mail-notifier-and-gps-tracker/
 */

#ifndef __GPRS_H_
#define __GPRS_H_

void gprs_init(void);
void gprs_connect(void);
void gprs_disconnect(void);
void gprs_event(API_Event_t* pEvent);

#endif
