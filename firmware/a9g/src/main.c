/*
 * Project: Remote Mail Notifier (and GPS Tracker)
 * Author: Zak Kemble, contact@zakkemble.net
 * Copyright: (C) 2020 by Zak Kemble
 * License: 
 * Web: https://blog.zakkemble.net/remote-mail-notifier-and-gps-tracker/
 */

#include "common.h"

#define MAIN_TASK_STACK_SIZE    (1024 * 2)
#define MAIN_TASK_PRIORITY      0 
#define MAIN_TASK_NAME         "Main task"

#define MAILBOX_TASK_STACK_SIZE    (1024 * 2)
#define MAILBOX_TASK_PRIORITY      1
#define MAILBOX_TASK_NAME         "Mail box task"

static HANDLE mainTaskHandle = NULL;
static HANDLE mailboxTaskHandle = NULL;
extern char* fwBuild;
static Power_On_Cause_t pwrOnCause;

void EventDispatch(API_Event_t* pEvent)
{
    switch(pEvent->id)
    {
		case API_EVENT_ID_POWER_ON:
			pwrOnCause = pEvent->param1;
			break;
        case API_EVENT_ID_SYSTEM_READY:
			PRINTD("FW: " FW_VERSION " (Built: %s)", fwBuild);
			PRINTD("system initialize complete (%u)", pwrOnCause);
			sms_info();
			mail_sendEvent(MAILBOX_EVT_BEGIN, 0, 0, NULL, NULL);
            break;
		case API_EVENT_ID_NO_SIMCARD:
			PRINTD("!!NO SIM CARD %d!!!!", pEvent->param1);
			// TODO turn off
			break;
		case API_EVENT_ID_SIMCARD_DROP:
			PRINTD("!!SIM CARD DROP %d!!!!", pEvent->param1);
			// TODO what now?
			break;
		case API_EVENT_ID_KEY_DOWN:
			break;
		case API_EVENT_ID_KEY_UP:
			break;
		case API_EVENT_ID_MALLOC_FAILED:
			PRINTD("MALLOC FAILED");
			break;
		case API_EVENT_ID_POWER_INFO:
			break;
		case API_EVENT_ID_USSD_IND:
		case API_EVENT_ID_USSD_SEND_SUCCESS:
		case API_EVENT_ID_USSD_SEND_FAIL:
			break;
        default:
			mailbox_eventDispatch(pEvent);
//			PRINTD("EVT2 %d %d %d", pEvent->id, pEvent->param1, pEvent->param2);
            break;
    }
}

static void unusedGPIO(GPIO_PIN pin)
{
	GPIO_EnablePower(pin, true);
	GPIO_config_t gpioUnused = {
		.mode			= GPIO_MODE_OUTPUT,
		.pin			= pin,
		.defaultLevel	= GPIO_LEVEL_LOW
	};
	if(!GPIO_Init(gpioUnused))
		PRINTD("GPIO CONFIG FAIL %u", pin);
}

static GPIO_PIN unused[] = {
	// VPAD1
	GPIO_PIN2,
	GPIO_PIN3,
	GPIO_PIN6,
	GPIO_PIN7,
	// MMC
	GPIO_PIN8,
	GPIO_PIN10,
	GPIO_PIN11,
	GPIO_PIN12,
	GPIO_PIN13,
	// LCD
	GPIO_PIN14,
	GPIO_PIN15,
	GPIO_PIN16,
	GPIO_PIN17,
	GPIO_PIN18,
	// CAM
	GPIO_PIN19,
	GPIO_PIN20,
	GPIO_PIN21,
	GPIO_PIN22,
	GPIO_PIN23,
	GPIO_PIN24,
	// VPAD2
	GPIO_PIN26,
	GPIO_PIN27,
	GPIO_PIN28,
	GPIO_PIN29,
	GPIO_PIN31, // Keep OUTPUT LOW to reduce current
	GPIO_PIN32, // Keep OUTPUT LOW to reduce current
	GPIO_PIN33, // Keep OUTPUT LOW to reduce current
	GPIO_PIN34, // Keep OUTPUT LOW to reduce current
};

static void init(void)
{
	PM_SetSysMinFreq(PM_SYS_FREQ_13M);

	for(uint8_t i=0;i<sizeof(unused)/sizeof(GPIO_PIN);i++)
		unusedGPIO(unused[i]);

	led_init();

	// GPS antenna power
	GPIO_EnablePower(GPIO_PIN9, true);
	GPIO_config_t gpioGPSAntennaPower = {
		.mode			= GPIO_MODE_OUTPUT,
		.pin			= GPIO_PIN9,
		.defaultLevel	= GPIO_LEVEL_LOW
	};
	GPIO_Init(gpioGPSAntennaPower);

	// BME280
	PM_PowerEnable(POWER_TYPE_CAM, true);
    I2C_Config_t i2cConfig;
    i2cConfig.freq = I2C_FREQ_100K;
    I2C_Init(I2C2, i2cConfig);

	TIME_SetIsAutoUpdateRtcTime(true);
	gsm_init();
	gprs_init();
	sms_init();
	bme280_init();
	mailcomm_init();
}

void MainTask(void *pData)
{
	init();

	mailboxTaskHandle = OS_CreateTask(
		mailbox_task,
		&mailboxTaskHandle,
		NULL,
		MAILBOX_TASK_STACK_SIZE,
		MAILBOX_TASK_PRIORITY,
		0,
		0,
		MAILBOX_TASK_NAME
	);

	API_Event_t* event = NULL;
	while(1)
	{
		if(OS_WaitEvent(mainTaskHandle, (void**)&event, OS_TIME_OUT_WAIT_FOREVER))
		{
			EventDispatch(event);
			OS_Free(event->pParam1);
			OS_Free(event->pParam2);
			OS_Free(event);
		}
	}
}

void mailbox_Main()
{
    mainTaskHandle = OS_CreateTask(
		MainTask,
        NULL,
		NULL,
		MAIN_TASK_STACK_SIZE,
		MAIN_TASK_PRIORITY,
		0,
		0,
		MAIN_TASK_NAME
	);
    OS_SetUserMainHandle(&mainTaskHandle);
}
