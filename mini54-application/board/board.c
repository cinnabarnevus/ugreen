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
#include "board.h"



void SYS_Clock(void){   //External 12MHz
	/* Unlock protected registers */
	SYS_UnlockReg();
	SYS->P5_MFP &= ~(SYS_MFP_P50_Msk | SYS_MFP_P51_Msk);
	SYS->P5_MFP |= (SYS_MFP_P50_XTAL1 | SYS_MFP_P51_XTAL2);
	CLK->PWRCON &= ~CLK_PWRCON_XTLCLK_EN_Msk;
	CLK->PWRCON |= (0x1 << CLK_PWRCON_XTLCLK_EN_Pos);
	CLK_WaitClockReady( CLK_CLKSTATUS_XTL_STB_Msk);
	/* STCLK to XTL STCLK to XTL */
	CLK_SetSysTickClockSrc(CLK_CLKSEL0_STCLK_S_XTAL);

	CLK->CLKSEL0 &= ~CLK_CLKSEL0_HCLK_S_Msk;
	CLK->CLKSEL0 |= CLK_CLKSEL0_HCLK_S_XTAL;
	CLK->CLKSEL0 = (CLK->CLKSEL0 & ~CLK_CLKSEL0_STCLK_S_Msk) | CLK_CLKSEL0_STCLK_S_XTAL;
	//UART CLOCK
	CLK->CLKSEL1 &= ~CLK_CLKSEL1_UART_S_Msk;
	CLK->CLKSEL1 |= (0x0 << CLK_CLKSEL1_UART_S_Pos);// Clock source from external 12 MHz or 32 KHz crystal clock
	//CLK->APBCLK |= CLK_APBCLK_UART_EN_Msk; // UART Clock Enable
	CLK_EnableModuleClock(UART_MODULE);
	CLK_EnableModuleClock(I2C_MODULE);
	SystemCoreClockUpdate();
	//UART pin
	SYS->P1_MFP &= ~(SYS_MFP_P12_Msk | SYS_MFP_P13_Msk);
	SYS->P1_MFP |= (SYS_MFP_P12_RXD | SYS_MFP_P13_TXD);
	//I2C pin
	SYS->P3_MFP &= ~(SYS_MFP_P34_Msk | SYS_MFP_P35_Msk);
	SYS->P3_MFP |= (SYS_MFP_P34_SDA | SYS_MFP_P35_SCL);  
	//timer0
	CLK->CLKSEL1 &= ~CLK_CLKSEL1_TMR0_S_Msk;  // Clock source from external 12 MHz or 32 KHz crystal clock
	CLK->APBCLK |= CLK_APBCLK_TMR0_EN_Msk; // Timer 0 clock Enable
	//timer1
	CLK->CLKSEL1 &= ~CLK_CLKSEL1_TMR1_S_Msk;  // Clock source from external 12 MHz or 32 KHz crystal clock
	CLK->APBCLK |= CLK_APBCLK_TMR1_EN_Msk; // Timer 1 clock Enable
  //WDT
  CLK->APBCLK |= CLK_APBCLK_WDT_EN_Msk; // WDT clock Enable
	//gpio pin
	SYS->P0_MFP &= ~SYS_MFP_P06_Msk;
	SYS->P0_MFP |= SYS_MFP_P06_GPIO;

	SYS->P4_MFP &= ~SYS_MFP_P06_Msk;
	SYS->P4_MFP |= SYS_MFP_P06_GPIO;

	SYS->P4_MFP &= ~SYS_MFP_P07_Msk;
	SYS->P4_MFP |= SYS_MFP_P07_GPIO;

	/* P2.2 defaults to PWM0, force it to a plain GPIO for the ARGB data */
	SYS->P2_MFP &= ~SYS_MFP_P22_Msk;

	SYS_LockReg();
}

void SYS_Platform_Board_init(void)
{
	SYS_ResetModule(SYS_IPRSTC2_TMR0_RST_Msk);
	TIMER_Start(TIMER0);
	TIMER_Start(TIMER1);
	UART_Open(UART, 115200);
	CLK_EnableSysTick(CLK_CLKSEL0_STCLK_S_XTAL, SYSTICK_DELAY);
	SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;
}

void SYS_Init(void)
{
	SYS_Clock();
	SYS_Platform_Board_init(); 
}

