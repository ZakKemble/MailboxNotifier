/*
 * Project: Remote Mail Notifier (and GPS Tracker)
 * Author: Zak Kemble, contact@zakkemble.net
 * Copyright: (C) 2020 by Zak Kemble
 * License: 
 * Web: https://blog.zakkemble.net/remote-mail-notifier-and-gps-tracker/
 */

#include "common.h"

#define JOB_RUN		0
#define JOB_UPDATE	1
#define JOB_TIMEOUT	2
#define JOB_EVENT	3
#define JOB_STOP	4

#define SMSBAL_NONE	0
#define SMSBAL_SUCCESS	1
#define SMSBAL_FAIL	2

#define PWROFF_UNKNOWN	0
#define PWROFF_FAILURE	1
#define PWROFF_SUCCESS	2

typedef struct {
	uint16_t success;
	uint16_t failure;
	uint16_t timeout;
} counts_t;

typedef struct {
	uint8_t newmail;
	uint8_t endcharging;
	uint8_t trackMode;
	uint8_t switchstuck;
} reasons_t;

typedef struct {
	uint8_t get;
	uint8_t state;
	char content[26];
	char dateTime[23];
} smsBalance_t;

typedef struct job_t job_t;
typedef uint8_t (*onProcess_t)(job_t* job, uint8_t action, void* data);
typedef void (*onComplete_t)(void* param, uint8_t success);

struct job_t {
	uint8_t running;
	millis_t startTime;
	uint8_t retries;
    millis_t timeout;
	uint8_t maxReties;
	onProcess_t onProcess;
	onComplete_t onComplete;
	void* onCompleteParam;
	// onFailure?
	// onSuccess?
	// onComplete?
	// onStop?
};

static uint8_t job_process_clearSMSs(job_t* job, uint8_t action, void* data);
static uint8_t job_process_environmentData(job_t* job, uint8_t action, void* data);
static uint8_t job_process_requestInfo(job_t* job, uint8_t action, void* data);
static uint8_t job_process_gsmConnect(job_t* job, uint8_t action, void* data);
static uint8_t job_process_smsBalance(job_t* job, uint8_t action, void* data);
static uint8_t job_process_waitAttach(job_t* job, uint8_t action, void* data);
static uint8_t job_process_gprsConnect(job_t* job, uint8_t action, void* data);
static uint8_t job_process_gps(job_t* job, uint8_t action, void* data);
static uint8_t job_process_http(job_t* job, uint8_t action, void* data);
static uint8_t job_process_gprsDisconnect(job_t* job, uint8_t action, void* data);
static uint8_t job_process_gsmDisconnect(job_t* job, uint8_t action, void* data);
static uint8_t job_process_requestPoweroff(job_t* job, uint8_t action, void* data);

static job_t job_clearSMSs = {
	0, 0, 0,
	5000,
	0,
	job_process_clearSMSs,
	NULL,
	NULL
};

static job_t job_environmentData = {
	0, 0, 0,
	5000,
	1,
	job_process_environmentData,
	NULL,
	NULL
};

static job_t job_requestInfo = {
	0, 0, 0,
	1000,
	1,
	job_process_requestInfo,
	NULL,
	NULL
};

static job_t job_gsmConnect = {
	0, 0, 0,
	70000,
	0,
	job_process_gsmConnect,
	NULL,
	NULL
};

static job_t job_waitAttach = {
	0, 0, 0,
	20000,
	0,
	job_process_waitAttach,
	NULL,
	NULL
};

static job_t job_smsBalance = {
	0, 0, 0,
	20000,
	1,
	job_process_smsBalance,
	NULL,
	NULL
};

static job_t job_gprsConnect = {
	0, 0, 0,
	60000,
	0,
	job_process_gprsConnect,
	NULL,
	NULL
};

static job_t job_gps = {
	0, 0, 0,
	0,
	0,
	job_process_gps,
	NULL,
	NULL
};

static job_t job_http = {
	0, 0, 0,
	30000,
	1,
	job_process_http,
	NULL,
	NULL
};

static job_t job_gprsDisconnect = {
	0, 0, 0,
	10000,
	0,
	job_process_gprsDisconnect,
	NULL,
	NULL
};

static job_t job_gsmDisconnect = {
	0, 0, 0,
	10000,
	0,
	job_process_gsmDisconnect,
	NULL,
	NULL
};

static job_t job_requestPoweroff = {
	0, 0, 0,
	1000,
	1,
	job_process_requestPoweroff,
	NULL,
	NULL
};

static job_t* jobs[] = {
	&job_clearSMSs,
	&job_environmentData,
	&job_requestInfo,
	&job_gsmConnect,
	&job_waitAttach,
	&job_smsBalance,
	&job_gprsConnect,
	&job_gps,
	&job_http,
	&job_gprsDisconnect,
	&job_gsmDisconnect,
	&job_requestPoweroff
};

extern char* fwBuild;
static HANDLE mailboxTaskHandle = NULL;
static counts_t counts;
static reasons_t reasons;
static uint8_t vlmDetected;
static smsBalance_t smsBalance;
static int fd_http;
static uint8_t fd_http_closing;
static uint8_t battPercent;
static uint16_t battVoltage;
static uint8_t powerOffStatus;

static void printStackHeap(void)
{
	OS_Task_Info_t info;
	OS_GetTaskInfo(mailboxTaskHandle, &info);
	volatile uint32_t j = 0;
	uint32_t last_bytes = (uint32_t)&j - info.stackTop;
	uint32_t all_bytes  = info.stackSize * 4;

	OS_Heap_Status_t osHeapStatus;
	OS_GetHeapUsageStatus(&osHeapStatus);

	PRINTD("STACK: %u/%u | HEAP: %u/%u", all_bytes - last_bytes, all_bytes, osHeapStatus.usedSize, osHeapStatus.totalSize);
}

