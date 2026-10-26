#include <stdio.h>
#include "Mini51Series.h"
#include "core.h"
#include "pm.h"
#include "wol.h"
#include "ir.h"
#include "update.h"
#include "led.h"
#include "adc_key.h"

#ifdef __IDE_KEIL__
const __attribute__((used,section(".DeviceInfo"),aligned(0x00000200))) struct RomInfomation romInfo={
                          .name = "M54A",
                          .vid = MCU_FW_VID,
                          .pid = MCU_FW_PID,
                          .version = MCU_FW_VERSION
};
#else
__root const struct RomInfomation romInfo@".DeviceInfo"={
                          .name = "M54A",
                          .vid = MCU_FW_VID,
                          .pid = MCU_FW_PID,
                          .version = MCU_FW_VERSION     
};
#endif
uint8_t  RegistBuffer[MAX_REG] __attribute__((aligned(4)));

static LIST_HEAD(TaskWorkerList);
static LIST_HEAD(AlarmEventCallbackList);
static LIST_HEAD(TransBufferCallbackList);

TransformTxRxBuffer TransformBuffer;



void SlaveI2cTransformRegistToHost(void)
{
	uint8_t reg;
	reg = TransformBuffer.RxBuffer.Buffer[0];
	TransformBuffer.TxBuffer.Cursor = 0;
	TransformBuffer.RxBuffer.Cursor = 0;

	TransformBuffer.TxBuffer.Buffer[0] = GetRegistBufferValue(reg);
}


void SlaveI2cReciveRegistFormHost(void)
{
	uint8_t reg,value;

	if (TransformBuffer.RxBuffer.Cursor >=2){
		reg = TransformBuffer.RxBuffer.Buffer[0];
		value = TransformBuffer.RxBuffer.Buffer[1];
		TransformBufferCallback(reg , value);
	}
}

void I2C_IRQHandler(void)
{
	
	uint32_t u32Status;
	u32Status = I2C_GET_STATUS(I2C);
	if (I2C_GET_TIMEOUT_FLAG(I2C))
	{
		/* Clear I2C Timeout Flag */
		I2C_ClearTimeoutFlag(I2C);
	}
	else
	{
		//slave read
		if ((u32Status == 0x60)  || (u32Status == 0x68) )  //Slave Receive Address ACK   or Slave Receive Arbitration Lost                /* START has been transmitted and prepare SLA+W */
		{
			I2C_SET_CONTROL_REG(I2C, I2C_SI | I2C_AA);
		}else if (u32Status == 0x80) //Slave Receive Data ACK
		{
			TransformBuffer.RxBuffer.Buffer[TransformBuffer.RxBuffer.Cursor++] = I2C_GET_DATA(I2C);
			I2C_SET_CONTROL_REG(I2C, I2C_SI  | I2C_AA);
		}else if (u32Status == 0x88)  //Slave Receive Data NACK
		{
			I2C_SET_CONTROL_REG(I2C, I2C_SI | I2C_AA);
		}else if (u32Status == 0xA0)  //A STOP or repeated START has been
		{
			SlaveI2cReciveRegistFormHost();  //write mI2cIrqHandleData.I2cRxBuffer.dataBuffer[] to RegistBuffer[]      
			TransformBuffer.RxBuffer.Cursor=0;
			I2C_SET_CONTROL_REG(I2C, I2C_SI | I2C_AA);
		}else if ( (u32Status == 0xA8) || (u32Status == 0xB0) )  //Slave Transmit Address ACK or Slave Transmit Arbitration Lost
		{
			SlaveI2cTransformRegistToHost();
			I2C_SET_DATA(I2C, TransformBuffer.TxBuffer.Buffer[TransformBuffer.TxBuffer.Cursor++]);
			I2C_SET_CONTROL_REG(I2C, I2C_SI | I2C_AA);
			//I2C_SET_CONTROL_REG(I2C, I2C_SI);
		}else if (u32Status == 0xB8)   //Slave Transmit Data ACK
		{
			I2C_SET_DATA(I2C, TransformBuffer.TxBuffer.Buffer[TransformBuffer.TxBuffer.Cursor++]);
			I2C_SET_CONTROL_REG(I2C, I2C_SI | I2C_AA);
		}else if (u32Status == 0xC0)   //Slave Transmit Data NACK
		{
			I2C_SET_CONTROL_REG(I2C, I2C_SI | I2C_AA);
		}else if (u32Status == 0xC8)  //Slave Transmit Last Data ACK 
		{
			I2C_SET_CONTROL_REG(I2C, I2C_SI | I2C_AA);
		}

	}

}

