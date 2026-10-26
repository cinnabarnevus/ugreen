
/**
 * @brief       Port0/Port1 IRQ
 *
 * @param       None
 *
 * @return      None
 *
 * @details     The Port0/Port1 default IRQ, declared in startup_Mini51.s.
 */

#include "core.h"
#include "board.h"
#include "pm.h"
#include "wol.h"
#include "led.h"
#include "ir.h"

struct TransBufferItem PowerManageTransItem;

uint8_t host_status = POWER_LOSS;

static uint8_t power_button_pressed = 0;
static uint8_t rtc_pme_actived = 0;
static uint8_t wol_pme_actived = 0;
static uint8_t ir_pme_actived = 0;
static uint8_t power_on_is_enable_actived = 0;
uint8_t sata_pwr_state = 0;
//uint32_t volatile* WOL_PME[] = {&HOST_WOL_PME_IN,&HOST_OLD_WOL_PME_IN};
//uint32_t volatile* IR_RECEIVE[] = {&MCU_IR_RECEIVE_IN,&MCU_OLD_IR_RECEIVE_IN};

#define MCU_WOL_PME(type)				*(volatile uint32_t *)WOL_PME[type]
#define MCU_IR_RECEIVE(type)			*(volatile uint32_t *)IR_RECEIVE[type]

struct task_struct PowerManageTask = {
	.name		= "Power Manage",
	.function	= PowerManageTaskWorker,
};

struct TransBufferCallBack PowerManageCallback = {
    	.name		= "Power Manage trans callback",
        .callback         = PowerManageTransCallback,
};

struct timer_struct WatchdogAlarmItem = {
       .name           = "Watchdog callback",
        .enabled       = 0,
        .function      = WatchdogAlarmCallback,
};

void sata_power_control(uint8_t on)
{
	sata_pwr_state = on ? 1 : 0;
	if (sata_pwr_state)
			SATA_PWR_ON();
	else
			SATA_PWR_OFF();
}