static void callback_smsList(SMS_Message_Info_t* messageInfo)
{
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
}

static void callback_smsNewMessage(SMS_Encode_Type_t encodeType, uint32_t contentLength, uint8_t* header, uint8_t* content)
{
	// " 2732",,"2019/08/10,20:41:52+01",129,17,0,0,"+447958879880",145,21
	if(
		smsBalance.state != SMSBAL_SUCCESS &&
		encodeType == SMS_ENCODE_TYPE_ASCII &&
		contentLength < sizeof(smsBalance.content) &&
		memcmp(header, "\"" BAL_NUM_RECV "\"", 7) == 0
	)
	{
		memcpy(smsBalance.dateTime, header + 10, sizeof(smsBalance.dateTime) - 1);
		smsBalance.dateTime[sizeof(smsBalance.dateTime) - 1] = '\0';
		strncpy(smsBalance.content, content, sizeof(smsBalance.content));
		smsBalance.content[sizeof(smsBalance.content) - 1] = '\0';
		
		// Sanitise string so JSON stuff doesn't mess up (mainly the £ pound symbol)
		// TODO make the pound symbol work
		for(uint8_t i=0;i<sizeof(smsBalance.content);i++)
		{
			if(smsBalance.content[i] == '\0')
				break;
			else if(smsBalance.content[i] < ' ' || smsBalance.content[i] > '~')
				smsBalance.content[i] = '?';
		}
		
		smsBalance.state = SMSBAL_SUCCESS;

		mail_sendEvent(MAILBOX_EVT_GOTBAL, 0, 0, NULL, NULL);
	}
}

static void job_run(job_t* job, onComplete_t onComplete, void* onCompleteParam)
{
	if(job->running)
		return;
	job->running = 1;
	job->startTime = millis();
	job->retries = 0;
	job->onComplete = onComplete;
	job->onCompleteParam = onCompleteParam;
	if(job->onProcess != NULL)
		job->onProcess(job, JOB_RUN, NULL);
}

static void job_next(job_t* job, job_t* next, onComplete_t onComplete, void* onCompleteParam)
{
	if(job != NULL)
	{
		if(job->onProcess != NULL)
			job->onProcess(job, JOB_STOP, NULL);
		job->running = 0;
	}

	if(next != NULL)
		job_run(next, onComplete, onCompleteParam);
}

static uint8_t job_process_clearSMSs(job_t* job, uint8_t action, void* data)
{
	if(action == JOB_RUN || action == JOB_UPDATE)
	{
		if(action == JOB_RUN)
			DBG_MAIL("JOB RUN: CLEAR SMS...");

		if(sms_clearAll())
		{
			DBG_MAIL("All SMSs deleted");
			job_next(job, &job_environmentData, NULL, NULL);
		}
	}
	else if(action == JOB_TIMEOUT)
	{
		DBG_MAIL("JOB TO: CLEAR SMS");
		job_next(job, &job_environmentData, NULL, NULL);
	}
	else if(action == JOB_EVENT)
	{
	}
	
	return 0;
}

static uint8_t job_process_environmentData(job_t* job, uint8_t action, void* data)
{
	if(action == JOB_RUN)
	{
		DBG_MAIL("JOB RUN: ENV DATA");
		bme280_startConvertion();
	}
	else if(action == JOB_UPDATE)
	{
		if(bme280_status() == 0)
		{
			PRINTD(
				"%u, Temp: %.2f, Press: %.3f, Humidity: %.3f",
				bme280_status(),
				(bme280_readTemperature() / 100.0),
				((bme280_readPressure() / 256.0) / 100.0),
				(bme280_readHumidity() / 1024.0)
			);
			job_next(job, &job_requestInfo, NULL, NULL);
		}
	}
	else if(action == JOB_TIMEOUT)
	{
		DBG_MAIL("JOB TO: ENV DATA");
		job_next(job, &job_requestInfo, NULL, NULL);
	}
	else if(action == JOB_EVENT)
	{
	}
	
	return 0;
}

static uint8_t job_process_requestInfo(job_t* job, uint8_t action, void* data)
{
	if(action == JOB_RUN)
	{
		DBG_MAIL("JOB RUN: REQ INFO...");
		mailcomm_request();
	}
	else if(action == JOB_UPDATE)
	{
	}
	else if(action == JOB_TIMEOUT)
	{
		DBG_MAIL("JOB TO: REQ INFO");
		// Wait for MCU timeout failure...
		// TODO go to 32k low power mode? (not for GPS mode)
	}
	else if(action == JOB_EVENT)
	{
		API_Event_t* event = (API_Event_t*)data;
		switch(event->id)
		{
			case MAILBOX_EVT_MAILCOMM_RESPONSE:
			{
				if(job->running)
				{
					uint8_t* buff = mailcomm_getBuff();
					if(buff != NULL)
					{
						DBG_MAIL("JOB EVT: REQ INFO");
						
						if(reasons.trackMode) 
						{
							job_next(job, NULL, NULL, NULL);

							vlmDetected = (buff[8]>>4) & 0x01;

							// a little bit hacky
							if(!((buff[8]>>1) & 0x01))
								job_next(&job_gps, NULL, NULL, NULL);
						}
						else
						{
							counts.success = 		(buff[2]<<8) | buff[3];
							counts.failure = 		(buff[4]<<8) | buff[5];
							counts.timeout = 		(buff[6]<<8) | buff[7];
							smsBalance.get =		(buff[8]>>5) & 0x01;
							vlmDetected =			(buff[8]>>4) & 0x01;
							reasons.newmail =		(buff[8]>>3) & 0x01;
							reasons.endcharging =	(buff[8]>>2) & 0x01;
							reasons.trackMode =		(buff[8]>>1) & 0x01;
							reasons.switchstuck =	(buff[8]>>0) & 0x01;

							if(reasons.trackMode || reasons.newmail || reasons.endcharging || reasons.switchstuck)
								job_next(job, &job_gsmConnect, NULL, NULL);
							else // Nothing to do
							{
								powerOffStatus = PWROFF_SUCCESS;
								job_next(job, &job_requestPoweroff, NULL, NULL);
							}
						}
					}
				}
			}
				break;
			default:
				break;
		}
	}
	
	return 0;
}

