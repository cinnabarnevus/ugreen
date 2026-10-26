#ifndef __IR_H__
#define __IR_H__
#include <stdio.h>
#include "Mini51Series.h"

#define IR_TIME_BIT0_MIN  1000  /*Bit0  1.125ms*/  //1060-1070
#define IR_TIME_BIT0_MAX  1100

#define IR_TIME_BIT1_MIN  2000 /*Bit1  2.25ms*/   //2120-2130
#define IR_TIME_BIT1_MAX  2200

//#define IR_TIME_PRE_MIN   6500   /*4500*/
//#define IR_TIME_PRE_MAX   7000   /*5500*/           /*PreLoad 4.5+0.56 = 5.06ms*/

#define IR_TIME_PRE_MIN   12600   /*4500*/
#define IR_TIME_PRE_MAX   13600  /*5500*/           /*PreLoad 9+4.5+0.56 = 13.06ms*/


#define IR_TIME_RPT_MIN   47500   /*101000*/
#define IR_TIME_RPT_MAX   49000   /*103000*/         /*Repeat  105-2.81=102.19ms*/  //110-9-2.25-0.56=98.19ms

#define IR_TIME_SEQ1_MIN   5000   /*2650*/
#define IR_TIME_SEQ1_MAX   6000   /*3000*/           /*sequence  2.25+0.56=2.81ms*/ //11.25ms

#define IR_TIME_SEQ2_MIN   20000   /*101000*/
#define IR_TIME_SEQ2_MAX   23500   /*103000*/         /*Repeat  105-2.81=102.19ms*/  //110-9-2.25-0.56=98.19ms     
 

extern void Mini52_Irda_interrupt_init(void);
extern void GetIrDaCore(void);

struct Ir_data{
	uint8_t   ir_state;
	uint8_t   ir_Count;
	uint8_t   ir_keybdNum;
	uint8_t   ir_Press;
	uint8_t   ir_prepress;
	uint8_t   ir_user_key;
	uint32_t  TimerCounter1;
	uint32_t  TimerPre;
	uint32_t  TimerCur;
	uint32_t  ir_periodTime;
	uint32_t  ir_ScanCode;

};

typedef enum _IR_STATE
{
  //  ARM_RMC_IDLE,
    IR_PRELOAD,
    IR_USERCODE,
    IR_GETDATA,
    IR_SEQUENCE
}IR_STATE;    


struct ir_button {	
    uint16_t	usercode;
    uint8_t		poweKey;
};


static struct ir_button mIrbutton[] = {
		{0x807F,0xe7},	
};


void ir_init(void);
 void IrServiceCore(void);
int IrTransCallback(uint8_t reg,uint8_t value);
#endif