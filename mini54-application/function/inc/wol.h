#ifndef __WOL_H__
#define __WOL_H__
#include <stdio.h>
#include "Mini51Series.h"

#define SMSC8742_SUPPORT	0
#define RTL8201F_SUPPORT	0
#define RTL8211E_SUPPORT	1
#define RTL8211F_SUPPORT	1
#define RTL8261C_SUPPORT	1
#define JL1111_SUPPORT	1
#define JL2121_SUPPORT	1
#define YT8521S_SUPPORT 1
#define YT8531S_SUPPORT 1

#define SMSC8742_ID	0x0007c130
#define RTL8201F_ID	0x001cc810
#define RTL8211E_ID     0x001cc910  
#define RTL8211F_ID     0x001cc916
#define RTL8261C_ID     0x001cc890
#define JL1111_ID     0x937c4020 
#define JL2121_ID     0x937c4030
#define YT8521S_ID   0x00000110
#define YT8531S_ID   0x4f51e910

#define MDIO_READ       2
#define MDIO_WRITE      1

//SMSC8742
#define	PHY_ADD	0
#define	PHY_MMD_ACCESS_CTL	13UL
#define PHY_MMD_ACCESS_ADDR_DATA	14UL
#define	MMD_PCS	3
#define	PHY_RX_ADDRA	32865UL /*3.32865*/
#define	PHY_RX_ADDRB	32866UL
#define	PHY_RX_ADDRC	32867UL
#define	PHY_WUCSR	32784UL
#define	PHY_WUCSR_IF_DIS_	(uint32_t)0x8000U
#define	PHY_WUCSR_LED1_FUNC_	(uint32_t)0x6000U
#define	PHY_WUCSR_LED1_FUNC_MASK_	(uint32_t)0x6000U
#define	PHY_WUCSR_LED1_FUNC_SHIFT_	13
#define	PHY_WUCSR_LED1_FUNC_LNK_ACT_	(uint32_t)0x0U
#define	PHY_WUCSR_LED1_FUNC_nINT_	(uint32_t)0x1U
#define	PHY_WUCSR_LED1_FUNC_nPME_	(uint32_t)0x2U
#define	PHY_WUCSR_LED1_FUNC_LNK_SPD_	(uint32_t)0x3U

#define	PHY_WUCSR_LED2_FUNC_	(uint32_t)0x1800U
#define	PHY_WUCSR_LED2_FUNC_MASK_	(uint32_t)0x1800U
#define	PHY_WUCSR_LED2_FUNC_SHIFT_	11
#define	PHY_WUCSR_LED2_FUNC_LNK_SPD_	(uint32_t)0x0U
#define	PHY_WUCSR_LED2_FUNC_nINT_	(uint32_t)0x1U
#define	PHY_WUCSR_LED2_FUNC_nPME_	(uint32_t)0x2U
#define	PHY_WUCSR_LED2_FUNC_LNK_ACT_	(uint32_t)0x3U

#define	PHY_WUCSR_RXD2_RMIISEL_	(uint32_t)0x0400U
#define	PHY_WUCSR_nPME_SLFCLR_	(uint32_t)0x0200U

#define	PHY_WUCSR_CFGED_	(uint32_t)0x0100U	/*Wol configured */
#define	PHY_WUCSR_PFDA_FR_	(uint32_t)0x0080U	/* Perfect DA Frame Receiced */
#define	PHY_WUCSR_WUFR_		(uint32_t)0x0040U	/*Wake up frame Received */
#define PHY_WUCSR_MPR_		(uint32_t)0x0020U	/* MagicPacket Received*/
#define	PHY_WUCSR_BCST_FR_	(uint32_t)0x0010U	/* Broadcast Frame Receviced */
#define	PHY_WUCSR_PFDA_EN_	(uint32_t)0x0008U	/*Perfect DA Wakeup Enable */
#define	PHY_WUCSR_WAKE_EN_	(uint32_t)0x0004U	/*Wake Frame Enable */
#define	PHY_WUCSR_MPEN_		(uint32_t)0x0002U	/* MagicPacket Enable */
#define	PHY_WUCSR_BCST_EN_	(uint32_t)0x0001U	/*Broadcast Enable */

#define	PHY_WUF_CFGA	32785UL
#define	PHY_WUF_CFGA_FILTER_EN_	(uint32_t)0x8000U
#define	PHY_WUF_CFGA_FILTER_TRGRD_	(uint32_t)0x4000U

#define	PHY_WUF_CFGA_DA_MTCH_MASK_	(uint32_t)0x0700U
#define	PHY_WUF_CFGA_ADDR_MTCH_EN_	(uint32_t)0x0400U
#define	PHY_WUF_CFGA_ANY_MLTCST_EN_	(uint32_t)0x0200U
#define	PHY_WUF_CFGA_BRDCST_EN_		(uint32_t)0x0100U

#define	PHY_WUF_CFGA_PTTN_OFFSET_	(uint32_t)0x00FFU
#define	PHY_WUF_CFGA_PTTN_OFFSET_MASK_	(uint32_t)0x00FFU

#define	PHY_WUF_CFGB	32786UL
#define	PHY_WUF_CFGB_CRC16_MASK_	(uint32_t)0xFFFFU

#define	PHY_WUF_MASK_7	32801UL
#define	PHY_WUF_MASK_6	32802UL
#define	PHY_WUF_MASK_5	32803UL
#define	PHY_WUF_MASK_4	32804UL
#define	PHY_WUF_MASK_3	32805UL
#define	PHY_WUF_MASK_2	32806UL
#define	PHY_WUF_MASK_1	32807UL
#define	PHY_WUF_MASK_0	32808UL

#define PHY_NUM    8

struct PhyInit {
        uint32_t  id;
        void (*init)(void);
};

struct PhyDate {
        uint32_t  phy_id;
        struct PhyInit phyInit[PHY_NUM];
};
void wol_init(void);
void detect_phy(void);
int config_wol(void);
#if SMSC8742_SUPPORT 
void Phy8742_Wol_Config(void);
#endif
#if RTL8201F_SUPPORT 
void Phy8021fn_Wol_Config(void);
#endif
#if RTL8211E_SUPPORT  
void Phy8211e_Wol_Config(void);
#endif
#if RTL8211F_SUPPORT 
void Phy8211f_Wol_Config(void);
#endif
#if RTL8261C_SUPPORT
void Phy8261c_Wol_Config(void);
#endif
#ifdef JL1111_SUPPORT
void PhyJL1111_Wol_Config(void);
#endif
#ifdef JL2121_SUPPORT
void PhyJL2121_Wol_Config(void);
#endif
#ifdef YT8521S_SUPPORT
void yt8521s_Wol_Config(void);
#endif
int WolTransCallback(uint8_t reg,uint8_t value);
uint8_t wol_int_flag(void);
#endif