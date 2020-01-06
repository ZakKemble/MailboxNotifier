/*
 * Project: Remote Mail Notifier (and GPS Tracker)
 * Author: Zak Kemble, contact@zakkemble.net
 * Copyright: (C) 2020 by Zak Kemble
 * License: 
 * Web: https://blog.zakkemble.net/remote-mail-notifier-and-gps-tracker/
 */

#ifndef CONFIG_H_
#define CONFIG_H_

#define FW_VERSION	"1.0.0 200103"

#define HTTP_API_KEY	"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"

#define APN_NAME	"apnname"
#define APN_USER	"apnuser"
#define APN_PASS	"apnpass"

#define BAL_NUM_SEND	"0000"
#define BAL_NUM_RECV	" 0000" // A9G BUG: Short numbers start with a space for some reason
#define BAL_MSG			"BAL"

#define HTTP_HOST	"example.com"
#define HTTP_PORT	80
#define HTTP_PATH	"/mailnotifier.php"

#define DEBUG 1 // Disable all *_DBG() and PRINTD() messages

#define DEBUG_SMS 1
#define DEBUG_GSM 1
#define DEBUG_GPS 1
#define DEBUG_MAIL 1
#define DEBUG_GPRS 1
#define DEBUG_SMTP 1
#define DEBUG_HTTP 1

#endif /* CONFIG_H_ */