int PowerManageTransCallback(uint8_t reg,uint8_t value) {
    //DEBUG("power manage->  reg:%02x value:%02x\n",reg,value);
    int ret = -1;
    uint8_t temp;
    uint32_t color_value;
    struct RegistBuffer *Regist = (struct RegistBuffer*)RegistBuffer;

    switch(reg){
      case MCU_POWERIN_DELAY:
          if(Regist->flashRegist.PwrInDelay != value){
              Regist->flashRegist.PwrInDelay = value;
              FlashRegistUpdate();
          } 
          ret = 0;
          break;
			case MCU_SATA_PWR:
						temp = value & 0x01;
						sata_power_control(temp);
						ret = 0;              /* 必须置 0：core.c:189 遇到第一个返回 0 的回调就停止遍历 */
						break;
      case MCU_POWER_LOSS:
          temp = value & 0x01;
						if(Regist->flashRegist.control.bits.power_loss != temp){
							Regist->flashRegist.control.bits.power_loss = temp;
              FlashRegistUpdate();
          } 
          ret = 0;
          break;      
      case MCU_RTC_WAKE:
          temp = value & 0x01;        
						if(Regist->flashRegist.control.bits.rtc_wake_en != temp){
							Regist->flashRegist.control.bits.rtc_wake_en = temp;
              FlashRegistUpdate();
            
          } 
          ret = 0;
          break; 
      case MCU_WOL_WAKE:
          temp = value & 0x01;         
          if(Regist->flashRegist.control.bits.wol_wake_en != temp){
              Regist->flashRegist.control.bits.wol_wake_en = temp;
              FlashRegistUpdate();
              if(Regist->flashRegist.control.bits.wol_wake_en == 1){
									 ENABLED_WOL_INTERRUPT(0,wol_int_flag());
              }else{
									DISABLED_WOL_INTERRUPT(0);
              }
          }
          ret = 0;          
          break;
      case MCU_WDT_TIMOUT:
          /* Watch Dog support 12 ~ 600 seconds */
          if(value >= 2 && value <= 100 && Regist->flashRegist.WdtTimout != value){
              Regist->flashRegist.WdtTimout = value;
							update_watchdog_time();
              FlashRegistUpdate();
              if(Regist->flashRegist.control.bits.watchdog_en == 1){
                  UpdateAlarmEventTime(&WatchdogAlarmItem, Regist->watchdog_timout *6* 1000, 1);
              }
          } 
          ret = 0;          
          break;        
      case MCU_WDT_CONTROL:
          temp = value & 0x01;         
          if(Regist->flashRegist.control.bits.watchdog_en != temp){
              Regist->flashRegist.control.bits.watchdog_en = temp;
              FlashRegistUpdate();
              if(Regist->flashRegist.control.bits.watchdog_en == 1){
                  UpdateAlarmEventTime(&WatchdogAlarmItem, Regist->watchdog_timout *6* 1000, 1);
              }else{
                  UpdateAlarmEventTime(&WatchdogAlarmItem, Regist->watchdog_timout *6* 1000, 0);
              }
          }  
          ret = 0;          
        break;   
			case MCU_PM_CTRL:
    	  if(value == HOST_SHUTDOWN){
					Regist->soc_reboot_flag = 0;
    	  }else if(value == HOST_FULL_RESET){
					host_status = POWER_RESET;
					Regist->pm_event = EVENT_REBOOT;
    	  }else if(value == HOST_DELAY_POWERON){
					Regist->soc_reboot_flag = 1;
					UpdateAlarmEventTime(&WatchdogAlarmItem, 100 * 1000, 1);
    	  }
				ret = 0;
        break;
      case MCU_LED_STATE:
          temp = value & 0xff;
          if(Regist->RgbState != temp){
              Regist->RgbState = temp;
              RGB_SetMode((led_mode_t)temp);
          }
          ret = 0;
          break;
      case MCU_LED_COLOR_H:
          temp = value & 0xff;
          if(Regist->flashRegist.color[0] != temp){
              Regist->flashRegist.color[0] = temp;
              FlashRegistUpdate();
              color_value = (uint32_t)Regist->flashRegist.color[0] << 16 | (uint32_t)Regist->flashRegist.color[1] << 8 | (uint32_t)Regist->flashRegist.color[2];
              set_color(color_value);
          }
          ret = 0;
          break;
      case MCU_LED_COLOR_M:
          temp = value & 0xff;
          if(Regist->flashRegist.color[1] != temp){
              Regist->flashRegist.color[1] = temp;
              FlashRegistUpdate();
              color_value = (uint32_t)Regist->flashRegist.color[0] << 16 | (uint32_t)Regist->flashRegist.color[1] << 8 | (uint32_t)Regist->flashRegist.color[2];
              set_color(color_value);
          }
          ret = 0;
          break;
      case MCU_LED_COLOR_L:
          temp = value & 0xff;
          if(Regist->flashRegist.color[2] != temp){
              Regist->flashRegist.color[2] = temp;
              FlashRegistUpdate();
              color_value = (uint32_t)Regist->flashRegist.color[0] << 16 | (uint32_t)Regist->flashRegist.color[1] << 8 | (uint32_t)Regist->flashRegist.color[2];
              set_color(color_value);
          }
          ret = 0;
          break;
      case MCU_LED_BLINK_TIMER:
          temp = value & 0xff;
          if(Regist->flashRegist.blinkTimer != temp){
              Regist->flashRegist.blinkTimer = temp;
              FlashRegistUpdate();
              set_blink_timer(temp);
          }
          ret = 0;
          break;
      default:
        break;
    }
    return ret;
}

int check_pwr_btn_action_time(){
      int count = 0;
      while(1){
        if(PWR_BUTTON_IN == 1){

            break;
        }else if(count >= 400){
           count = 0;
           break;
        }else{
            Delay10ms(1);
            count++;
        }
      }
      count = count*10;
      return count;
}

int pwr_btn_Anti_shake(){
      int count = 0;  
      while(1){
        if(PWR_BUTTON_IN == 1){

            return -1;
        }
        if(count >= 100){    //100us    
            break;
        }
        Delay1us(1);  
        count++;        
      }
      return 0;
}

int PowerOnhostBoard(void)
{
      struct RegistBuffer *Regist = (struct RegistBuffer*)RegistBuffer; 
			int isRecoveryKey = 0;
	
      if(HOST_RECOVERY_IN == 0){
         isRecoveryKey = 1;
      }
      rtc_pme_actived = 0;
      wol_pme_actived = 0;
			ir_pme_actived = 0;
      power_button_pressed = 0;
      RGB_SetMode(LED_MODE_BLINK);
      Regist->RgbState = LED_MODE_BLINK;
      set_color(0xff0000);
      set_blink_timer(5);
      pending_power_request |= NV_REQ_POWERON;
      Delay100ms(20);
      
      if((Regist->flashRegist.control.bits.watchdog_en == 1) && (HOST_RECOVERY_IN !=0) && (isRecoveryKey == 0)){
            UpdateAlarmEventTime(&WatchdogAlarmItem, 300 * 1000, 1);
      }
      if(isRecoveryKey) 
			{
				Regist->flashRegist.control.bits.watchdog_en = 0;
				FlashRegistUpdate();
			}
      return 0;
}

