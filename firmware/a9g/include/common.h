/*
 * Project: Remote Mail Notifier (and GPS Tracker)
 * Author: Zak Kemble, contact@zakkemble.net
 * Copyright: (C) 2020 by Zak Kemble
 * License: 
 * Web: https://blog.zakkemble.net/remote-mail-notifier-and-gps-tracker/
 */

#ifndef __COMMON_H_
#define __COMMON_H_

#include <string.h>
#include <stdio.h>
#include <api_os.h>
#include <api_gps.h>
#include <api_event.h>
#include <api_hal_uart.h>
#include <api_debug.h>
#include "stdint.h"
#include "stdbool.h"
#include "stdlib.h"

#include "../config.h"

#include "debug.h"

#include "buffer.h"
#include "gps_parse.h"
#include "math.h"
#include "gps.h"
#include "time.h"
#include "assert.h"
#include "api_sim.h"
#include "api_info.h"
#include "api_hal_gpio.h"
#include "api_hal_pm.h"
#include "api_socket.h"
#include "api_network.h"
#include "api_call.h"
#include "api_audio.h"
#include "api_hal_i2c.h"
#include "api_sms.h"

#include "cJSON.h"

#include "main.h"
#include "mailbox.h"
#include "gprs.h"
#include "sms.h"
#include "gsm.h"
#include "http.h"
#include "bme280.h"
#include "mailcomm.h"
#include "mailcomm_defs.h"
#include "led.h"

#endif
