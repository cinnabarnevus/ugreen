#ifndef __PM_H__
#define __PM_H__
#include <stdio.h>
#include "Mini51Series.h"
#include "core.h"

typedef enum
{
	POWER_LOSS = 1,
	POWER_OFF = 2,
	POWER_ON = 3,
	POWER_RESET = 4,
	POWER_FAIL = 5,
	SUSPEND = 6,
	OTHER,
} HOST_STATE;

/* Request type for Nvidia board */
#define NV_REQ_POWEROFF	(1 << 0)
#define NV_REQ_POWERON	(1 << 1)

extern uint32_t pending_power_request;
uint8_t get_poweron_times();

static inline void DISABLED_WOL_INTERRUPT(uint8_t type){
	if(type == 0){
		GPIO_DisableInt(P0, 1);
	}
}
static inline void ENABLED_WOL_INTERRUPT(uint8_t type,uint8_t flag){
	if(type == 0){
		GPIO_EnableInt(P0, 1,(flag == 0)? GPIO_INT_FALLING:GPIO_INT_RISING);
	}
}

extern uint8_t host_status;
extern struct timer_struct WatchdogAlarmItem;

void update_watchdog_time(void);
uint8_t is_host_poweroff(void);
void pm_task_init(void);
int PowerManageTaskWorker(struct task_struct *__this);
int PowerManageTransCallback(uint8_t reg,uint8_t value);
int WatchdogAlarmCallback(struct timer_struct *__this);
extern uint8_t sata_pwr_state;
void sata_power_control(uint8_t on);
#endif
