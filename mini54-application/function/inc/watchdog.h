#ifndef __WATCHDOG_H__
#define __WATCHDOG_H__

#include <stdio.h>
#include <stdint.h>
#include "Mini51Series.h"
#include "core.h"
#include "pm.h"
#include <stdbool.h>

extern uint8_t watchdog_timout;

int WatchdogAlarmCallback(struct timer_struct *__this);
void update_watchdog_time(void);
void watchdog_task_init(void);

#endif