static uint8_t job_process_gsmConnect(job_t* job, uint8_t action, void* data)
{
	if(action == JOB_RUN)
	{
		DBG_MAIL("JOB RUN: GSM CONNECT...");
		gsm_connect();
	}
	else if(action == JOB_UPDATE)
	{
	}
	else if(action == JOB_TIMEOUT)
	{
		DBG_MAIL("JOB TO: GSM CONNECT");
		job_next(job, NULL, NULL, NULL);

		// TODO reboot? wait longer for bad signal?
		// what about tracking mode?

		powerOffStatus = PWROFF_FAILURE;
		job_next(job, &job_gsmDisconnect, NULL, NULL);
	}
	else if(action == JOB_EVENT)
	{
		API_Event_t* event = (API_Event_t*)data;
		switch(event->id)
		{
			case MAILBOX_EVENT_GSM_CONNECTED:
				DBG_MAIL("JOB EVT: GSM CONNECTED %d", event->param1);
				if(job->running)
				{
					if(event->param1 == 1) // 1 = New connection, 0 = Auto-reconnected from lost signal
					{
						job_next(job, &job_waitAttach, NULL, NULL);
					}
				}
				break;
			default:
				break;
		}
	}
	
	return 0;
}

static uint8_t job_process_waitAttach(job_t* job, uint8_t action, void* data)
{
	// Waiting for the attach thing seems be make GPRS and SMS stuff a bit more reliable

	if(action == JOB_RUN)
	{
		DBG_MAIL("JOB RUN: ATTACH WAIT...");
	}
	else if(action == JOB_UPDATE)
	{
		uint8_t attachStatus;
		bool attachRet = Network_GetAttachStatus(&attachStatus);
		if(attachRet && attachStatus)
		{
			DBG_MAIL("JOB UPT: ATTACHED!");
			if(smsBalance.get)
				job_next(job, &job_smsBalance, NULL, NULL);
			else
				job_next(job, &job_gprsConnect, NULL, NULL);
		}
	}
	else if(action == JOB_TIMEOUT)
	{
		DBG_MAIL("JOB TO: ATTACH");
	
		// Carry on anyway and hope for the best...
		if(smsBalance.get)
			job_next(job, &job_smsBalance, NULL, NULL);
		else
			job_next(job, &job_gprsConnect, NULL, NULL);
	}
	else if(action == JOB_EVENT)
	{
	}
	
	return 0;
}

static uint8_t job_process_smsBalance(job_t* job, uint8_t action, void* data)
{
	if(action == JOB_RUN)
	{
		DBG_MAIL("JOB RUN: GET BAL...");
		sms_send(BAL_NUM_SEND, BAL_MSG);
	}
	else if(action == JOB_UPDATE)
	{
	}
	else if(action == JOB_TIMEOUT)
	{
		DBG_MAIL("JOB TO: GET BAL");
		smsBalance.state = SMSBAL_FAIL;
		job_next(job, &job_gprsConnect, NULL, NULL);
	}
	else if(action == JOB_EVENT)
	{
		API_Event_t* event = (API_Event_t*)data;

		switch(event->id)
		{
			case MAILBOX_EVT_GOTBAL:
				if(job->running)
				{
					DBG_MAIL("JOB EVT: GOT BAL");
					job_next(job, &job_gprsConnect, NULL, NULL);
				}
				else
					DBG_MAIL("JOB EVT: GOT UNEXPECTED BAL");
				break;
			default:
				break;
		}
	}
	
	return 0;
}

static void onSingleRequestComplete(void* param, uint8_t success)
{
	// TODO we should wait a few seconds before disconnecting from GPRS so that the FIN,ACK packet from http_close() can reach the server, and maybe
	// receive the ACK response, otherwise the server connection will be stuck in CLOSE_WAIT (or maybe FIN_WAIT2) state for a while.

	powerOffStatus = success ? PWROFF_SUCCESS : PWROFF_FAILURE;
	job_next(NULL, &job_gprsDisconnect, NULL, NULL);
}

static uint8_t job_process_gprsConnect(job_t* job, uint8_t action, void* data)
{
	if(action == JOB_RUN)
	{
		DBG_MAIL("JOB RUN: GPRS CONNECT...");
		gprs_connect();
	}
	else if(action == JOB_UPDATE)
	{
	}
	else if(action == JOB_TIMEOUT)
	{
		// Connecting to GPRS can be glitchy, if it takes too long or fails then the best thing to do is reboot

		DBG_MAIL("JOB TO: GPRS");
		job_next(job, NULL, NULL, NULL);
		// send keepalive
		//DBG_MAIL("Rebooting...");
		//PM_Restart();

		powerOffStatus = PWROFF_FAILURE;
		job_next(job, &job_gsmDisconnect, NULL, NULL);
	}
	else if(action == JOB_EVENT)
	{
		API_Event_t* event = (API_Event_t*)data;
		switch(event->id)
		{
			case MAILBOX_EVENT_GPRS_CONNECTED:
			{
				DBG_MAIL("JOB EVT: GPRS CONNECTED");
				if(job->running)
				{
					// A9G BUG: https://github.com/Ai-Thinker-Open/GPRS_C_SDK/issues/380
					uint8_t isnCount = (counts.success + counts.failure + counts.timeout) & 0x07;
					PRINTD("ISN FIX: %u", isnCount);
					for(uint8_t i=0;i<isnCount;i++)
						Socket_TcpipClose(Socket_TcpipConnect(TCP, "127.0.0.1", 12345));

					if(reasons.trackMode)
						job_next(job, &job_gps, NULL, NULL);
					else if(reasons.newmail || reasons.endcharging || reasons.switchstuck)
						job_next(job, &job_http, onSingleRequestComplete, NULL);
					else // Nothing to do?
					{
						powerOffStatus = PWROFF_SUCCESS;
						job_next(job, &job_gprsDisconnect, NULL, NULL);
					}
				}
			}
				break;
			case MAILBOX_EVENT_GPRS_FAIL:
			{
				if(job->running)
				{
					job_next(job, NULL, NULL, NULL);
					// send keepalive
					//DBG_MAIL("Rebooting...");
					//PM_Restart();
					
					powerOffStatus = PWROFF_FAILURE;
					job_next(job, &job_gsmDisconnect, NULL, NULL);
				}
			}
			default:
				break;
		}
	}
	
	return 0;
}

