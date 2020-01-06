/*
 * Project: Remote Mail Notifier (and GPS Tracker)
 * Author: Zak Kemble, contact@zakkemble.net
 * Copyright: (C) 2020 by Zak Kemble
 * License: 
 * Web: https://blog.zakkemble.net/remote-mail-notifier-and-gps-tracker/
 */

#ifndef __MAILCOMM_DEFS_H_
#define __MAILCOMM_DEFS_H_

// Commands use bits [0:2] (max command ID = 7)
// Bits [3:7] are for data

#define MAIL_COMM_RESERVED			0x00
#define MAIL_COMM_REQUEST			0x01
#define MAIL_COMM_DO				0x02
#define MAIL_COMM_KEEPALIVE			0x03
#define MAIL_COMM_POWEROFF			0x04
#define MAIL_COMM_POWERCYCLE		0x05

#endif