int PowerDownHostBoard(void)
{
      int count = 0;
      pending_power_request |= NV_REQ_POWEROFF;
      while(1){
        if(HOST_PWR_IN == 0){

            break;
        }else if(count >= 1000){
           break;
        }else{
            Delay10ms(1);
            count++;
        }
      }
      return 0;
}

int ResetHostBoard(void)
{
		DEBUG("start reset host .....\n");
    PowerDownHostBoard();
    Delay10ms(100);
    PowerOnhostBoard();
		DEBUG("end reset host .....\n");    
    return 0;
}

int PowerManageTaskWorker(struct task_struct *__this)
{   
	struct RegistBuffer *Regist = (struct RegistBuffer*)RegistBuffer;
	
	switch(host_status){
			case POWER_LOSS:
				if(HOST_PWR_IN == 1 && (get_host_power_status() != POWER_OFF)){
					host_status = POWER_ON;
				}else{
					if(sata_pwr_state != 0)
            sata_power_control(0);
					if(Regist->flashRegist.control.bits.power_loss == 1){
						DEBUG("Power Loss Poweron\n");
						PowerOnhostBoard();
					}else{
						if(power_on_is_enable_actived == 1){
							power_on_is_enable_actived = 0;
							PowerOnhostBoard();
						}	
					}
					host_status = POWER_OFF;
				}
				break;
			case POWER_OFF:
				power_on_is_enable_actived = 0;
				if(get_host_power_status() == POWER_ON){
					host_status = POWER_ON;
					break;
				}else{
					if(sata_pwr_state != 0)
             sata_power_control(0);
					if(power_button_pressed == 1){
						power_button_pressed = 0;
						DEBUG("pwrbtn low!!!\n");
						if(Regist->flashRegist.PwrInDelay == 0){
							if(pwr_btn_Anti_shake() == 0){
								DEBUG("process pwr button output\n");
								PowerOnhostBoard();                          
							}
						}else{
							if(check_pwr_btn_action_time() >= Regist->flashRegist.PwrInDelay*1000){
								DEBUG("process pwr button output\n");
								PowerOnhostBoard();
							}                        
						}
					}else if(rtc_pme_actived == 1) {
						DEBUG("alarm power on!!!\n");
						rtc_pme_actived = 0;
						PowerOnhostBoard();
					}else if(wol_pme_actived == 1) {
						DEBUG("wol power on!!!\n");                    
						wol_pme_actived = 0;
						Regist->pm_event = EVENT_WOL_WAKEUP;
						PowerOnhostBoard();
					}else if(ir_pme_actived == 1) {
						DEBUG("ir power on!!!\n");                    
						ir_pme_actived = 0;
						PowerOnhostBoard();
					}else if(Regist->soc_reboot_flag == 1) {
						DEBUG("system reboot!!!\n");
						Regist->soc_reboot_flag = 0;
						Regist->pm_event = EVENT_REBOOT;
						PowerOnhostBoard();
					}
				}
				break;
			case POWER_ON:
				rtc_pme_actived = 0;
				wol_pme_actived = 0;
				ir_pme_actived = 0;
				power_button_pressed = 0;
				power_on_is_enable_actived = 0;
				DISABLED_WOL_INTERRUPT(0);
				if(is_host_poweroff()){
					DEBUG("soc power off\n");
					if(Regist->flashRegist.control.bits.wol_wake_en == 1){
						DISABLED_WOL_INTERRUPT(0);
						config_wol();
						ENABLED_WOL_INTERRUPT(0,wol_int_flag());
					}
					UpdateAlarmEventTime(&WatchdogAlarmItem, 0, 0);
					host_status = POWER_OFF;
				}
				break;
			case POWER_RESET:
				DEBUG("process pwr reset host\n");
				if(HOST_PWR_IN == 1){
					host_status = POWER_ON;
				}
				ResetHostBoard();
				break;              
			default:
				break;
      }
    return 0;
}   

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
	if(Regist->flashRegist.WdtTimout < 2 || Regist->flashRegist.WdtTimout > 100)
		Regist->flashRegist.WdtTimout = 100;
	Regist->watchdog_timout = Regist->flashRegist.WdtTimout;
}