extern void vtimer_hander( void );
void SysTick_Handler(void){
	static uint32_t tick_count = 0;
	if (0 == (tick_count % 10)) {
		TrigerAlarmEventCallback(10);	//10ms
	}
	vtimer_hander();
	tick_count++;
}

void RegistTransBufferCallback(struct TransBufferItem *item)
{
	if(item)
	list_add((struct list_head *)item ,&TransBufferCallbackList);
  
}

void RegistAlarmEventCallback(struct timer_struct *item)
{
	if(item)
		list_add((struct list_head *)item ,&AlarmEventCallbackList);
  
}

void UpdateAlarmEventTime(struct timer_struct *item, int time_ms, bool enable)
{
	if(item){
		item->reload_time_ms = time_ms;
		item->current_time_ms = time_ms;
		item->enabled = enable;
	}
}

void RegistTaskWorker(struct task_struct *item)
{
	if(item)
		list_add((struct list_head *)item ,&TaskWorkerList);
}


void ScheduleTaskWorkerLooper(void)
{
	struct list_head *pos;
	struct task_struct *item;
	list_for_each(pos,&TaskWorkerList) {
		item = (struct task_struct*)pos;
		if(item){
			if(item->function){
				item->function(item);
			}
		}
	}
}


void TrigerAlarmEventCallback(int ms)
{
	struct list_head *pos;
	struct timer_struct *EventItem;
	list_for_each(pos,&AlarmEventCallbackList){
		EventItem = (struct timer_struct*)pos;
		if(EventItem && EventItem->enabled){
			/* time count down */
			if(EventItem->current_time_ms <= ms){
				EventItem->current_time_ms = EventItem->reload_time_ms;
				/* Handle timer event */
				EventItem->function(EventItem);
			}else {
				EventItem->current_time_ms -= ms;
			}
		}
	}
}

void TransformBufferCallback(uint8_t reg,uint8_t value)
{
	struct list_head *pos;
	struct TransBufferItem *bufferItem;   
	DEBUG("Transform set regist: %02x -> %02x\n",reg,value);
	list_for_each(pos,&TransBufferCallbackList){
		bufferItem = (struct TransBufferItem*)pos;
		if(bufferItem){
			if(bufferItem->trans->callback){
				if(bufferItem->trans->callback(reg,value) == 0){
				return;
				}
			}
		}
	}
}


