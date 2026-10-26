/******************************************************************************
 * @file     ISP_USER.c
 * @brief    M5451 series ISP sample source file
 * @version  0x27
 * @date     31, December, 2014
 *
 * @note
 * Copyright (C) 2014 Nuvoton Technology Corp. All rights reserved.
 ******************************************************************************/
#include <stdio.h>
#include "string.h"
#include "ISP_USER.h"

__align(4) uint8_t response_buff[64];
__align(4) static uint8_t aprom_buf[FMC_FLASH_PAGE_SIZE];
uint32_t bUpdateApromCmd;
uint32_t g_apromSize, g_dataFlashAddr, g_dataFlashSize;

static uint32_t Checksum(unsigned char *buf, int len)
{
    int i;
    uint32_t c;

    for (c = 0, i = 0 ; i < len; i++)
    {
        c += buf[i];
    }

    return (c);
}

static uint32_t CalCheckSum(uint32_t start, uint32_t len)
{
    int i;
    register uint32_t lcksum = 0;

    for (i = 0; i < len; i += FMC_FLASH_PAGE_SIZE)
    {
        ReadData(start + i, start + i + FMC_FLASH_PAGE_SIZE, (uint32_t *)aprom_buf);

        if (len - i >= FMC_FLASH_PAGE_SIZE)
        {
            lcksum += Checksum(aprom_buf, FMC_FLASH_PAGE_SIZE);
        }
        else
        {
            lcksum += Checksum(aprom_buf, len - i);
        }
    }

    return lcksum;
}

//bAprom == TRUE erase all aprom besides data flash
void EraseAP(unsigned int addr_start, unsigned int addr_end)
{
    unsigned int eraseLoop = addr_start;

    for (; eraseLoop < addr_end; eraseLoop += FMC_FLASH_PAGE_SIZE)
    {
        FMC_Erase_User(eraseLoop);
    }

    return;
}

//void UpdateConfig(unsigned int *data, unsigned int *res)
//{
//    FMC_ENABLE_CFG_UPDATE();
//    FMC_Erase_User(Config0);
//    FMC_Write_User(Config0, *data);
//    FMC_Write_User(Config1, *(data + 1));
//
//    if (res)
//    {
//        FMC_Read_User(Config0, res);
//        FMC_Read_User(Config1, res + 1);
//    }
//
//    FMC_DISABLE_CFG_UPDATE();
//}

/* Test Command in linux */
// GET_LDVERSION: i2ctransfer -y 1 w64@0x10 0xa1 0x00= r64

int ParseCmd(unsigned char *buffer, uint8_t len)
{
	static uint32_t w_addr = 0;
	struct i2c_message_t *mesg = (void *)buffer;
	struct i2c_message_t *r_mesg = (void *)response_buff;
	
	switch(mesg->cmd) {
		case CMD_ERASE_ALL:
			w_addr = 0;
			EraseAP(0, g_dataFlashAddr + g_dataFlashSize);
			break;

		case CMD_ERASE_APROM:
			w_addr = 0;
			//EraseAP(0, g_dataFlashAddr);
			/* Erase All Flash */
			EraseAP(0, g_dataFlashAddr + g_dataFlashSize);
			break;

		case CMD_RUN_APROM:
			/* The first word in dataflash is status flag */
			FMC_Write_User(g_dataFlashAddr, APROM_STATUS_READY);
			outpw(&SYS->RSTSRC, 3);//clear bit
			outpw(&FMC->ISPCON, inpw(&FMC->ISPCON) & 0xFFFFFFFC);
			outpw(&SCB->AIRCR, (V6M_AIRCR_VECTKEY_DATA | V6M_AIRCR_SYSRESETREQ));
			break;

		case CMD_RUN_LDROM:
			FMC->ISPCON |= FMC_ISPCON_BS_Msk;
			FMC->ISPCON &= ~(FMC_ISPCON_ISPEN_Msk);
			NVIC_SystemReset();
			break;

		case CMD_GET_LD_VERSION:
			r_mesg->cmd = CMD_GET_LD_VERSION;
			r_mesg->data_w_len = 1;
			r_mesg->data_u32[0] = FW_VERSION;
			break;

		case CMD_UPDATE_APROM:
			if (len - 8 < (mesg->data_w_len * 4)) {
				break;
			}
			WriteData(w_addr, w_addr + (mesg->data_w_len * 4), mesg->data_u32);
			w_addr += (mesg->data_w_len * 4);
			break;
			
		case CMD_GET_AP_CHECKSUM:
			if (mesg->data_u32[0] > g_apromSize) {
				break;
			}
			r_mesg->cmd = CMD_GET_AP_CHECKSUM;
			r_mesg->data_w_len = 1;
			r_mesg->data_u32[0] = CalCheckSum(0, mesg->data_u32[0]);
			break;

		default:
			//Unkown command
			break;
	}
	return 0;
}