static uint8_t job_process_gps(job_t* job, uint8_t action, void* data)
{
	if(action == JOB_RUN)
	{
		DBG_MAIL("JOB RUN: GPS");

		GPS_Init();
		GPS_Open(NULL);
		GPIO_Set(GPIO_PIN9, GPIO_LEVEL_HIGH); // Turn GPS antenna on
		job_run(&job_http, NULL, NULL);
		led_rate(LED_GPS, LED_RATE_GPS_NOFIX);
/*
		GPS_Info_t* gpsInfo = Gps_GetInfo();
		PRINTD("GPS WAIT...");
		while(gpsInfo->rmc.latitude.value == 0)
			OS_Sleep(1000);
		PRINTD("GPS BLAH...");
		// set gps nmea output interval
		for(uint8_t i = 0;i<5;++i)
		{
			bool ret = GPS_SetOutputInterval(5000);
			PRINTD("set gps ret:%d",ret);
			if(ret)
				break;
			OS_Sleep(1000);
		}

		// if(!GPS_ClearInfoInFlash())
		//     Trace(1,"erase gps fail");

		if(!GPS_SetQzssOutput(true))
			PRINTD("enable qzss nmea output fail");

		// if(!GPS_SetSearchMode(true,false,true,false))
		//     Trace(1,"set search mode fail");

		if(!GPS_SetSBASEnable(true))
			PRINTD("enable sbas fail");
		
		GPS_SetQzssEnable(true);
		GPS_SetSearchMode(true, true, true, true);

		uint8_t buffer[300];
		if(!GPS_GetVersion(buffer,150))
			PRINTD("get gps firmware version fail");
		else
			PRINTD("gps firmware version:%s",buffer);

		// if(!GPS_SetFixMode(GPS_FIX_MODE_LOW_SPEED))
		// Trace(1,"set fix mode fail");

		if(!GPS_SetOutputInterval(1000))
			PRINTD("set nmea output interval fail");
*/
	}
	else if(action == JOB_UPDATE)
	{
		static uint8_t battUndervoltCount;
		static uint32_t timer_http;

		timer_http++;
		if(timer_http >= 1200) // JOB_UPDATE runs every 50ms, 1200 = 60 seconds
		{
			timer_http = 0;
			bme280_startConvertion();
			battVoltage = PM_Voltage(&battPercent);
			
			// 3400mV = 0%
			// 3800mV = 50%
			// 4200mV = 100%
			// Datasheet says min voltage is 3.5V
			// A9G dies at around 3250mV and fails to boot at around 3450 - 3500mV

			if(battVoltage < 3450) 
				battUndervoltCount++;
			else
				battUndervoltCount = 0;
			
			if(battUndervoltCount >= 3) // Battery too low, time to turn off
				job_next(job, NULL, NULL, NULL);
			else
				job_run(&job_http, NULL, NULL); // TODO what if job is still running? not a problem at the mo, timeout is 30 seconds
		}
	}
	else if(action == JOB_TIMEOUT)
	{
	}
	else if(action == JOB_EVENT)
	{
		API_Event_t* event = (API_Event_t*)data;
		switch(event->id)
		{
			case API_EVENT_ID_GPS_UART_RECEIVED:
			{
				PRINTD("received GPS data,length:%d", event->param1);
				
#if DEBUG
				int len = event->param1;
				while(len > 0)
				{
					int offset = (event->param1 - len);
					int printLen = (len <= 128) ? len : 128;
					char tmp = event->pParam1[offset + printLen];
					event->pParam1[offset + printLen] = 0x00;
					Trace(1, "%s", event->pParam1 + offset);
					event->pParam1[offset + printLen] = tmp;
					len -= printLen;
				}
#endif
				
				GPS_Update(event->pParam1, event->param1);
				//Trace(1, "GPSUPDT END");

				GPS_Info_t* gpsInfo = Gps_GetInfo();
				if(gpsInfo->gsa[0].fix_type > 1 || gpsInfo->gsa[1].fix_type > 1)
					led_rate(LED_GPS, LED_RATE_GPS_FIX);
				else
					led_rate(LED_GPS, LED_RATE_GPS_NOFIX);
				
				// A9G has another bug where if both UART1 and UART2 receive data at the same time their buffers get all messed up
				// It looks like they both share the same buffer???
				// A work around is to run the info request job straight after receiving data from the GPS module
				static uint32_t timer_req;
				timer_req++;
				if(timer_req >= 2) // Run every 2 seconds (GPS data comes in every 1 second)
				{
					timer_req = 0;
					job_next(NULL, &job_requestInfo, NULL, NULL);
				}
			}
				break;
			default:
				break;
		}
	}
	else if(action == JOB_STOP)
	{
		GPS_Close();
		GPIO_Set(GPIO_PIN9, GPIO_LEVEL_LOW);
		led_rate(LED_GPS, LED_RATE_GPS_OFF);
		powerOffStatus = PWROFF_SUCCESS;
		job_next(NULL, &job_gprsDisconnect, NULL, NULL);
		// TODO what if HTTP job is still running?
	}
	
	return 0;
}

