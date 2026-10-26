
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
#include "wol.h"
#include "pm.h"

#define ENABLE_WOL

struct TransBufferItem WolTransItem;    


struct PhyDate mPhyData = {
                .phy_id = 0,
                .phyInit = {
#if SMSC8742_SUPPORT 
                    {SMSC8742_ID,Phy8742_Wol_Config},
#endif
#if RTL8201F_SUPPORT 
                    {RTL8201F_ID,Phy8021fn_Wol_Config},
#endif
#if RTL8211E_SUPPORT 
                    {RTL8211E_ID,Phy8211e_Wol_Config},
#endif
#if RTL8211F_SUPPORT 
                    {RTL8211F_ID,Phy8211f_Wol_Config},  
#endif
#if RTL8261C_SUPPORT 
										{RTL8261C_ID,Phy8261c_Wol_Config},
#endif
#if JL1111_SUPPORT 
                    {JL1111_ID,PhyJL1111_Wol_Config},   
#endif
#if YT8521S_SUPPORT 
                    {YT8521S_ID,yt8521s_Wol_Config},   
#endif
#if YT8531S_SUPPORT 
                    {YT8531S_ID,yt8521s_Wol_Config},   
#endif
#if JL2121_SUPPORT
                    {JL2121_ID,PhyJL2121_Wol_Config},   
#endif
                    {0xffffffff,NULL},  						
                },
};
 
struct TransBufferCallBack WolCallback = {
    	.name		= "Wol trans callback",
        .callback         = WolTransCallback,
};
   

int WolTransCallback(uint8_t reg,uint8_t value) {
    int ret = -1;
    //uint8_t need_update = 0;
    struct RegistBuffer *Regist = (struct RegistBuffer*)RegistBuffer;
    //DEBUG("wol->  reg:%02x value:%02x\n",reg,value);

    switch(reg){
      case MCU_ETH_MAC1:
          if(Regist->flashRegist.mac[0] != value){
              Regist->flashRegist.mac[0] = value;
              FlashRegistUpdate();
          } 
          ret = 0;          
          break;

      case MCU_ETH_MAC2:
          if(Regist->flashRegist.mac[1] != value){
              Regist->flashRegist.mac[1] = value;
              FlashRegistUpdate();
          } 
          ret = 0;          
          break;
        
      case MCU_ETH_MAC3:
          if(Regist->flashRegist.mac[2] != value){
              Regist->flashRegist.mac[2] = value;
              FlashRegistUpdate();
              
          } 
          ret = 0;          
          break;
        
      case MCU_ETH_MAC4:
          if(Regist->flashRegist.mac[3] != value){
              Regist->flashRegist.mac[3] = value;
              FlashRegistUpdate();
          } 
          ret = 0;          
          break;
        
      case MCU_ETH_MAC5:
          if(Regist->flashRegist.mac[4] != value){
              Regist->flashRegist.mac[4] = value;
              FlashRegistUpdate();
          } 
          ret = 0;          
          break;
          
      case MCU_ETH_MAC6:
          if(Regist->flashRegist.mac[5] != value){
              Regist->flashRegist.mac[5] = value;
              FlashRegistUpdate();
          }  
          ret = 0;          
          break;      
      default:
        break;
    }
//    if( need_update == 1 ) {
//       FlashRegistUpdate();
//    }
    return ret;
}

static void phy_delay_5us(void)
{
	uint32_t count = 1;
	while(count){
		count--;
	}
 }


