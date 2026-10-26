#include "watchdog.h"

uint8_t watchdog_timout = 12;

struct timer_struct WatchdogAlarmItem = {
    	.name		= "Watchdog callback",
        .enabled	= 0,
        .function	= WatchdogAlarmCallback,
};

int WatchdogAlarmCallback(struct timer_struct *__this) {
    struct RegistBuffer *Regist = (struct RegistBuffer*)RegistBuffer;   
    DEBUG("Watchdog Timeout.....\n");
    if(Regist->flashRegist.control.bits.watchdog_en == 1){
      host_status = POWER_RESET;
      Regist->pm_event = EVENT_WDT_RESET;
    }
    return 0;
}

void update_watchdog_time(void)
{
	struct RegistBuffer *Regist = (struct RegistBuffer*)RegistBuffer;
	
	if(Regist->flashRegist.control.bits.watchdog_en == 1){
		if(Regist->flashRegist.control.bits.WdtTimout == 0){
			watchdog_timout = 12;
		}else if(Regist->flashRegist.control.bits.WdtTimout == 1){
			watchdog_timout = 15;
		}else if(Regist->flashRegist.control.bits.WdtTimout == 2){
			watchdog_timout = 19;
		}else{
			watchdog_timout = 21;
		}
		UpdateAlarmEventTime(&WatchdogAlarmItem,watchdog_timout * 1000,true);
	}
}

void watchdog_task_init(void)
{
	RegistAlarmEventCallback(&WatchdogAlarmItem);
}
