/******************************************************************************
 * @file     ISP_USER.h
 * @brief    General UART ISP slave header file
 * @version  1.0.0
 * @date     22, Sep, 2014
 *
 * @note
 * Copyright (C) 2015 Nuvoton Technology Corp. All rights reserved.
 ******************************************************************************/
#ifndef ISP_USER_H
#define ISP_USER_H

#define FW_VERSION                  6

#include "FMC_USER.h"

struct ap_info_header {
	uint32_t magic;
#define AP_MAGIC	0x56781234
	uint32_t ap_version;
	uint32_t ap_command;
	uint32_t ap_argument;
};

struct i2c_message_t {
	uint32_t cmd;
	uint32_t data_w_len;
	uint32_t data_u32[1];
}__align(64);

#define CMD_UPDATE_APROM            0x000000A0
#define CMD_GET_LD_VERSION          0x000000A1
#define CMD_ERASE_ALL               0x000000A2
#define CMD_ERASE_APROM             0x000000A3
#define CMD_RUN_APROM               0x000000A4
#define CMD_RUN_LDROM               0x000000A5
#define CMD_GET_AP_CHECKSUM         0x000000A6

#define V6M_AIRCR_VECTKEY_DATA      0x05FA0000UL
#define V6M_AIRCR_SYSRESETREQ       0x00000004UL

#define APROM_STATUS_MASK		(0x000000FFUL)
#define APROM_STATUS_READY		(0x000000AAUL)

extern void GetDataFlashInfo(uint32_t *addr, uint32_t *size);
extern uint32_t GetApromSize(void);
extern int ParseCmd(unsigned char *buffer, uint8_t len);
extern uint32_t g_apromSize, g_dataFlashAddr, g_dataFlashSize;

extern __align(4) uint8_t usb_rcvbuf[];
extern __align(4) uint8_t usb_sendbuf[];
extern __align(4) uint8_t response_buff[64];
#endif  // #ifndef ISP_USER_H
