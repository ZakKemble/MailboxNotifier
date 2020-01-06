/*
 * Project: Remote Mail Notifier (and GPS Tracker)
 * Author: Zak Kemble, contact@zakkemble.net
 * Copyright: (C) 2020 by Zak Kemble
 * License: 
 * Web: https://blog.zakkemble.net/remote-mail-notifier-and-gps-tracker/
 */

#ifndef __GSM_H_
#define __GSM_H_

void gsm_init(void);
void gsm_connect(void);
void gsm_disconnect(void);
void gsm_event(API_Event_t* pEvent);

#endif
