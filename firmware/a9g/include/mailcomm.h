/*
 * Project: Remote Mail Notifier (and GPS Tracker)
 * Author: Zak Kemble, contact@zakkemble.net
 * Copyright: (C) 2020 by Zak Kemble
 * License: 
 * Web: https://blog.zakkemble.net/remote-mail-notifier-and-gps-tracker/
 */

#ifndef __MAILCOMM_H_
#define __MAILCOMM_H_

void mailcomm_init(void);
void mailcomm_request(void);
void mailcomm_keepalive(void);
void mailcomm_poweroff(uint8_t status);
uint8_t* mailcomm_getBuff(void);
void mailcomm_update(void);
void mailcomm_event(API_Event_t* pEvent);

#endif