static uint8_t job_process_http(job_t* job, uint8_t action, void* data)
{
	// WARNING: Do not run this job again if it is still running, things will probably get super messed up

	static char jsonRes[16];
	static uint8_t requestSuccessful;

	if(action == JOB_RUN)
	{
		DBG_MAIL("JOB RUN: HTTP");
		fd_http_closing = 0;
		memset(jsonRes, '\0', sizeof(jsonRes));
		requestSuccessful = 0;
		http_begin();
	}
	else if(action == JOB_UPDATE)
	{
	}
	else if(action == JOB_TIMEOUT)
	{
		// TODO reset job timeout on receive

		DBG_MAIL("JOB TO: HTTP");
		DBG_HTTP("TIMEOUT, closing");

		if(fd_http > 0) // fs_http could be 0 if we timeout while waiting for a DNS response
		{
			// NOTE: When http_close() is called the main task might interrupt this task with the API_EVENT_ID_SOCKET_CLOSED event
			fd_http_closing = 1;
			http_close(fd_http);
		}

		job_next(job, NULL, NULL, NULL);
		if(job->onComplete != NULL)
			job->onComplete(job->onCompleteParam, 0);
		
		// TODO retry
	}
	else if(action == JOB_EVENT)
	{
		API_Event_t* event = (API_Event_t*)data;
		switch(event->id)
		{
			case MAILBOX_EVT_HTTP_BEGIN:
			{
				// If timeout occurs while doing a DNS lookup and then DNS returns a response after the timeout things will go wonky
				if(!job->running)
					break;
				
				int res = http_host(HTTP_HOST, HTTP_PORT);
				if(res > 0)
					fd_http = res;
				else if(res < 0) // Failure
				{
					job_next(job, NULL, NULL, NULL);
					if(job->onComplete != NULL)
						job->onComplete(job->onCompleteParam, 0);
				}
				else // Waiting for a DNS response
				{

				}
			}
				break;
			case MAILBOX_EVT_HTTP_DNSFAIL: // TODO what if a different DNS lookup causes the failure?
				DBG_HTTP("DNS Error " HTTP_HOST);
				job_next(job, NULL, NULL, NULL);
				if(job->onComplete != NULL)
					job->onComplete(job->onCompleteParam, 0);
				break;
			case API_EVENT_ID_SOCKET_CONNECTED:
			{
				//timeoutReset(10000);
				
				if(event->param1 != fd_http)
					break;
				
				DBG_HTTP("skt connected %d", event->param1);

				char ip[16];
				Network_GetIp(ip, 16);
				
				Network_Signal_Quality_t gsmSignal;
				Network_GetSignalQuality(&gsmSignal);

				//uint8_t battPercent = 0;
				//uint16_t battVoltage = PM_Voltage(&battPercent);

				uint8_t imei[16];
				memset(imei, 0, sizeof(imei));
				INFO_GetIMEI(imei);
				
				uint8_t iccid[21];
				memset(iccid, 0, sizeof(iccid));
				SIM_GetICCID(iccid);
				
				GPS_Info_t* gpsInfo = Gps_GetInfo();

				if(gpsInfo->rmc.latitude.scale == 0)
					gpsInfo->rmc.latitude.scale = 1;
				if(gpsInfo->rmc.longitude.scale == 0)
					gpsInfo->rmc.longitude.scale = 1;
				float latitude = minmea_tocoord(&gpsInfo->rmc.latitude);
				float longitude = minmea_tocoord(&gpsInfo->rmc.longitude);
				
				if(gpsInfo->gga.altitude.scale == 0)
					gpsInfo->gga.altitude.scale = 1;
				if(gpsInfo->vtg.speed_kph.scale == 0)
					gpsInfo->vtg.speed_kph.scale = 1;
				if(gpsInfo->rmc.course.scale == 0)
					gpsInfo->rmc.course.scale = 1;
				float altitude = minmea_tofloat(&gpsInfo->gga.altitude);
				float speed = minmea_tofloat(&gpsInfo->vtg.speed_kph);
				float course = minmea_tofloat(&gpsInfo->rmc.course);
				
				// minmea lib doesn't differentiate between different talkers for the GSV messages, making it difficult to find out how many satellites are in view for each constellation (GPS, BDS etc)
				uint8_t totalSats[2] = {gpsInfo->gsv[0].total_sats, 0}; // GPS, BDS
				uint8_t totalSatsIdx = 0;
				uint8_t lastMsgNum = gpsInfo->gsv[0].msg_nr;
				for(uint8_t i=1;i<GPS_PARSE_MAX_GSV_NUMBER;i++)
				{
					if(gpsInfo->gsv[i].msg_nr <= lastMsgNum)
					{
						totalSatsIdx++;
						if(totalSatsIdx >= 2)
							break;
						totalSats[totalSatsIdx] = gpsInfo->gsv[i].total_sats;
					}

					lastMsgNum = gpsInfo->gsv[i].msg_nr;
				}
				
				// Get number of tracked satellites for each constellation
				// GSA messages can only have up to 12 satellites, but up to 16 can be in view
				uint8_t trackedSats[2] = {0, 0};
				for(uint8_t i=0;i<12;i++)
				{
					if(gpsInfo->gsa[0].sats[i] != 0)
						trackedSats[0]++;
					if(gpsInfo->gsa[1].sats[i] != 0)
						trackedSats[1]++;
				}

				cJSON* root = NULL;
				cJSON* fw = NULL;
				cJSON* batt = NULL;
				cJSON* jReasons = NULL;
				cJSON* jCounts = NULL;
				cJSON* environment = NULL;
				cJSON* balance = NULL;
				cJSON* network = NULL;
				cJSON* track = NULL;
				cJSON* gps = NULL;
				cJSON* bds = NULL;
				cJSON* tracktime = NULL;
				cJSON* trackdate = NULL;

				root = cJSON_CreateObject();
				cJSON_AddStringToObject(root, "key", HTTP_API_KEY);
				cJSON_AddNumberToObject(root, "millis", millis());
				cJSON_AddItemToObject(root, "firmware", fw = cJSON_CreateObject());
				cJSON_AddStringToObject(fw, "version", FW_VERSION);
				cJSON_AddStringToObject(fw, "built", fwBuild);
				cJSON_AddItemToObject(root, "network", network = cJSON_CreateObject());
				cJSON_AddNumberToObject(network, "signal", gsmSignal.signalLevel);
				cJSON_AddNumberToObject(network, "biterror", gsmSignal.bitError);
				cJSON_AddStringToObject(network, "ip", ip);
				cJSON_AddStringToObject(network, "number", "");
				cJSON_AddStringToObject(network, "imei", imei);
				cJSON_AddStringToObject(network, "iccid", iccid);
				cJSON_AddItemToObject(root, "battery", batt = cJSON_CreateObject());
				cJSON_AddNumberToObject(batt, "voltage", battVoltage);
				cJSON_AddNumberToObject(batt, "percent", battPercent);
				cJSON_AddNumberToObject(batt, "vlm", vlmDetected);
				cJSON_AddItemToObject(root, "balance", balance = cJSON_CreateObject());
				cJSON_AddNumberToObject(balance, "state", smsBalance.state);
				cJSON_AddStringToObject(balance, "message", smsBalance.content);
				cJSON_AddStringToObject(balance, "datetime", smsBalance.dateTime);
				cJSON_AddItemToObject(root, "reasons", jReasons = cJSON_CreateObject());
				cJSON_AddNumberToObject(jReasons, "newmail", reasons.newmail);
				cJSON_AddNumberToObject(jReasons, "endcharge", reasons.endcharging);
				cJSON_AddNumberToObject(jReasons, "trackmode", reasons.trackMode);
				cJSON_AddNumberToObject(jReasons, "switchstuck", reasons.switchstuck);
				cJSON_AddItemToObject(root, "counts", jCounts = cJSON_CreateObject());
				cJSON_AddNumberToObject(jCounts, "success", counts.success);
				cJSON_AddNumberToObject(jCounts, "failure", counts.failure);
				cJSON_AddNumberToObject(jCounts, "timeout", counts.timeout);
				cJSON_AddItemToObject(root, "environment", environment = cJSON_CreateObject());
				cJSON_AddNumberToObject(environment, "temperature", (bme280_readTemperature() / 100.0));
				cJSON_AddNumberToObject(environment, "humidity", (bme280_readHumidity() / 1024.0));
				cJSON_AddNumberToObject(environment, "pressure", ((bme280_readPressure() / 256.0) / 100.0));
				if(reasons.trackMode)
				{
					cJSON_AddItemToObject(root, "track", track = cJSON_CreateObject());
					cJSON_AddItemToObject(track, "gps", gps = cJSON_CreateObject());
					cJSON_AddNumberToObject(gps, "fix", gpsInfo->gsa[0].fix_type);
					cJSON_AddNumberToObject(gps, "sattotal", totalSats[0]);
					cJSON_AddNumberToObject(gps, "sattrack", trackedSats[0]);
					cJSON_AddItemToObject(track, "bds", bds = cJSON_CreateObject());
					cJSON_AddNumberToObject(bds, "fix", gpsInfo->gsa[1].fix_type); // NOTE: fix_type for both GPS and BDS are always the same value, if we have a GPS fix then BDS will also say it has a fix, even when it can't see any BDS satellites
					cJSON_AddNumberToObject(bds, "sattotal", totalSats[1]);
					cJSON_AddNumberToObject(bds, "sattrack", trackedSats[1]);
					cJSON_AddNumberToObject(track, "quality", gpsInfo->gga.fix_quality);
					cJSON_AddNumberToObject(track, "sattrack", gpsInfo->gga.satellites_tracked); // Total satellites tracked for all constellations, GSA messages can only have up to 12 satellites, but up to 16 can be in view at once
					cJSON_AddNumberToObject(track, "latitude", latitude);
					cJSON_AddNumberToObject(track, "longitude", longitude);
					cJSON_AddNumberToObject(track, "altitude", altitude);
					cJSON_AddNumberToObject(track, "speed", speed);
					cJSON_AddNumberToObject(track, "course", course);
					cJSON_AddItemToObject(track, "date", trackdate = cJSON_CreateObject());
					cJSON_AddNumberToObject(trackdate, "y", gpsInfo->rmc.date.year);
					cJSON_AddNumberToObject(trackdate, "m", gpsInfo->rmc.date.month);
					cJSON_AddNumberToObject(trackdate, "d", gpsInfo->rmc.date.day);
					cJSON_AddItemToObject(track, "time", tracktime = cJSON_CreateObject());
					cJSON_AddNumberToObject(tracktime, "h", gpsInfo->rmc.time.hours);
					cJSON_AddNumberToObject(tracktime, "m", gpsInfo->rmc.time.minutes);
					cJSON_AddNumberToObject(tracktime, "s", gpsInfo->rmc.time.seconds);
					cJSON_AddNumberToObject(tracktime, "ms", gpsInfo->rmc.time.microseconds / 1000);
				}

				// malloc a ~2K buffer for JSON and HTTP header (cJSON_PrintPreallocated()) so that the entire HTTP request can be sent in a single packet.
				// Socket_TcpipWrite() can take up to around 11.5KB in one go.
				// The maximum transmitted packet size is 1360 bytes, but might vary depending on mobile network.

				// JSON data is around 550-1200 bytes (depending if it is formatted and if tracking mode is enabled)
				// Header is around 190 bytes
				
				#define HTTP_HDR_MAXLEN		256
				#define HTTP_BODY_MAXLEN	1792

				char* httpReqBuff = malloc(HTTP_HDR_MAXLEN + HTTP_BODY_MAXLEN);

				int success = cJSON_PrintPreallocated(root, httpReqBuff + HTTP_HDR_MAXLEN, HTTP_BODY_MAXLEN, 0);
				cJSON_Delete(root);
				
				if(success)
				{
					uint32_t len = strlen(httpReqBuff + HTTP_HDR_MAXLEN);

					char contentLen[11];
					snprintf(contentLen, sizeof(contentLen), "%u", len);

					char* headers = httpReqBuff;
					headers += http_headerBegin(headers, "POST", HTTP_HOST, HTTP_PATH);
					headers += http_headerAdd(headers, "Connection", "Close");
					headers += http_headerAdd(headers, "Content-Type", "application/json");
					headers += http_headerAdd(headers, "Content-Length", contentLen);
					headers += http_headerEnd(headers);

					uint32_t headerLen = headers - httpReqBuff;
					DBG_HTTP("Header len: %u", headerLen);
					DBG_HTTP("Body len: %u", len);
					
					// Move body to the end of the headers
					memmove(headers, httpReqBuff + HTTP_HDR_MAXLEN, len);
					
					int writeLen = http_send(fd_http, httpReqBuff, len + headerLen);
					DBG_HTTP("Wrote %d", writeLen);
				}
				else
					PRINTD("ERROR: httpReqBuff is not large enough to fit JSON data!");
				
				free(httpReqBuff);
			}
				break;
			case API_EVENT_ID_SOCKET_RECEIVED:
			{
				if(event->param1 == fd_http)
				{
					DBG_HTTP("skt recv %d, len %d", event->param1, event->param2);
					
					// TODO force close after 128 bytes?

					char buff[128];
					int len;
					while((len = http_read(fd_http, buff, sizeof(buff) - 1)) > 0)
					{
						buff[len] = '\0';
						PRINTD("%s", buff);
						
						uint8_t jsonResLen = strlen(jsonRes);
						if(!requestSuccessful && jsonResLen < sizeof(jsonRes) - 1)
						{
							// Super simple response parsing
							// Look for a '{' character then check to see if the next 14 characters are '"result":"ok"}'
							char* jsonStart = (jsonResLen == 0) ? strchr(buff, '{') : buff;
							if(jsonStart != NULL)
							{
								strncpy(jsonRes + jsonResLen, jsonStart, sizeof(jsonRes) - jsonResLen);
								jsonRes[sizeof(jsonRes) - 1] = '\0';
								if(strcmp(jsonRes, "{\"result\":\"ok\"}") == 0)
									requestSuccessful = 1;
							}
						}
					}
				}
			}
				break;
			case API_EVENT_ID_SOCKET_SENT:
				if(event->param1 == fd_http)
					DBG_HTTP("skt sent %d", event->param1);
				break;
			case API_EVENT_ID_SOCKET_CLOSED:
				if(event->param1 == fd_http)
				{
					DBG_HTTP("skt closed %d", event->param1);
					if(!fd_http_closing)
					{
						http_close(fd_http);
						job_next(job, NULL, NULL, NULL);
						if(job->onComplete != NULL)
							job->onComplete(job->onCompleteParam, requestSuccessful);
						
						// TODO retry
					}
					fd_http = 0;
					fd_http_closing = 0;
				}
				break;
			case API_EVENT_ID_SOCKET_ERROR:
				if(event->param1 == fd_http)
				{
					DBG_HTTP("skt error %d, cause: %d", event->param1, event->param2);
					if(!fd_http_closing)
					{
						// NOTE: When http_close() is called the main task might interrupt this task with the API_EVENT_ID_SOCKET_CLOSED event
						// Or maybe not? API_EVENT_ID_SOCKET_ERROR is already called from main task
						fd_http_closing = 1;
						http_close(fd_http);
						
						job_next(job, NULL, NULL, NULL);
						if(job->onComplete != NULL)
							job->onComplete(job->onCompleteParam, 0);
					}
					
					// TODO retry
				}
				break;
			default:
				break;
		}
	}

	return 0;
}

