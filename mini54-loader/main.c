/****************************************************************************
 * @file     main.c
 * @version  V2.00
 * $Revision: 5 $
 * $Date: 13/11/07 4:40p $
 * @brief    MINI51 Series UART Interface ISP Driver Sample Code
 *
 * @note
 * Copyright (C) 2011 Nuvoton Technology Corp. All rights reserved.
 *
 ******************************************************************************/
#include <stdio.h>
#include <string.h>
#include "Mini51Series.h"

#include "ISP_USER.h"
#include "i2c_transfer.h"

void SYS_Init(void)
{
    /*---------------------------------------------------------------------------------------------------------*/
    /* Init System Clock                                                                                       */
    /*---------------------------------------------------------------------------------------------------------*/

    /* Unlock protected registers */
    while (SYS->RegLockAddr != 1)
    {
        SYS->RegLockAddr = 0x59;
        SYS->RegLockAddr = 0x16;
        SYS->RegLockAddr = 0x88;
    }
	/* Set P5 multi-function pins for XTAL1 and XTAL2 */
	SYS->P5_MFP &= ~(SYS_MFP_P50_Msk | SYS_MFP_P51_Msk);
	SYS->P5_MFP |= (SYS_MFP_P50_XTAL1 | SYS_MFP_P51_XTAL2);

	CLK->PWRCON |= (CLK_PWRCON_HXT);

	/* Waiting for clock ready */
	while (!(CLK->CLKSTATUS & CLK_CLKSTATUS_HXT_STB_Msk));

	CLK->CLKSEL0 = (CLK->CLKSEL0 & ~CLK_CLKSEL0_HCLK_S_Msk) | CLK_CLKSEL0_HCLK_S_XTAL;
	CLK->CLKDIV = (CLK->CLKDIV & ~CLK_CLKDIV_HCLK_N_Msk) | CLK_CLKDIV_HCLK(1);

	CLK->CLKSEL0 = (CLK->CLKSEL0 & ~CLK_CLKSEL0_STCLK_S_Msk) | CLK_CLKSEL0_STCLK_S_HCLK;

    /* Enable UART clock */
    CLK->APBCLK |= CLK_APBCLK_I2C_EN_Msk;

    /* Update System Core Clock */
    /* User can use SystemCoreClockUpdate() to calculate SystemCoreClock and CycylesPerUs automatically. */
    SystemCoreClock = __XTAL12M;
    CyclesPerUs = 12;
    /*---------------------------------------------------------------------------------------------------------*/
    /* Init I/O Multi-function                                                                                 */
    /*---------------------------------------------------------------------------------------------------------*/
    /* Set P3.4 and P3.5 for I2C SDA and SCL */
    SYS->P3_MFP = SYS_MFP_P34_SDA | SYS_MFP_P35_SCL;

    /* I2C pin enable schmitt trigger */
    SYS->P3_MFP |= SYS_MFP_TYPE_Msk(4) | SYS_MFP_TYPE_Msk(5);

    /* Lock protected registers */
    //  SYS_LockReg();
}

void GPIO_SetMode(GPIO_T *gpio, uint32_t u32PinMask, uint32_t u32Mode)
{
    uint32_t i;

    for (i=0; i<GPIO_PIN_MAX; i++)
    {
        if (u32PinMask & (1 << i))
        {
            gpio->PMD = (gpio->PMD & ~(0x3 << (i << 1))) | (u32Mode << (i << 1));
        }
    }
}

