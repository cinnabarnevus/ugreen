
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
#include "update.h"

struct TransBufferItem UpdateTransItem;    

uint8_t update_flag = 0;

struct task_struct UpdateTask = {
	.name		= "updata task",
	.function	= UpdateTaskWorker,
};

struct TransBufferCallBack UpdateCallback = {
	.name		= "update trans callback",
	.callback	= UpdateTransCallback,
};
 
int UpdateTaskWorker(struct task_struct *__this)
{
	if(update_flag == 1){
		update_flag = 0;
		RebootToLoader();
	}
	return 0;
}   


void RebootToLoader(void){
	SYS_UnlockReg();
	/* Disable WDT */
	WDT_Close();
	FMC_SET_LDROM_BOOT();
	// NVIC_SystemReset();
	/* Reset CPU only */
	SYS->IPRSTC1 |= SYS_IPRSTC1_CPU_RST_Msk;
	SYS_LockReg();
}

void RebootUpdateLoader(void){
	struct RegistBuffer *Regist = (struct RegistBuffer*)RegistBuffer;
	SYS_UnlockReg();
	FMC_ENABLE_ISP();
	Regist->flashRegist.Signature &= ~(AP_FIRMWARE_GOOD_MASK);
	FlashRegistUpdate();
	FMC_DISABLE_ISP();
	update_flag = 1;
	SYS_LockReg();
}

int UpdateTransCallback(uint8_t reg,uint8_t value) {
	int ret = -1;
	switch(reg){
		case MCU_UPDATE_CTRL:

		if(value == 1){
			DEBUG("update enable->  reg:%02x value:%02x\n",reg,value);
			RebootUpdateLoader();
		}
		ret = 0;          
		break;

		default:
			break;
	}
	return ret;
}

void update_task_init(void)
{
	update_flag = 0;

	RegistTaskWorker(&UpdateTask);

	UpdateTransItem.trans = &UpdateCallback;
	RegistTransBufferCallback(&UpdateTransItem);
}