static uint8_t job_process_gprsDisconnect(job_t* job, uint8_t action, void* data)
{
	if(action == JOB_RUN)
	{
		DBG_MAIL("JOB RUN: GPRS DISCONNECT...");
		gprs_disconnect();
	}
	else if(action == JOB_UPDATE)
	{
	}
	else if(action == JOB_TIMEOUT)
	{
		DBG_MAIL("JOB TO: GPRS DISCONNECT");
		job_next(job, &job_gsmDisconnect, NULL, NULL);
	}
	else if(action == JOB_EVENT)
	{
		API_Event_t* event = (API_Event_t*)data;
		switch(event->id)
		{
			case MAILBOX_EVENT_GPRS_DISCONNECTED:
			{
				DBG_MAIL("JOB EVT: GPRS DISCONNECTED");
				if(job->running)
					job_next(job, &job_gsmDisconnect, NULL, NULL);
			}
				break;
			default:
				break;
		}
	}
	
	return 0;
}

static uint8_t job_process_gsmDisconnect(job_t* job, uint8_t action, void* data)
{
	if(action == JOB_RUN)
	{
		DBG_MAIL("JOB RUN: GSM DISCONNECT...");
		gsm_disconnect();
	}
	else if(action == JOB_UPDATE)
	{
	}
	else if(action == JOB_TIMEOUT)
	{
		DBG_MAIL("JOB TO: GSM DISCONNECT");
		job_next(job, &job_requestPoweroff, NULL, NULL);
	}
	else if(action == JOB_EVENT)
	{
		API_Event_t* event = (API_Event_t*)data;
		switch(event->id)
		{
			case MAILBOX_EVENT_GSM_DISCONNECTED:
			{
				DBG_MAIL("JOB EVT: GSM DISCONNECTED");
				if(job->running)
					job_next(job, &job_requestPoweroff, NULL, NULL);
			}
				break;
			default:
				break;
		}
	}
	
	return 0;
}

