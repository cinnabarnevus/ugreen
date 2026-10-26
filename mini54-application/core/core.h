#ifndef __SM1_CORE_H__
#define __SM1_CORE_H__
#include <stdio.h>
#include "list.h"

#define	MCU_FW_VID		0x3F
#define	MCU_FW_PID		0x10
#define	MCU_FW_VERSION		6

#define	bool	uint8_t
#define	LONGLAKE_DEBUG
#ifdef	LONGLAKE_DEBUG
#define	DEBUG(fmt, arg...)	printf("<--mini54-->  "fmt, ##arg)
#else
#define DEBUG(fmt, arg...)
#endif

#define	FMC_APROM_END			0x4000
#define	DATA_FLASH_PARTITION_SIZE	512
#define	DATA_FLASH_BASE			FMC_APROM_END - DATA_FLASH_PARTITION_SIZE

#define  MAX_REG		100
#define  MAX_FLASH_REG		22

#define  AP_SIGNATURE				0x55AA5500
#define  AP_FIREWARE_GOOD_SIGNATURE		0x55AA55AA
#define  AP_FIRMWARE_GOOD_MASK			0x000000ff
#define  AP_FIREWARE_GOOD_VALUE			0x000000AA

typedef enum
{
	MCU_VID = 1,
	MCU_PID = 2,
	MCU_POWERIN_DELAY = 0x09, 
	MCU_POWER_LOSS = 0x0D, 
	MCU_WDT_START_DELAY =0x0E,
	MCU_WDT_CONTROL = 0x0F,
	MCU_VERSION = 0x11,
	MCU_REBOOT_FLAG = 0x12,  
	MCU_SATA_PWR = 0x1E,	
	MCU_RTC_WAKE = 0x20,
	MCU_PM_CTRL  = 0x23,
	MCU_ETH_MAC1 = 0x27, 
	MCU_ETH_MAC2 = 0x28, 
	MCU_ETH_MAC3 = 0x29, 
	MCU_ETH_MAC4 = 0x2A, 
	MCU_ETH_MAC5 = 0x2B, 
	MCU_ETH_MAC6 = 0x2C,
	MCU_WOL_WAKE = 0x2E,
	MCU_WDT_TIMOUT = 0x30,	
	MCU_WOL_TYPE = 0x41, 		
	DEBUG_WDT_TIME = 0x51,//WDT remaining time.
	DEBUG_PM_EVENT = 0x52,
	DEBUG_POWERON_TIMES = 0x53,
	/* ARGB LED, see function/src/led.c */
	MCU_LED_STATE = 0x55,
	MCU_LED_COLOR_H = 0x56,
	MCU_LED_COLOR_M = 0x57,
	MCU_LED_COLOR_L = 0x58,
	MCU_LED_BLINK_TIMER = 0x59,
	MCU_UPDATE_CTRL = 0x60,
	/* ADC key ladder, MCU_ADC_CH1_IN, see function/src/adc_key.c */
	MCU_ADC_KEY = 0x61,
	MCU_ADC_KEY_RAW_H = 0x62,
	MCU_ADC_KEY_RAW_L = 0x63,
} MCU_REGIST;

typedef enum {
	HOST_SHUTDOWN = 1,
	HOST_FULL_RESET = 2,
	HOST_DELAY_POWERON = 3
} HOST_PM_MODE;

typedef union {
	uint8_t byte;
    struct {
		uint8_t power_loss : 1;
		uint8_t rtc_wake_en : 1;
		uint8_t wol_wake_en : 1;
		uint8_t watchdog_en : 1;
		uint8_t WdtTimout : 2;
		uint8_t WdtStartDelay : 2;
    } bits;
} PM_CONTROL;

struct RomInfomation {
	char  name[4];
	uint8_t   vid;
	uint8_t   pid;
	uint8_t   version;
};

struct FlashRegist {
	uint32_t  Signature;
	uint8_t   WdtTimout;
	uint8_t   PwrInDelay;
	uint8_t   mac[6];
	PM_CONTROL   control;
	uint8_t   model;
	/* keep the new fields at the end so that the offsets of the fields
	   above stay unchanged for boards updating from an older firmware */
	uint8_t   color[3];         /* LED color, high/middle/low byte */
	uint8_t   blinkTimer;       /* LED blink period, unit 100ms */
};

struct RegistBuffer {
	uint8_t   vid;
	uint8_t   pid;
	uint8_t   version;
	uint8_t   RgbState;
	uint8_t   wdt_test;
	uint8_t   boot_type;        
	uint8_t   pm_event;
	uint8_t   soc_reboot_flag;
	uint8_t   watchdog_timout;
	struct FlashRegist  flashRegist;    
};

enum {
	EVENT_HW_RESET = 0,
	EVENT_WDT_RESET = 1,
	EVENT_SHUTDOWN = 2,
	EVENT_REBOOT = 3,
	EVENT_WOL_WAKEUP = 4,
};

struct task_struct {
	char *name;
	struct list_head list;
	int (*function)(struct task_struct *__this);
};


struct timer_struct {
	char *name;
	struct list_head list;
	uint32_t reload_time_ms;	// milliseconds
	uint32_t current_time_ms;	// milliseconds, count down.
	int (*function)(struct timer_struct *__this);
	uint8_t enabled;
};

struct  TransBufferCallBack {
	char *name;
	int (*callback)(uint8_t ,uint8_t);	
};

struct TransBufferItem {
	struct list_head list;
	struct TransBufferCallBack *trans;
};

typedef struct __I2cTransformBuffer {
	uint8_t Cursor;
	uint8_t Buffer[100];
}I2cTransformBuffer;

typedef struct __TransformTxRxBuffer {
	I2cTransformBuffer RxBuffer;
	I2cTransformBuffer TxBuffer;
}TransformTxRxBuffer;


extern uint8_t  RegistBuffer[MAX_REG];

void RegistAlarmEventCallback(struct timer_struct *timer);
void UpdateAlarmEventTime(struct timer_struct *item, int time_ms, bool enable);
void RegistTaskWorker(struct task_struct *task);
void RegistTransBufferCallback(struct TransBufferItem *item);
void ScheduleTaskWorker(struct task_struct *task);
void CancelTaskWorker(struct task_struct *task);
void ScheduleTaskWorkerLooper(void);
void TrigerAlarmEventCallback(int passed_ms);
void TransformBufferCallback(uint8_t reg,uint8_t value);
uint8_t GetRegistBufferValue(uint8_t reg);
void Delay100us(uint32_t u32CNT);
void Delay1ms(uint32_t u32CNT);
void Delay10ms(uint32_t u32CNT);
void Delay500us(uint32_t u32CNT);
void Delay100ms(uint32_t u32CNT);
void Delay40ms(uint32_t u32CNT);
void Delay10us(uint32_t u32CNT);
void Delay1us(uint32_t u32CNT);
void FlashRegistUpdate(void);
void core_init(void);

#endif