void GPIO_Init(void)
{
	GPIO_SetMode(P0, BIT0, GPIO_PMD_OUTPUT);  //VIN_PWR_ON_MINI54
	GPIO_SetMode(P0, BIT1, GPIO_PMD_INPUT);  //HOST_WOL_PME_IN
	GPIO_SetMode(P0, BIT4, GPIO_PMD_OUTPUT);  //HOST_WOL_EN_OUT
	GPIO_SetMode(P0, BIT5, GPIO_PMD_INPUT);  // 
	GPIO_SetMode(P0, BIT6, GPIO_PMD_OUTPUT);  //NV_POWER_EN_MINI54
	GPIO_SetMode(P0, BIT7, GPIO_PMD_OUTPUT);  //MCU_OUTPUT_INTERRUPT_OUT
	
	GPIO_SetMode(P1, BIT0, GPIO_PMD_OUTPUT);   //NOT USE
	GPIO_SetMode(P1, BIT4, GPIO_PMD_OUTPUT);  //HOST_SOC_RST
	GPIO_SetMode(P1, BIT5, GPIO_PMD_INPUT);  //KEY_ADC P1.5/AIN5 (analog setup in adc_key_init)
	
	GPIO_SetMode(P2, BIT2, GPIO_PMD_OUTPUT);  //MCU_RGB_CTL (ARGB data)
  GPIO_SetMode(P2, BIT3, GPIO_PMD_OUTPUT);  //HOST_POWER_ENABLE
	
	SATA_PWR_EN_OUT = !SATA_PWR_ACTIVE_LEVEL;   /* 先置为断电电平 */
	GPIO_SetMode(P2, BIT4, GPIO_PMD_OUTPUT);  //MCU_P2.4_SATA_CTR
	
	GPIO_SetMode(P2, BIT5, GPIO_PMD_OUTPUT);  //HOST_PWR_BUTTIN_OUT
	GPIO_SetMode(P2, BIT6, HOST_SYSTEM_RST_OUTPUT);  // HOST_SYSTEM_RST_OUTPUT
	
	GPIO_SetMode(P3, BIT0, GPIO_PMD_INPUT);   //PWR_BUTTON_IN 
	GPIO_SetMode(P3, BIT1, GPIO_PMD_INPUT);  //HOST_RECOVERY_IN
	GPIO_SetMode(P3, BIT2, GPIO_PMD_INPUT);  //HOST_POWER_STATUS_BIT0 
	GPIO_SetMode(P3, BIT6, GPIO_PMD_INPUT);  //HOST_WDG_FEED_DOG_IN
	
	GPIO_SetMode(P4, BIT6, GPIO_PMD_OUTPUT);  //HOST_ETH_PHY_MDC
	GPIO_SetMode(P4, BIT7, GPIO_PMD_OUTPUT);   //HOST_ETH_PHY_MDIO
	
	GPIO_SetMode(P5, BIT2, GPIO_PMD_INPUT);  //HOST_RTC_PME_IN
	GPIO_SetMode(P5, BIT3, GPIO_PMD_INPUT);  // HOST_POWER_IS_ENABLE
	GPIO_SetMode(P5, BIT4, GPIO_PMD_INPUT);  //HOST_POWER_STATUS_BIT1	

	HOST_SOC_RST = 1;
	HOST_SYSTEM_RST_OUTPUT = 1;
	MCU_RGB_CTL = 0;
	
	HOST_PWR_BUTTIN_OUT = 1;
	HOST_WOL_EN_OUT = 0;
	MCU_OUTPUT_INTERRUPT_OUT = 1;
	/* Reset the power pin of NV */
	VIN_PWR_ON_MINI54 = 0;
	NV_POWER_EN_MINI54 = 0;
}

HOST_STATE get_host_power_status(void)
{
	if(HOST_POWER_STATUS_BIT0 == 0 && HOST_POWER_STATUS_BIT1 == 0){
		return POWER_ON;
	}
	else if(HOST_POWER_STATUS_BIT0 == 1 && HOST_POWER_STATUS_BIT1 == 0){
		return SUSPEND;
	}
	else if(HOST_POWER_STATUS_BIT0 == 1 && HOST_POWER_STATUS_BIT1 == 1){
		return POWER_OFF;
	}
	else{
		return OTHER;
	}
}

void I2C_Init(void)
{
	I2C_Open(I2C, 100000);
	I2C->I2CADDR0  = (0x15 << 1);
	I2C->I2CON |= I2C_I2CON_EI_Msk;
	NVIC_SetPriority(I2C_IRQn,0);
	NVIC_EnableIRQ(I2C_IRQn);
	I2C_SET_CONTROL_REG(I2C, I2C_SI | I2C_AA);
}

void WDT_Init(void)
{
       SYS_UnlockReg();
       /* The clock of WDT is 10KHz, (2^16)/10000 = 65536/10000 = 6.6s */
       WDT_Open(WDT_TIMEOUT_2POW16, 0, TRUE, FALSE);
       SYS_LockReg();
}

void WDT_Feed(void)
{
       SYS_UnlockReg();
       WDT->WTCR |= WDT_WTCR_WTR_Msk;
       SYS_LockReg();
}

void board_init(void)
{
	SYS_Init();
	GPIO_Init();
	I2C_Init();
//  WDT_Init();
}