static uint8_t job_process_requestPoweroff(job_t* job, uint8_t action, void* data)
{
	if(action == JOB_RUN)
	{
		mailcomm_poweroff(powerOffStatus);
	}
	else if(action == JOB_UPDATE)
	{
	}
	else if(action == JOB_TIMEOUT)
	{
		DBG_MAIL("JOB TO: PWR OFF");
		// Wait for MCU timeout failure...
		// TODO go to 32k low power mode?
		// retry?
		// shutdown?
	}
	else if(action == JOB_EVENT)
	{
	}
	
	return 0;
}

static void update(void)
{
	for(uint8_t i=0;i<sizeof(jobs) / sizeof(job_t*);i++)
	{
		if(jobs[i]->running)
		{
			if(jobs[i]->timeout != 0 && millis() - jobs[i]->startTime > jobs[i]->timeout)
			{
				DBG_MAIL("JOB TIMEOUT! %u %u", i, jobs[i]->timeout);
				jobs[i]->running = 0;
				if(jobs[i]->onProcess != NULL)
					jobs[i]->onProcess(jobs[i], JOB_TIMEOUT, NULL);
			}
			else if(jobs[i]->onProcess != NULL)
				jobs[i]->onProcess(jobs[i], JOB_UPDATE, NULL);
		}
	}

	led_update();
/*
	static uint8_t tickCount;
	tickCount++;
	if(tickCount < 10)
		return;
	tickCount = 0;
*/
	//if(ledFlash == GPIO_LEVEL_HIGH)
	//	SMS_ListMessageRequst(SMS_STATUS_ALL, SMS_STORAGE_SIM_CARD);

	//printStackHeap();
}