static void MDC_OUT(void)
{
     HOST_ETH_PHY_MDC_OUTPUT; 
     HOST_ETH_PHY_MDC=0;
 }

 static void MDIO_OUT(void)
  {
     HOST_ETH_PHY_MDIO_OUTPUT; ; 
     HOST_ETH_PHY_MDIO = 0;
  }
  
  static void MDIO_IN(void)
  {
     HOST_ETH_PHY_MDIO_INPUT; 
  }
  
 static void MDC_H(void)
 {
     HOST_ETH_PHY_MDC=1;
 }
 
 static void MDC_L(void)
 {
     HOST_ETH_PHY_MDC=0;
 }
 
 static int GET_MDIO(void)
 {
     return HOST_ETH_PHY_MDIO;
 }
 
 static void SET_MDIO(int val)
 {
     HOST_ETH_PHY_MDIO=val;
 }
 
 
 static void mdio_bb_send_bit(int val)
 {
     phy_delay_5us();
     MDC_L();
     SET_MDIO(val);
     phy_delay_5us();
     MDC_H();
 }
 
 static int mdio_bb_get_bit(void)
 {
     int value;  
     phy_delay_5us();
     MDC_L();
     phy_delay_5us();
     MDC_H();
     //Delay10us(3);	 
     value = GET_MDIO();  
     return value;
 }
 
 static void mdio_bb_send_num(uint32_t value ,uint32_t bits)
 {
     int i;
     MDIO_OUT();
     
     for(i = bits - 1; i >= 0; i--)
         mdio_bb_send_bit((value >> i) & 1);
 }
 

 static int mdio_bb_get_num(uint32_t bits)
 {
     int i;
     int ret = 0;
     for(i = bits - 1; i >= 0; i--)
     {
         ret <<= 1;
         ret |= mdio_bb_get_bit(); 
     }
 
     return ret;
 }
 
 
 
 /*  Utility to send the preamble, address, and
 *   register (common to read and write). 
 */
 static void mdio_bb_cmd(uint32_t op,uint32_t phy,uint32_t reg)
 {
     int i = 0 ;
     MDIO_OUT();
 
     for(i = 0; i < 32; i++)
         mdio_bb_send_bit(1);
 
     mdio_bb_send_bit(0);
     mdio_bb_send_bit(1);
     

     mdio_bb_send_bit((op >> 1) & 1);
     mdio_bb_send_bit((op >> 0) & 1);
 
     mdio_bb_send_num(phy,5);
     mdio_bb_send_num(reg,5);
 
 }

 void mdio_set_turnaround(void)
 {
     MDIO_IN();
     phy_delay_5us();
     MDC_L();
     phy_delay_5us();
     MDC_H();
 }
 
 static uint32_t phy_read(uint32_t phy,uint32_t reg)
 {
     uint32_t ret,i;
 
     mdio_bb_cmd(MDIO_READ,phy,reg);
     //Delay10us(1);
     MDIO_IN();
     //mdio_set_turnaround();
     /*  check the turnaround bit: the PHY should be driving it to zero */ 
     if(mdio_bb_get_bit() != 0)
    {
         /* PHY didn't driver TA low -- flush any bits it may be trying to send*/
         for(i = 0; i < 32; i++)
             mdio_bb_get_bit();
         return 0xFFFF;
     }
     ret = mdio_bb_get_num(16);
     mdio_bb_get_bit();
     return ret;
 }
 
 

 static int phy_write(uint32_t phy,uint32_t reg,uint32_t val)
 {
     mdio_bb_cmd(MDIO_WRITE,phy,reg);
     /*  send the turnaround (10) */  
     mdio_bb_send_bit(1);
     mdio_bb_send_bit(0);
     mdio_bb_send_num(val,16);
     MDIO_IN();
     return 0;
}
#if SMSC8742_SUPPORT 
void Phy874x_Read_Mmd_Register(uint32_t mmdAddress,uint32_t mmdIndex,uint32_t *pValue)
{
	/* Write Phy Reg 13 with MMD Device Address */
	phy_write(PHY_ADD,PHY_MMD_ACCESS_CTL,mmdAddress);
	/* Write Phy Reg 14 with MMD Register index */
	phy_write(PHY_ADD,PHY_MMD_ACCESS_ADDR_DATA,mmdIndex);
	/*Set Data, no post increment */
	phy_write(PHY_ADD,PHY_MMD_ACCESS_CTL,(0x4000 | mmdAddress));
	/* Read value */
	*pValue = phy_read(PHY_ADD,PHY_MMD_ACCESS_ADDR_DATA);
}


void Phy874x_Write_Mmd_Register(uint32_t mmdAddress,uint32_t mmdIndex,uint32_t value)
{
	/* Write Phy Reg 13 with MMD Device Address */
	phy_write(PHY_ADD,PHY_MMD_ACCESS_CTL,mmdAddress);
	/* Write Phy Reg 14 with MMD Register index */
	phy_write(PHY_ADD,PHY_MMD_ACCESS_ADDR_DATA,mmdIndex);
	/*Set Data, no post increment */
	phy_write(PHY_ADD,PHY_MMD_ACCESS_CTL,(0x4000 | mmdAddress));
	/* Read value */
	phy_write(PHY_ADD,PHY_MMD_ACCESS_ADDR_DATA, value);
}

int Lan874x_Clear_Wakes(void)
{
	uint32_t temp;

	/* Clear wakeup filter configuration */
	Phy874x_Read_Mmd_Register(MMD_PCS,PHY_WUF_CFGA, &temp);
	temp |= PHY_WUF_CFGA_FILTER_TRGRD_;/* Write to clear */
	temp &= ~PHY_WUF_CFGA_FILTER_EN_;
	Phy874x_Write_Mmd_Register(MMD_PCS,PHY_WUF_CFGA,temp);

	/* Configure Wakeup control and status register */
	Phy874x_Read_Mmd_Register(MMD_PCS,PHY_WUCSR, &temp);
	temp &= ~PHY_WUCSR_CFGED_;
	temp |= (PHY_WUCSR_PFDA_FR_ | PHY_WUCSR_WUFR_ | PHY_WUCSR_MPR_ |
		PHY_WUCSR_BCST_FR_); /* Write to clear */
	Phy874x_Write_Mmd_Register(MMD_PCS,PHY_WUCSR,temp);
  return 0;
}