void systick_delay(uint32_t us)
{
	SysTick->LOAD = us * CyclesPerUs;
	SysTick->VAL   = (0x00);
	SysTick->CTRL = SysTick->CTRL | SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;//using cpu clock

	while( (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0 );
}

/* Start address of APROM */
#define START_ADDRESS_SRAM	0x20000000
#define SIZE_OF_SRAM		0x800

int is_aprom_valid(void) {
	uint32_t aprom_status;
	uint32_t aprom_stack_pointer;
	uint32_t aprom_reset_handler;
	uint32_t aprom_size = GetApromSize();

/* Note: The first word of APROM is stack pointer */
/* Note: The second word of APROM is entry of reset handler */
	FMC_Read_User(0, &aprom_stack_pointer);
	FMC_Read_User(4, &aprom_reset_handler);
	/* The first word in dataflash is status flag */
	FMC_Read_User(g_dataFlashAddr, &aprom_status);

	if ((aprom_status & APROM_STATUS_MASK) != APROM_STATUS_READY) {
		return 0;
	}

	if (aprom_reset_handler > g_apromSize) {
		return 0;
	}
	if (aprom_stack_pointer > (START_ADDRESS_SRAM + SIZE_OF_SRAM)) {
		return 0;
	}
	if (aprom_stack_pointer < (START_ADDRESS_SRAM)) {
		return 0;
	}
	/* Check pass */
	return 1;
}

#define HOST_POWER_ENABLE		P23
#define FORCE_OFF_MINI54	P05
#define HOST_PWR_IN			P53
#define VIN_PWR_ON		P00
#define PMIC_EN0		P06
#define GPIO_LED		P22

void gpio_init(void)
{
	GPIO_SetMode(P0, BIT0, GPIO_PMD_OUTPUT);
	GPIO_SetMode(P0, BIT5, GPIO_PMD_INPUT);
	GPIO_SetMode(P0, BIT6, GPIO_PMD_OUTPUT);
	GPIO_SetMode(P1, BIT0, GPIO_PMD_OUTPUT);
	GPIO_SetMode(P1, BIT5, GPIO_PMD_INPUT);
	GPIO_SetMode(P2, BIT2, GPIO_PMD_OUTPUT);
}

void nvidia_poweron(void)
{
	HOST_POWER_ENABLE = 0;
	systick_delay(1000 * 1000);	// 1000 Ms
	PMIC_EN0 = 0;
	systick_delay(500 * 1000);	// 500 Ms
	VIN_PWR_ON = 0;

	systick_delay(1000 * 1000);	// 1000 Ms
	VIN_PWR_ON = 1;
	systick_delay(500 * 1000);	// 500 Ms
	PMIC_EN0 = 1;

	systick_delay(500 * 1000);	// 500 Ms
	//while(HOST_PWR_IN == 0);	//wait for power_good
	systick_delay(500 * 1000);	// 500 Ms
}
void nvidia_poweroff(void)
{
	HOST_POWER_ENABLE = 1;
	systick_delay(1000 * 1000);	// 1000 Ms
	PMIC_EN0 = 0;
	systick_delay(1000 * 1000);	// 500 Ms
	VIN_PWR_ON = 0;

	systick_delay(500 * 1000);	// 500 Ms
	//while(HOST_PWR_IN == 1);	//wait for power_good down
	systick_delay(500 * 1000);	// 500 Ms
}

/*---------------------------------------------------------------------------------------------------------*/
/* MAIN function                                                                                           */
/*---------------------------------------------------------------------------------------------------------*/

int main(void)
{
    uint32_t cmd_buff[16];
    /* Init System, IP clock and multi-function I/O
       In the end of SYS_Init() will issue SYS_LockReg()
       to lock protected register. If user want to write
       protected register, please issue SYS_UnlockReg()
       to unlock protected register if necessary */
    SYS_Init();
    NVIC->ICER[0U] = 0xffffffffUL;//Disable all interrupts.

    /* Init I2C */
    I2C_Init();
		gpio_init();
    CLK->AHBCLK |= CLK_AHBCLK_ISP_EN_Msk;
    FMC->ISPCON |= FMC_ISPCON_ISPEN_Msk;
    g_apromSize = GetApromSize();
    GetDataFlashInfo(&g_dataFlashAddr, &g_dataFlashSize);

    GPIO_LED = 0;	/* True off the led */

/*
 * When entering the loader, 
 * if the SOC is live, then wait for the upgrade,
 * if the SOC is off, then enter the APROM, or power up the SOC.
 */
	if (HOST_PWR_IN == 1) {
		goto ISP;
	} else {
		if (is_aprom_valid()) {
			goto APROM;
		} else {
			nvidia_poweron();
			goto ISP;
		}
	}

ISP:
	while (1)
	{
		if (bI2cDataReady == 1)
		{
			memcpy(cmd_buff, i2c_rcvbuf, 64);
			bI2cDataReady = 0;
			ParseCmd((unsigned char *)cmd_buff, 64);
			NVIC_EnableIRQ(I2C_IRQn);
		}
		if ((FORCE_OFF_MINI54 + HOST_PWR_IN) != 2)
		{
			nvidia_poweroff();
			outpw(&SYS->RSTSRC, 3);//clear bit
			outpw(&SCB->AIRCR, (V6M_AIRCR_VECTKEY_DATA | V6M_AIRCR_SYSRESETREQ));
		}
	}

APROM:
	outpw(&SYS->RSTSRC, 3);//clear bit
	outpw(&FMC->ISPCON, inpw(&FMC->ISPCON) & 0xFFFFFFFC);
	outpw(&SCB->AIRCR, (V6M_AIRCR_VECTKEY_DATA | V6M_AIRCR_SYSRESETREQ));


	/* Trap the CPU */
	while (1);
}