static void tmr_tick(void* param)
{
	mail_sendEvent(TRK_EVENT_TICK, 0, 0, NULL, NULL);
	OS_StartCallbackTimer(mailboxTaskHandle, 50, tmr_tick, NULL);
}

static void eventDispatch(API_Event_t* pEvent)
{
//	PRINTD("EVT %d %d %d", pEvent->id, pEvent->param1, pEvent->param2);

    switch(pEvent->id)
    {
		case MAILBOX_EVT_BEGIN:
			PM_SetSysMinFreq(PM_SYS_FREQ_13M);
			//PM_SleepMode(true);
			//OS_Sleep(10000);
			//PM_ShutDown();
			battVoltage = PM_Voltage(&battPercent);
			job_run(&job_clearSMSs, NULL, NULL);
			//job_run(&job_gps, NULL, NULL);
            break;
		case MAILBOX_EVENT_GSM_LOST:
			DBG_MAIL("GSM LOST");
			// Signal lost, will automatically reconnect
			break;
		case MAILBOX_EVENT_GPRS_DISCONNECTED:
		{
			DBG_MAIL("GPRS DISCONNECTED");
		}
			break;
		case MAILBOX_EVENT_GPRS_FAIL:
		{
			DBG_MAIL("GPRS CONNECT FAIL");
		}
			break;
		case MAILBOX_EVT_HTTPREADY:
			break;
        default:
            break;
    }

	// Always pass events to jobs even when they're not running
	for(uint8_t i=0;i<sizeof(jobs) / sizeof(job_t*);i++)
	{
		if(jobs[i]->onProcess != NULL)
			jobs[i]->onProcess(jobs[i], JOB_EVENT, pEvent);
	}
}

void mailbox_eventDispatch(API_Event_t* event)
{
//	if(event->id != TRK_EVENT_TICK)
//		PRINTD("%p %u %u %u %p %p", (void*)event, event->id, event->param1, event->param2, (void*)event->pParam1, (void*)event->pParam1);

	if(event->id != TRK_EVENT_TICK)
		eventDispatch(event);
	else
		update();
	
	if(event->id == API_EVENT_ID_POWER_INFO)
		PRINTD("ABC pwr %d %d", event->param1, event->param2);
	
	gsm_event(event);
	gprs_event(event);
	sms_event(event);
	mailcomm_event(event);

//	if(event->id != TRK_EVENT_TICK)
//		PRINTD("evt end %u", event->id);
}

void mailbox_task(void *pData)
{
	mailboxTaskHandle = *(HANDLE*)pData;
	API_Event_t* event = NULL;

	sms_listCallback(callback_smsList);
	sms_newMessageCallback(callback_smsNewMessage);

	tmr_tick(NULL);

	while(1)
	{
        if(OS_WaitEvent(mailboxTaskHandle, (void**)&event, OS_TIME_OUT_WAIT_FOREVER))
        {
			mailbox_eventDispatch(event);
			OS_Free(event->pParam1);
			OS_Free(event->pParam2);
			OS_Free(event);
        }
	}
}

void mail_sendEvent(uint32_t id, uint32_t param1, uint32_t param2, void* pParam1, void* pParam2)
{
	API_Event_t* event = OS_Malloc(sizeof(API_Event_t));
	if(event != NULL)
	{
		event->id = id;
		event->param1 = param1;
		event->param2 = param2;
		event->pParam1 = pParam1;
		event->pParam2 = pParam2;
		OS_SendEvent(mailboxTaskHandle, event, OS_WAIT_FOREVER, OS_EVENT_PRI_NORMAL);
	}
}

millis_t millis()
{
	return (millis_t)(clock() / CLOCKS_PER_MSEC);
}
/*
static void loadConfig(void)
{
	// TODO load config from file
}
*/