uint8_t is_host_poweroff(void)
{
	struct RegistBuffer *Regist = (struct RegistBuffer*)RegistBuffer;
	
	if(Regist->flashRegist.control.bits.wol_wake_en == 1){
		if(get_host_power_status() == POWER_OFF){
			return 1;
		}
	}else{
		if(HOST_PWR_IN == 0){
			return 1;
		}
	}
	return 0;
}

void GPIO01_IRQHandler(void)
{	
	struct RegistBuffer *Regist = (struct RegistBuffer*)RegistBuffer; 
	
	if (P0->ISRC & BIT1){
		P0->ISRC = BIT1;
		if(Regist->flashRegist.control.bits.wol_wake_en == 1){
			if(host_status == POWER_OFF){
				wol_pme_actived = 1;
				DEBUG("wol_pme_actived!!!\n");
			}else{
				DEBUG("wol_pme_actived but host_status :%d\n",host_status);
			}
		}
	}
}

void GPIO234_IRQHandler(void){
    struct RegistBuffer *Regist = (struct RegistBuffer*)RegistBuffer;  
    if (P3->ISRC & BIT0)
    {
        P3->ISRC = BIT0;
			if(host_status == POWER_OFF){
				power_button_pressed = 1;
				DEBUG("pwr button active!!!\n");
			}else{
				DEBUG("pwr button active but host_status :%d\n",host_status);
			}
    }
    if (P3->ISRC & BIT6)
    {
        P3->ISRC = BIT6;
        //DEBUG("watchdog feed...!!!\n");
        if(Regist->flashRegist.control.bits.watchdog_en == 1){
						 if(Regist->soc_reboot_flag == 1){
							 UpdateAlarmEventTime(&WatchdogAlarmItem, 100 * 1000, 1);
						 }else{
							 UpdateAlarmEventTime(&WatchdogAlarmItem, Regist->watchdog_timout *6* 1000, 1);
						 }
        }else{
             UpdateAlarmEventTime(&WatchdogAlarmItem, 0, 0);
        }

    }    
}

void GPIO5_IRQHandler(void)
{
	struct RegistBuffer *Regist = (struct RegistBuffer*)RegistBuffer;  
	
	if (P5->ISRC & BIT3){
		P5->ISRC = BIT3;
		if(Regist->flashRegist.control.bits.power_loss == 0){
			if(host_status == POWER_OFF){
				power_on_is_enable_actived = 1;
				DEBUG("power_on_is_enable_actived!!!\n");
			}else{
				DEBUG("power_on_is_enable_actived but host_status :%d\n",host_status);
			}
		}
	}
}

void EINT1_IRQHandler(void)
{	
    struct RegistBuffer *Regist = (struct RegistBuffer*)RegistBuffer;   
    if (P5->ISRC & BIT2)
    {
        P5->ISRC = BIT2;
				if(Regist->flashRegist.control.bits.rtc_wake_en == 1){
					if(host_status == POWER_OFF){
						rtc_pme_actived = 1;
            DEBUG("rtc_pme_actived!!!\n");
					}else{
						DEBUG("rtc_pme_actived but host_status :%d\n",host_status);
					}
				}
    }
}

void pm_gpio_init(void)
{
    struct RegistBuffer *Regist = (struct RegistBuffer*)RegistBuffer;  

    //GPIO_EnableInt(P0, 1, GPIO_INT_FALLING);
    GPIO_EnableInt(P3, 0, GPIO_INT_FALLING);
		GPIO_EnableInt(P5, 3, GPIO_INT_FALLING);
    GPIO_EnableEINT1(P5, 2, GPIO_INT_FALLING);
    GPIO_EnableInt(P3, 6, GPIO_INT_FALLING); 
	
    NVIC_SetPriority(GPIO01_IRQn,1);
    NVIC_SetPriority(GPIO234_IRQn,1);
    NVIC_SetPriority(GPIO5_IRQn,1);    
	
    NVIC_EnableIRQ(GPIO01_IRQn);
    NVIC_EnableIRQ(GPIO234_IRQn); 
    NVIC_EnableIRQ(EINT1_IRQn); 
    
}

void pm_task_init(void)
{ 
     pm_gpio_init();
		 update_watchdog_time();
     RegistTaskWorker(&PowerManageTask);
     PowerManageTransItem.trans = &PowerManageCallback;
     RegistTransBufferCallback(&PowerManageTransItem);
		 RegistAlarmEventCallback(&WatchdogAlarmItem);
}
