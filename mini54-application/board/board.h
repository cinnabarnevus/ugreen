#ifndef __BOARD_H__
#define __BOARD_H__

#ifdef __cplusplus
extern "C"
{
#endif
  
#include <stdio.h>
#include "Mini51Series.h"
#include "pm.h"

#define SYSTICK_DELAY				SystemCoreClock/1000                      //1ms

/* ARGB LED data output */
#define  MCU_RGB_CTL														P22

#define  PWR_BUTTON_IN                          P30
#define  HOST_PWR_BUTTIN_OUT                    P25
#define  HOST_RECOVERY_IN                       P31

/* RTC */
#define  HOST_RTC_PME_IN                        P52

/* WOL */
#define  HOST_WOL_EN_OUT                        P04
#define  HOST_ETH_PHY_MDC                       P46
#define  HOST_ETH_PHY_MDIO                      P47
#define  HOST_WOL_PME_IN                        P01

/* WDT */
#define  HOST_WDG_FEED_DOG_IN                   P36

/* WOL INT to PC*/
#define  MCU_OUTPUT_INTERRUPT_OUT               P07

/* force to poweroff */
#define	FORCE_OFF_MINI54												P05

/* PC power status */
#define HOST_POWER_STATUS_BIT0                  P32
#define HOST_POWER_STATUS_BIT1                  P54

//#define	HOST_POWER_IS_ENABLE										P53
#define HOST_POWER_ENABLE                  			P23
#define HOST_PWR_IN                  						P53

/* ADC key, three keys on one ADC pin : P1.5 = AIN5. */
#define  MCU_ADC_CH1_IN                         P15
#define  MCU_ADC_CH1_AIN                        5
#define  MCU_ADC_CH1_MFP                        SYS->P1_MFP
#define  MCU_ADC_CH1_MFP_Msk                    SYS_MFP_P15_Msk
#define  MCU_ADC_CH1_MFP_AIN                    SYS_MFP_P15_AIN5
#define  MCU_ADC_CH1_GPIO                       P1
#define  MCU_ADC_CH1_BIT                        BIT5

/* SATA power control, low active */
#define  SATA_PWR_EN_OUT                        P24
#define  SATA_PWR_ACTIVE_LEVEL                  0     /* 0: low active */
#define  SATA_PWR_ON()                          (SATA_PWR_EN_OUT = SATA_PWR_ACTIVE_LEVEL)
#define  SATA_PWR_OFF()                         (SATA_PWR_EN_OUT = !SATA_PWR_ACTIVE_LEVEL)
	
#define	NV_POWER_EN_MINI54											P06
#define	VIN_PWR_ON_MINI54												P00

#define  HOST_SOC_RST                      			P14
#define  HOST_SYSTEM_RST_OUTPUT									P26

#define	HOST_ETH_PHY_MDIO_INPUT			GPIO_SetMode(P4, BIT7, GPIO_PMD_INPUT);
#define	HOST_ETH_PHY_MDIO_OUTPUT		GPIO_SetMode(P4, BIT7, GPIO_PMD_OUTPUT); 
#define	HOST_ETH_PHY_MDC_OUTPUT			GPIO_SetMode(P4, BIT6, GPIO_PMD_OUTPUT); 

void board_init(void);
void SYS_Platform_Board_Clock(void);
void WDT_Feed(void);
HOST_STATE get_host_power_status(void);

#endif