uint8_t GetRegistBufferValue(uint8_t reg)
{
	uint8_t value = 0;
	struct RegistBuffer *Regist = (struct RegistBuffer*)RegistBuffer;
	if(reg > MAX_REG){
		return 0;
	}
	switch(reg){
		case MCU_VID:
			value = Regist->vid;
			break;
		case MCU_PID: 
			value = Regist->pid;
			break;
		case MCU_VERSION:
			value = Regist->version;
			break;
		case MCU_POWERIN_DELAY:
			value = Regist->flashRegist.PwrInDelay;
			break;
		case MCU_POWER_LOSS:
			value = Regist->flashRegist.control.bits.power_loss;
			break;
		case MCU_SATA_PWR:
      value = sata_pwr_state;
      break;
		case MCU_RTC_WAKE:
				value = Regist->flashRegist.control.bits.rtc_wake_en;
			break;
		case MCU_WOL_WAKE:
			value = Regist->flashRegist.control.bits.wol_wake_en;
			break;
		case MCU_WDT_TIMOUT:
			value = Regist->flashRegist.WdtTimout;
			break;
		case MCU_WDT_CONTROL:
			value = Regist->flashRegist.control.bits.watchdog_en;
			break;
		case MCU_ETH_MAC1:
			value = Regist->flashRegist.mac[0];
			break;
		case MCU_ETH_MAC2:
			value = Regist->flashRegist.mac[1];
			break;
		case MCU_ETH_MAC3:
			value = Regist->flashRegist.mac[2];
			break;
		case MCU_ETH_MAC4:
			value = Regist->flashRegist.mac[3];
			break;
		case MCU_ETH_MAC5:
			value = Regist->flashRegist.mac[4];
			break;
		case MCU_ETH_MAC6:
			value = Regist->flashRegist.mac[5];
			break;
		case MCU_REBOOT_FLAG:
			value = Regist->soc_reboot_flag;
			break;
		case DEBUG_PM_EVENT:
			value = Regist->pm_event;
			break;
		case DEBUG_WDT_TIME:
			value = (WatchdogAlarmItem.current_time_ms) / 1000 /6;
			break;
		case DEBUG_POWERON_TIMES:
			value = get_poweron_times();
			break;
		case MCU_LED_STATE:
			value = Regist->RgbState;
			break;
		case MCU_LED_COLOR_H:
			value = Regist->flashRegist.color[0];
			break;
		case MCU_LED_COLOR_M:
			value = Regist->flashRegist.color[1];
			break;
		case MCU_LED_COLOR_L:
			value = Regist->flashRegist.color[2];
			break;
		case MCU_LED_BLINK_TIMER:
			value = Regist->flashRegist.blinkTimer;
			break;
		case MCU_ADC_KEY:
			value = adc_key_get();
			break;
		case MCU_ADC_KEY_RAW_H:
			value = (uint8_t)((adc_key_raw() >> 8) & 0x03);	/* 10 bit */
			break;
		case MCU_ADC_KEY_RAW_L:
			value = (uint8_t)(adc_key_raw() & 0xFF);
			break;
		default:
			break;
	}
	DEBUG("Transform get regist: %02x -> %02x\n",reg,value); 
	return value;
}


void Delay100us(uint32_t u32CNT)
{
	while (u32CNT != 0)
	{
		TIMER_Delay(TIMER0,100);
		u32CNT --;
	}
}
//------------------------------------------------------------------------------
void Delay1ms(uint32_t u32CNT)
{
	while (u32CNT != 0)
	{
		TIMER_Delay(TIMER0,1000);
		u32CNT --;
	}
}

//------------------------------------------------------------------------------
void Delay10ms(uint32_t u32CNT)
{
	while (u32CNT != 0)
	{
		TIMER_Delay(TIMER0,10000);
		u32CNT --;
	}
}
//------------------------------------------------------------------------------
void Delay500us(uint32_t u32CNT)
{
	while (u32CNT != 0)
	{
		TIMER_Delay(TIMER0,500);
		u32CNT --;
	}
}
//------------------------------------------------------------------------------
void Delay100ms(uint32_t u32CNT)
{
	while (u32CNT != 0)
	{
		TIMER_Delay(TIMER0,100000);
		u32CNT --;
	}
}
//------------------------------------------------------------------------------
void Delay10us(uint32_t u32CNT)
{
	while (u32CNT != 0)
	{
		TIMER_Delay(TIMER0,10);
		u32CNT --;
	}
}

//------------------------------------------------------------------------------
void Delay1us(uint32_t u32CNT)
{
	TIMER_Delay(TIMER0,u32CNT);
}
void FlashRegistUpdate(void){
	uint8_t i;
	struct RegistBuffer *Regist = (struct RegistBuffer*)RegistBuffer;
	uint32_t* flashBuffer = (uint32_t *)&Regist->flashRegist;
	SYS_UnlockReg();
	FMC_ENABLE_ISP();
	FMC_Erase(DATA_FLASH_BASE);
	for(i = 0 ; i < MAX_FLASH_REG; i++){
		FMC_Write(DATA_FLASH_BASE+i*4, flashBuffer[i]);
	}
	FMC_DISABLE_ISP();
	SYS_LockReg();
}