void Lan874x_Set_Mac_Address()
{
	uint32_t temp;
	//uint32_t temp1;
        struct RegistBuffer *Regist = (struct RegistBuffer*)RegistBuffer;         
	temp = (Regist->flashRegist.mac[1] << 8) | Regist->flashRegist.mac[0];
	Phy874x_Write_Mmd_Register(MMD_PCS,PHY_RX_ADDRC,temp);
	temp = (Regist->flashRegist.mac[3] << 8) | Regist->flashRegist.mac[2];
	Phy874x_Write_Mmd_Register(MMD_PCS,PHY_RX_ADDRB,temp);
	temp = (Regist->flashRegist.mac[5] << 8) | Regist->flashRegist.mac[4];
	Phy874x_Write_Mmd_Register(MMD_PCS,PHY_RX_ADDRA,temp);
	//Phy874x_Read_Mmd_Register(MMD_PCS,PHY_RX_ADDRC,&temp1);

	//DEBUG("magic packet temp: 0x%04x\n",temp1);
	//Phy874x_Read_Mmd_Register(MMD_PCS,PHY_RX_ADDRB,&temp1);

	//DEBUG("magic packet temp: 0x%04x\n",temp1);
	//Phy874x_Read_Mmd_Register(MMD_PCS,PHY_RX_ADDRA,&temp1);

	//DEBUG("magic packet temp: 0x%04x\n",temp1);
}
void Lan874x_Magic_Packet_Wake_Enable(void)
{
	uint32_t temp;
	/* Configure Mac Address */
	Lan874x_Set_Mac_Address();

	/*Enable Magic packet wake in WUCSR */
	Phy874x_Read_Mmd_Register(MMD_PCS,PHY_WUCSR, &temp);
	temp |= (PHY_WUCSR_MPEN_ | PHY_WUCSR_CFGED_ | 
		(PHY_WUCSR_LED2_FUNC_nPME_ << PHY_WUCSR_LED2_FUNC_SHIFT_));
	Phy874x_Write_Mmd_Register(MMD_PCS,PHY_WUCSR,temp);

}

void Phy8742_Wol_Config(void)
{
	DEBUG("%s\n",__FUNCTION__);
	Lan874x_Clear_Wakes();
	Lan874x_Magic_Packet_Wake_Enable();	
}
#endif
#if RTL8201F_SUPPORT 
void Lan8021fn_Clear_Wakes(void)
{
	uint32_t temp;

	phy_write(PHY_ADD,31,0);

	temp = phy_read(PHY_ADD,0);
	temp |= (1 << 6);
	phy_write(PHY_ADD,0,temp);

	phy_write(PHY_ADD,31,7);
	temp = phy_read(PHY_ADD,20);
	temp &= ~(1 << 15);
	phy_write(PHY_ADD,20,temp);

	phy_write(PHY_ADD,31,17);
	temp = phy_read(PHY_ADD,19);
	temp &= ~(1 << 15);
	phy_write(PHY_ADD,19,temp);

	phy_write(PHY_ADD,31,17);
	phy_write(PHY_ADD,16,0x0);

	phy_write(PHY_ADD,31,17);	
	temp = phy_read(1,17);
	temp |= (1 << 15);
	phy_write(PHY_ADD,17,temp);

	phy_write(PHY_ADD,31,0);
}



void Lan8201fn_Set_Mac_Address()
{
	uint32_t temp;
        struct RegistBuffer *Regist = (struct RegistBuffer*)RegistBuffer;        

	phy_write(PHY_ADD,31,0x0012);//page18

	temp = (Regist->flashRegist.mac[1] << 8) | Regist->flashRegist.mac[0];
	phy_write(PHY_ADD, 16, temp);

	temp = (Regist->flashRegist.mac[3] << 8) | Regist->flashRegist.mac[2];
	phy_write(PHY_ADD, 17, temp);

	temp = (Regist->flashRegist.mac[5] << 8) | Regist->flashRegist.mac[4];
	phy_write(PHY_ADD, 18, temp);

	phy_write(PHY_ADD,31,0x0011);//page17
	phy_write(PHY_ADD,17,0x1FFF);
	temp = phy_read(1,17);
}

void Lan8021fn_Magic_Packet_Wake_Enable(void)
{
	uint32_t temp;
	/* Configure Mac Address */
	Lan8201fn_Set_Mac_Address();

	phy_write(PHY_ADD,31,17);//page17
	phy_write(PHY_ADD,16,0x1000);
        
	phy_write(PHY_ADD,31,7);  //page7
	temp = phy_read(PHY_ADD,19);
	temp |= (1 << 10);
	phy_write(PHY_ADD,19,temp);
        
	phy_write(PHY_ADD,31,0);  //page0
	temp = phy_read(PHY_ADD,0);
	temp &= ~(1 << 6);
	phy_write(PHY_ADD,0,temp);

	//MII/RMII TX lsolate
	phy_write(PHY_ADD,31,7);//page7
	temp = phy_read(PHY_ADD,20);
	temp |= (1 << 15);
	phy_write(PHY_ADD,20,temp);

	//MII/RMII RX lsolate
	phy_write(PHY_ADD,31,17);//page17
	temp = phy_read(PHY_ADD,19);
	temp |= (1 << 15);
	phy_write(PHY_ADD,19,temp);

	phy_write(PHY_ADD,31,17);//page17 //andy add for pulse low for PMEB
	temp = phy_read(PHY_ADD,19);
	temp |= 0x0111;
	phy_write(PHY_ADD,19,temp);

	phy_write(PHY_ADD,31,0); //page0
         
}

