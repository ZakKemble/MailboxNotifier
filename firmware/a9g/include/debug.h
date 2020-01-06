/*
 * Project: Remote Mail Notifier (and GPS Tracker)
 * Author: Zak Kemble, contact@zakkemble.net
 * Copyright: (C) 2020 by Zak Kemble
 * License: 
 * Web: https://blog.zakkemble.net/remote-mail-notifier-and-gps-tracker/
 */

#ifndef __DEBUG_H_
#define __DEBUG_H_

#define PRINTD(fmt, ...) \
            do { if (DEBUG) Trace(1, ":(%u)(DBG)"fmt, millis(), ## __VA_ARGS__); } while (0)

#define DBG_SMS(fmt, ...) \
            do { if (DEBUG && DEBUG_SMS) printf(":(%u)(SMS)"fmt, millis(), ## __VA_ARGS__); } while (0)

#define DBG_GSM(fmt, ...) \
            do { if (DEBUG && DEBUG_GSM) printf(":(%u)(GSM)"fmt, millis(), ## __VA_ARGS__); } while (0)

#define DBG_GPRS(fmt, ...) \
            do { if (DEBUG && DEBUG_GPRS) printf(":(%u)(GPRS)"fmt, millis(), ## __VA_ARGS__); } while (0)
				
#define DBG_MAIL(fmt, ...) \
            do { if (DEBUG && DEBUG_MAIL) printf(":(%u)(MAIL)"fmt, millis(), ## __VA_ARGS__); } while (0)

#define DBG_SMTP(fmt, ...) \
            do { if (DEBUG && DEBUG_SMTP) printf(":(%u)(SMTP)"fmt, millis(), ## __VA_ARGS__); } while (0)

#define DBG_HTTP(fmt, ...) \
            do { if (DEBUG && DEBUG_HTTP) printf(":(%u)(HTTP)"fmt, millis(), ## __VA_ARGS__); } while (0)

#endif