void InitDefaultFlashRegistBuffer(void){
        struct RegistBuffer *Regist = (struct RegistBuffer*)RegistBuffer;
        Regist->vid = MCU_FW_VID;
        Regist->pid = MCU_FW_PID;
        Regist->version = MCU_FW_VERSION;
        Regist->wdt_test = 0;
        Regist->boot_type = 0;
        Regist->pm_event = EVENT_HW_RESET;
        Regist->RgbState = LED_MODE_ON;
        Regist->flashRegist.Signature = AP_FIREWARE_GOOD_SIGNATURE;
        Regist->flashRegist.control.bits.power_loss = 0;
				Regist->flashRegist.control.bits.rtc_wake_en = 1;
        Regist->flashRegist.control.bits.wol_wake_en = 1; 
        Regist->flashRegist.control.bits.watchdog_en = 0;
				Regist->flashRegist.WdtTimout = 2;
        Regist->flashRegist.PwrInDelay = 0;
				//a6-0b-0c-3f-12-f4
        Regist->flashRegist.mac[0] = 0xA6;
        Regist->flashRegist.mac[1] = 0x0B;
        Regist->flashRegist.mac[2] = 0x0C;
        Regist->flashRegist.mac[3] = 0x3F;
        Regist->flashRegist.mac[4] = 0x12;
        Regist->flashRegist.mac[5] = 0xF4;
        Regist->flashRegist.color[0] = 0xff;
        Regist->flashRegist.color[1] = 0x00;
        Regist->flashRegist.color[2] = 0x00;
        Regist->flashRegist.blinkTimer = 0x05;
}

void InitFlashAndRegist(void){
	uint32_t value;
	uint8_t i;
	struct RegistBuffer *Regist = (struct RegistBuffer*)RegistBuffer;
	uint32_t* flashBuffer = (uint32_t *)&Regist->flashRegist;
	Regist->vid = MCU_FW_VID;
	Regist->pid = MCU_FW_PID;
	Regist->version = MCU_FW_VERSION;
	Regist->RgbState = LED_MODE_ON;
	SYS_UnlockReg();
	FMC_ENABLE_ISP();
	for(i = 0 ; i < MAX_FLASH_REG; i++){
	value = FMC_Read(DATA_FLASH_BASE+i*4);
	flashBuffer[i] = value;
	}
	if((Regist->flashRegist.Signature & ~(AP_FIRMWARE_GOOD_MASK)) != AP_SIGNATURE)
	{
		InitDefaultFlashRegistBuffer();
		FlashRegistUpdate();
	}else if((Regist->flashRegist.Signature & AP_FIRMWARE_GOOD_MASK) != AP_FIREWARE_GOOD_VALUE)
	{
		Regist->flashRegist.Signature = AP_FIREWARE_GOOD_SIGNATURE;
		FlashRegistUpdate();
	}
	FMC_DISABLE_ISP();
	SYS_LockReg(); 
}

void core_init(void)
{
	/* SysTick carries the ADC key scan, and one of those conversions holds
	   its handler for about 520us (the sample window, see adc_key.c).  Every
	   other interrupt in this project - I2C, TMR1 - sits at the reset
	   priority 0, so putting SysTick at the lowest level lets them preempt
	   that conversion instead of waiting it out.  __NVIC_PRIO_BITS is 2, so
	   3 is the lowest level there is.  CLK_EnableSysTick() in board.c does
	   not touch the NVIC, so this is the only place the priority is set. */
	NVIC_SetPriority(SysTick_IRQn, 3);
	InitFlashAndRegist();
	pm_task_init();
	wol_init();
	update_task_init();
	Timer1_Init();
	init_led_colors();
	adc_key_init();
}