void Phy8021fn_Wol_Config(void)
{
	DEBUG("%s\n",__FUNCTION__);
	Lan8021fn_Clear_Wakes();
	Lan8021fn_Magic_Packet_Wake_Enable();	
}

#endif
#if RTL8211E_SUPPORT 
void Lan8211e_Clear_Wakes(void)
{
	//phy_write(PHY_ADD,31,0x0007);//page7
	//phy_write(PHY_ADD,30,0x006D);//select extension page 109
        //phy_write(PHY_ADD,22,0x9fff);//maximum packet length	
	//phy_write(PHY_ADD,31,0); //page0	
}


void Lan8211e_Set_Mac_Address()
{
	uint32_t temp;
        struct RegistBuffer *Regist = (struct RegistBuffer*)RegistBuffer;
	DEBUG("%s\n",__FUNCTION__);
	phy_write(PHY_ADD,31,0x0007);//page7
	phy_write(PHY_ADD,30,0x006e);//select extension page 110
	temp = (Regist->flashRegist.mac[1] << 8) | Regist->flashRegist.mac[0];
	phy_write(PHY_ADD,21,temp);	

	temp = (Regist->flashRegist.mac[3] << 8) | Regist->flashRegist.mac[2];
	phy_write(PHY_ADD,22,temp);

	temp = (Regist->flashRegist.mac[5] << 8) | Regist->flashRegist.mac[4];
	phy_write(PHY_ADD,23,temp);

	temp = phy_read(PHY_ADD,21);
	DEBUG("PHY_ADD page 7 expage 110 -> %d: 0x%04x\n",21,temp);

	temp = phy_read(PHY_ADD,22);
	DEBUG("PHY_ADD page 7 expage 110 -> %d: 0x%04x\n",22,temp);

	temp = phy_read(PHY_ADD,23);
	DEBUG("PHY_ADD page 7 expage 110 -> %d: 0x%04x\n",23,temp);

        phy_write(PHY_ADD,31,0x0000);

}
void Lan8211e_Magic_Packet_Wake_Enable(void)
{
	uint32_t tmp;
	
	//2.set maximum packet and WOL event
	phy_write(PHY_ADD,31,0x0007);
	phy_write(PHY_ADD,30,0x006d);//select extension page 109
	phy_write(PHY_ADD,22,0x1fff);//maximum packet length
	phy_write(PHY_ADD,21,0x1000);//enable WOL Wake-Up all events	
	tmp = phy_read(PHY_ADD,22);
	DEBUG("PHY_ADD page 7 expage 109 -> 22: 0x%x\n",tmp);	
	tmp = phy_read(PHY_ADD,21);
	DEBUG("PHY_ADD page 7 expage 109 -> 21: 0x%x\n",tmp);
	Lan8211e_Set_Mac_Address();	
	//4. disable GMII/RGMII pad for power saving
	phy_write(PHY_ADD,31,0x0007);
	phy_write(PHY_ADD,30,0x006d);//select extension page 109
	phy_write(PHY_ADD,25,0x0001);
	phy_write(PHY_ADD,24,0x0007);
	tmp = phy_read(PHY_ADD,25);
	DEBUG("PHY_ADD page 7 expage 109 -> 25: 0x%x\n",tmp);
	tmp = phy_read(PHY_ADD,24);
	DEBUG("PHY_ADD page 7 expage 109 -> 24: 0x%x\n",tmp);

	//5. back to page0
	phy_write(PHY_ADD,31,0); //back to page0
        tmp = phy_read(PHY_ADD,0);
}


void Phy8211e_Wol_Config(void)
{
	DEBUG("%s\n",__FUNCTION__);
	Lan8211e_Clear_Wakes();
	Lan8211e_Magic_Packet_Wake_Enable();	
}
#endif

#if RTL8211F_SUPPORT 
void Lan8211f_Clear_Wakes(void)
{
	uint32_t tmp;  
	phy_write(PHY_ADD,31,0x0d8a);
	phy_write(PHY_ADD,16,0x0000);//enable WOL Wake-Up all events
     
	tmp = phy_read(PHY_ADD,17);        
        tmp &= ~(1 << 5);
        phy_write(PHY_ADD,17,tmp);         
	phy_write(PHY_ADD,31,0);        
}


