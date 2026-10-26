/******************************************************************************
 * @file     main.c
 * @version  V1.00
 * $Revision: 3 $
 * $Date: 15/02/13 2:19p $
 * @brief    A project template for Mini58 MCU.
 *
 * @note
 * Copyright (C) 2015 Nuvoton Technology Corp. All rights reserved.
*****************************************************************************/
#include <stdio.h>
#include "Mini51Series.h"
#include "core.h"
#include "board.h"
#include "led.h"
#include "adc_key.h"
#include <string.h>
#include "pm.h"

int FMC_Read_(unsigned int address, unsigned int * data)
{
	FMC->ISPCMD = FMC_ISPCMD_READ;
	FMC->ISPADR = address;
	FMC->ISPDAT = 0x00000000;
	FMC->ISPTRG = FMC_ISPTRG_ISPGO_Msk; 

	__ISB();
	while(FMC->ISPTRG);
	*data = FMC->ISPDAT;

	return 0;
} 

int main()
{
	uint32_t config0;
	board_init();
	core_init();
	DEBUG("\n\n");
	DEBUG("+---------------------------------------+ \n");
	DEBUG("|      Mini54 Aprom firmware code       | \n");
	DEBUG("|          Author: chenyongchao         | \n");
	DEBUG("|          Firmware verson: %d           | \n",MCU_FW_VERSION);
	DEBUG("+---------------------------------------+ \n");
	DEBUG("build time: %s %s\n", __DATE__, __TIME__);
	DEBUG("\n\n");

	SYS_UnlockReg();
	FMC_ENABLE_ISP();
	FMC_Read_(FMC_CONFIG_BASE,&config0);
	FMC_DISABLE_ISP();
	DEBUG("config:    %08x\n",config0);
	SYS_LockReg();

	while(1){
		ScheduleTaskWorkerLooper();
		RGB_Process();
		//adc_key_debug();
	}
}

/*** (C) COPYRIGHT 2015 Nuvoton Technology Corp. ***/