void Lan8211F_Set_Mac_Address()
{
	uint32_t temp;
        struct RegistBuffer *Regist = (struct RegistBuffer*)RegistBuffer;
	DEBUG("%s\n",__FUNCTION__);
	phy_write(PHY_ADD,31,0x0d8c);//0D8C
	temp = (Regist->flashRegist.mac[1] << 8) | Regist->flashRegist.mac[0];
	phy_write(PHY_ADD,16,temp);	

	temp = (Regist->flashRegist.mac[3] << 8) | Regist->flashRegist.mac[2];
	phy_write(PHY_ADD,17,temp);

	temp = (Regist->flashRegist.mac[5] << 8) | Regist->flashRegist.mac[4];
	phy_write(PHY_ADD,18,temp);

	temp = phy_read(PHY_ADD,16);
	DEBUG("PHY_ADD page 0D8C -> %d: 0x%04x\n",16,temp);

	temp = phy_read(PHY_ADD,17);
	DEBUG("PHY_ADD page 0D8C -> %d: 0x%04x\n",17,temp);

	temp = phy_read(PHY_ADD,18);
	DEBUG("PHY_ADD page 0D8C -> %d: 0x%04x\n",18,temp);

        phy_write(PHY_ADD,31,0x0000);

}
void Lan8211f_Magic_Packet_Wake_Enable(void)
{
	uint32_t tmp;
	phy_write(PHY_ADD,31,0x0d40);        
	tmp = phy_read(PHY_ADD,22);        
        tmp |= (1 << 5);
        phy_write(PHY_ADD,22,tmp); 
	tmp = phy_read(PHY_ADD,22);          
	DEBUG("PHY_ADD page 0d40 -> 22: 0x%x\n",tmp);     
        
	Lan8211F_Set_Mac_Address();
	//2.set maximum packet and WOL event
	phy_write(PHY_ADD,31,0x0d8a);
	phy_write(PHY_ADD,17,0x9fff);//maximum packet length
	phy_write(PHY_ADD,16,0x1000);//enable WOL Wake-Up all events	
	tmp = phy_read(PHY_ADD,17);
	DEBUG("PHY_ADD page 7 -> 22: 0x%x\n",tmp);	
	tmp = phy_read(PHY_ADD,16);
	DEBUG("PHY_ADD page 0D8A -> 21: 0x%x\n",tmp);
	//4. disable GMII/RGMII pad for power saving
	tmp = phy_read(PHY_ADD,19);        
        tmp |= (1 << 15);
        phy_write(PHY_ADD,19,tmp);
        
	tmp = phy_read(PHY_ADD,19);
	DEBUG("PHY_ADD page 0D8A -> 19: 0x%x\n",tmp);
	//5. back to page0
	phy_write(PHY_ADD,31,0); //back to page0
        
	phy_write(PHY_ADD, 31, 0x0d04);
	phy_write(PHY_ADD, 16, 0x205B);			//LED mode A
	phy_write(PHY_ADD, 17, 0x0);			// LED blink slow
	phy_write(PHY_ADD,31,0); //page0        
}
void Phy8211f_Wol_Config(void)
{
	DEBUG("%s\n",__FUNCTION__);
	Lan8211f_Clear_Wakes();
	Lan8211f_Magic_Packet_Wake_Enable(); 
}

void Read_Mmd_Register(uint32_t phyAddr,uint16_t mmdAddress,uint16_t mmdIndex,uint16_t *pValue)
{
	/* Write Phy Reg 13 with MMD Device Address */
	phy_write(phyAddr,PHY_MMD_ACCESS_CTL,mmdAddress);
	/* Write Phy Reg 14 with MMD Register index */
	phy_write(phyAddr,PHY_MMD_ACCESS_ADDR_DATA,mmdIndex);
	/*Set Data, no post increment */
	phy_write(phyAddr,PHY_MMD_ACCESS_CTL,(0x4000 | mmdAddress));
	/* Read value */
	*pValue = phy_read(phyAddr,PHY_MMD_ACCESS_ADDR_DATA);
}

void Write_Mmd_Register(uint32_t phyAddr,uint16_t mmdAddress,uint16_t mmdIndex,uint16_t value)
{
	/* Write Phy Reg 13 with MMD Device Address */
	phy_write(phyAddr,PHY_MMD_ACCESS_CTL,mmdAddress);
	/* Write Phy Reg 14 with MMD Register index */
	phy_write(phyAddr,PHY_MMD_ACCESS_ADDR_DATA,mmdIndex);
	/*Set Data, no post increment */
	phy_write(phyAddr,PHY_MMD_ACCESS_CTL,(0x4000 | mmdAddress));
	/* Read value */
	phy_write(phyAddr,PHY_MMD_ACCESS_ADDR_DATA, value);
}

void rtl_Set_Mac_Address(uint32_t phyAddr)
{
	uint16_t temp;
	struct RegistBuffer *Regist = (struct RegistBuffer*)RegistBuffer;
	
	temp = (Regist->flashRegist.mac[1] << 8) | Regist->flashRegist.mac[0];
	Write_Mmd_Register(phyAddr,31,0xD8C0,temp);

	temp = (Regist->flashRegist.mac[3] << 8) | Regist->flashRegist.mac[2];
	Write_Mmd_Register(phyAddr,31,0xD8C2,temp);

	temp = (Regist->flashRegist.mac[5] << 8) | Regist->flashRegist.mac[4];
	Write_Mmd_Register(phyAddr,31,0xD8C4,temp);
	 /* set max package length */
	Write_Mmd_Register(phyAddr,31,0xD8A2,0x9FFF);
}

void Phy8261c_Wol_Config(void)
{
	uint16_t temp;
	/*set INTB/PMEB as PMEB */
	Read_Mmd_Register(PHY_ADD,31,0xD05C, &temp);
	DEBUG("PHY_ADD:%x REG:31 0xD05C value:%x\n",PHY_ADD,temp);
	temp |= 0x0001;
	Write_Mmd_Register(PHY_ADD,31,0xD05C,temp);

	/* Configure Mac Address */
	rtl_Set_Mac_Address(PHY_ADD);

	Read_Mmd_Register(PHY_ADD,31,0xD8A0, &temp);
	DEBUG("PHY_ADD:%x REG:31 0xD8A0 value:%x\n",PHY_ADD,temp);
	temp |= 0x1000; /* enable magic event */
	Write_Mmd_Register(PHY_ADD,31,0xD8A0,temp);

	Read_Mmd_Register(PHY_ADD,31,0xD8A0, &temp);
	DEBUG("PHY_ADD:%x REG:31 0xD8A0 value:%x\n",PHY_ADD,temp);
	DEBUG("wol\n");
}

#endif
#if JL1111_SUPPORT 
void LanJL1111_Clear_Wakes(void)
{
	uint32_t tmp;  
	phy_write(PHY_ADD,31,129);  
	tmp = phy_read(PHY_ADD,28);        
    tmp |= (1 << 1);
    phy_write(PHY_ADD,28,tmp);  
	tmp = phy_read(PHY_ADD,28);        
    tmp &= ~(1 << 1);
    phy_write(PHY_ADD,28,tmp); 	
	phy_write(PHY_ADD,31,0);        
}

void JL1111_Set_Mac_Address()
{
 uint32_t temp;
    struct RegistBuffer *Regist = (struct RegistBuffer*)RegistBuffer;
	DEBUG("%s\n",__FUNCTION__);
 phy_write(PHY_ADD,31,129);//0D8C

 uint32_t m0, m1;
 uint32_t m2, m3;
 uint32_t m4, m5;
 uint32_t m6, m7;
 uint32_t m8, m9;
 uint32_t m10, m11;

 uint32_t a0, a1, a2;

 m0 = Regist->flashRegist.mac[0] & 0xf;
 m1 = (Regist->flashRegist.mac[0] >> 4) & 0xf;

 m2 = Regist->flashRegist.mac[1] & 0xf;
 m3 = (Regist->flashRegist.mac[1] >> 4) & 0xf;

 m4 = Regist->flashRegist.mac[2] & 0xf;
 m5 = (Regist->flashRegist.mac[2] >> 4) & 0xf;

 m6 = Regist->flashRegist.mac[3] & 0xf;
 m7 = (Regist->flashRegist.mac[3] >> 4) & 0xf;

 m8 = Regist->flashRegist.mac[4] & 0xf;
 m9 = (Regist->flashRegist.mac[4] >> 4) & 0xf;

 m10 = Regist->flashRegist.mac[5] & 0xf;
 m11 = (Regist->flashRegist.mac[5] >> 4) & 0xf;

 a0 = (((m0 << 4) | (m1)) << 8) | ((m2 << 4) | m3);
 a1 = (((m4 << 4) | (m5)) << 8) | ((m6 << 4) | m7);
 a2 = (((m8 << 4) | (m9)) << 8) | ((m10 << 4) | m11);

 phy_write(PHY_ADD,25, a0); 

 phy_write(PHY_ADD,26, a1);

 phy_write(PHY_ADD,27, a2);

 temp = phy_read(PHY_ADD,25);
 DEBUG("PHY_ADD page 129 -> %d: 0x%04x\n",25,temp);

 temp = phy_read(PHY_ADD,26);
 DEBUG("PHY_ADD page 129 -> %d: 0x%04x\n",26,temp);

 temp = phy_read(PHY_ADD,27);
 DEBUG("PHY_ADD page 129 -> %d: 0x%04x\n",27,temp);

    phy_write(PHY_ADD,31,0x0000);

}
void LanJL1111_Magic_Packet_Wake_Enable(void)
{
	uint32_t tmp;
	phy_write(PHY_ADD,31,7);        
	tmp = phy_read(PHY_ADD,16);
    tmp &= ~(1 << 12);	
    tmp |= (1 << 3);
    phy_write(PHY_ADD,16,tmp); 
	tmp = phy_read(PHY_ADD,16);          
	DEBUG("PHY_ADD page 7 -> 16: 0x%x\n",tmp);     
        
	JL1111_Set_Mac_Address();
	//2.set maximum packet and WOL event

	phy_write(PHY_ADD,31,129);
	tmp = phy_read(PHY_ADD,28);
    tmp &= ~(1 << 15);	
    phy_write(PHY_ADD,28,tmp); 


	phy_write(PHY_ADD,31,128);
	tmp = phy_read(PHY_ADD,29);
    tmp |= (1 << 13);	
    phy_write(PHY_ADD,29,tmp); 

	phy_write(PHY_ADD,31,0); //page0        
}

void PhyJL1111_Wol_Config(void)
{
	DEBUG("%s\n",__FUNCTION__);
	LanJL1111_Clear_Wakes();
	LanJL1111_Magic_Packet_Wake_Enable(); 
}

#endif

#if JL2121_SUPPORT 

void JL2121_Set_Mac_Address()
{
 uint32_t temp;
    struct RegistBuffer *Regist = (struct RegistBuffer*)RegistBuffer;
	DEBUG("%s\n",__FUNCTION__);
 phy_write(PHY_ADD,31,4608);//0D8C


 temp = (Regist->flashRegist.mac[0] << 8) | Regist->flashRegist.mac[1];
 phy_write(PHY_ADD,17, temp); 
 temp = (Regist->flashRegist.mac[2] << 8) | Regist->flashRegist.mac[3];
 phy_write(PHY_ADD,18, temp);
 temp = (Regist->flashRegist.mac[4] << 8) | Regist->flashRegist.mac[5];
 phy_write(PHY_ADD,19, temp);

 temp = phy_read(PHY_ADD,17);
 DEBUG("PHY_ADD page 4608 -> %d: 0x%04x\n",17,temp);

 temp = phy_read(PHY_ADD,18);
 DEBUG("PHY_ADD page 4608 -> %d: 0x%04x\n",18,temp);

 temp = phy_read(PHY_ADD,19);
 DEBUG("PHY_ADD page 4608 -> %d: 0x%04x\n",19,temp);

    phy_write(PHY_ADD,31,0x0000);

}
void LanJL2121_Magic_Packet_Wake_Enable(void)
{
	uint32_t tmp;
	phy_write(PHY_ADD,31,18);        
	tmp = phy_read(PHY_ADD,21);
    tmp |= (1 << 6);
    phy_write(PHY_ADD,21,tmp); 
	tmp = phy_read(PHY_ADD,21);          
	DEBUG("PHY_ADD page 18 -> 21: 0x%x\n",tmp);   
	//software reset
 	tmp = phy_read(PHY_ADD,0);
    tmp |= (1 << 15);
    phy_write(PHY_ADD,0,tmp);      
		

	JL2121_Set_Mac_Address();
	//2.set maximum packet and WOL event

	phy_write(PHY_ADD,31,4608);
	tmp = phy_read(PHY_ADD,16);
    tmp &= ~(1 << 15);	
    tmp |= (1 << 14);	
    phy_write(PHY_ADD,16,tmp); 
	tmp = phy_read(PHY_ADD,16);          
	DEBUG("PHY_ADD page 4608 -> 16: 0x%x\n",tmp);   
	phy_write(PHY_ADD,31,0); //page0        
}

void PhyJL2121_Wol_Config(void)
{
	DEBUG("%s\n",__FUNCTION__);
	LanJL2121_Magic_Packet_Wake_Enable(); 
}

#endif

#if (YT8521S_SUPPORT || YT8531S_SUPPORT)

uint32_t yt8521s_ext_read(uint32_t Address)
{
	uint32_t value;
	phy_write(PHY_ADD,0x1e,Address);
	value = phy_read(0,0x1f);
	return value;
}
void yt8521s_ext_write(uint32_t Address,uint32_t value)
{
	phy_write(PHY_ADD,0x1e,Address);
	phy_write(PHY_ADD,0x1f,value);
}
/*
void yt8521s_Clear_Wakes(void)
{
	uint32_t tmp;  
	yt8521s_ext_write(0xa000,0);  //utp
	tmp = yt8521s_ext_read(0xa00a);
	if(tmp & 1){
		tmp = tmp & ~(1 < 3);
	}else{

		phy_read(PHY_ADD,0x13);   
		phy_read(PHY_ADD,0x13);  
	}
}
*/
void yt8521s_Set_Mac_Address()
{
	uint32_t temp;
    struct RegistBuffer *Regist = (struct RegistBuffer*)RegistBuffer;
	DEBUG("%s\n",__FUNCTION__);
	temp = (Regist->flashRegist.mac[0] << 8) | Regist->flashRegist.mac[1];
	yt8521s_ext_write(0xa007,temp);	

	temp = (Regist->flashRegist.mac[2] << 8) | Regist->flashRegist.mac[3];
	yt8521s_ext_write(0xa008,temp);	

	temp = (Regist->flashRegist.mac[4] << 8) | Regist->flashRegist.mac[5];
	yt8521s_ext_write(0xa009,temp);	

	temp = yt8521s_ext_read(0xa007);
	DEBUG("yt8521s MAC -> 0x%04x\n",temp);

	temp = yt8521s_ext_read(0xa008);
	DEBUG("yt8521s MAC -> 0x%04x\n",temp);

	temp = yt8521s_ext_read(0xa009);
	DEBUG("yt8521s MAC -> 0x%04x\n",temp);

}

void yt8521s_Magic_Packet_Wake_Enable(void)
{
	uint32_t tmp;    
	tmp = yt8521s_ext_read(0xa00a);        
    tmp |= (1 << 6);
    tmp |= (1 << 3);
    tmp &= ~(3 << 4);
    tmp |= (2 << 4);
    yt8521s_ext_write(0xa00a,tmp); 
	tmp = yt8521s_ext_read(0xa00a);          
	DEBUG("yt8521s 0xa00a: 0x%x\n",tmp);  		
	yt8521s_Set_Mac_Address();
}
void yt8521s_Wol_Config(void)
{
	DEBUG("%s\n",__FUNCTION__);
	//yt8521s_Clear_Wakes();
	yt8521s_Magic_Packet_Wake_Enable(); 
}
#endif

uint32_t ReadPhyID(void)
{
	uint32_t tmp;	
	tmp = phy_read(0x00,0x02);
	tmp = tmp << 16;
	tmp |= phy_read(0x00,0x03);
        if(tmp != 0x001cc916){
          tmp= (tmp&0xfffffff0);
        }
	DEBUG("--phy id--:0x%08x\n",tmp);
        return tmp;
}

void phy_bus_init(void)
{
    SYS->P4_MFP &= ~(SYS_MFP_P46_Msk | SYS_MFP_P47_Msk);
    SYS->P4_MFP |= (SYS_MFP_P46_GPIO | SYS_MFP_P47_GPIO);	
	
    MDIO_IN();
    MDC_OUT();
}

void phy_bus_deinit(void)
{
    SYS->P4_MFP &= ~(SYS_MFP_P46_Msk | SYS_MFP_P47_Msk);
    SYS->P4_MFP |= (SYS_MFP_P46_ICE_CLK | SYS_MFP_P47_ICE_DAT);	
}

void detect_phy(void)
{
	phy_bus_init();

	mPhyData.phy_id = ReadPhyID();
	
	phy_bus_deinit();
}

#ifdef ENABLE_WOL

uint8_t wol_int_flag(void)
{
	if(mPhyData.phy_id == JL1111_ID){
		return 1;
	}
	return 0;
}

int is_phy_id_valid(void)
{
	uint8_t i;
	
	if(mPhyData.phy_id == 0)
		return 0;
	for(i = 0 ; i < PHY_NUM; i++){
		if(mPhyData.phyInit[i].id == mPhyData.phy_id)
			return 1;
	}
	return 0;
}

int config_wol(void){
       uint8_t i;
	
	     DEBUG("config_wol\n");
       HOST_WOL_EN_OUT = 0;
       Delay100ms(2);
       HOST_WOL_EN_OUT = 1;       
       Delay10ms(300);
       phy_bus_init();
			DEBUG("mPhyData.phy_id = %x\n",mPhyData.phy_id);
       if(is_phy_id_valid() != 1){
          Delay100ms(1);
          mPhyData.phy_id = ReadPhyID();
       }
       for(i = 0 ; i < PHY_NUM; i++){
			if(mPhyData.phyInit[i].id == 0xffffffff){
				break;
			}
			if(mPhyData.phyInit[i].id == mPhyData.phy_id){
               if(mPhyData.phyInit[i].init){
                   mPhyData.phyInit[i].init();
                   return 0;
               }
			}
       }  
       phy_bus_deinit();
       return 0;
}

void wol_init(void)
{ 	
    WolTransItem.trans = &WolCallback;
    RegistTransBufferCallback(&WolTransItem);  
    HOST_WOL_EN_OUT = 1;

	
    struct RegistBuffer *Regist = (struct RegistBuffer*)RegistBuffer;
		DEBUG("wol_init\n");
    if(Regist->flashRegist.control.bits.wol_wake_en != 1){
        HOST_WOL_EN_OUT = 0;
    }else {
      if(Regist->flashRegist.control.bits.power_loss != 1){
				HOST_POWER_ENABLE = 1;
				Delay100ms(10);
				if(HOST_PWR_IN){
					if(config_wol() == 0 || Regist->flashRegist.control.bits.wol_wake_en == 1){
           ENABLED_WOL_INTERRUPT(0,wol_int_flag());
					}
				}
      }
    }
}

#else

uint8_t inline wol_int_flag(void) {
	return 0;
}

int inline config_wol(void) {
	return 0;
}

void wol_init(void) {
	WolTransItem.trans = &WolCallback;
	RegistTransBufferCallback(&WolTransItem);  
	HOST_WOL_EN_OUT = 0;

	struct RegistBuffer *Regist = (struct RegistBuffer*)RegistBuffer;
	if(Regist->flashRegist.control.bits.wol_wake_en != 1){
		HOST_WOL_EN_OUT = 1;
	} else {
		if(Regist->flashRegist.control.bits.power_loss != 1){
			if(config_wol() == 0 || Regist->flashRegist.control.bits.wol_wake_en == 1){
				ENABLED_WOL_INTERRUPT(0, wol_int_flag());
			}
		}
	}
}

#endif




