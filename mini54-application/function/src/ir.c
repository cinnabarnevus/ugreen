
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
#include "ir.h"
   
#define IR_TIMER	TIMER1
struct TransBufferItem IrTransItem;    
   
struct Ir_data mIrdata ;
struct ir_button mIrUserButton;

uint8_t i=0,j,ii=0,ij=0;

static void InitIrData(void){
	mIrdata.ir_state = IR_PRELOAD ;
	mIrdata.TimerCounter1 = 0;
	mIrdata.TimerPre = 0;
	mIrdata.TimerCur = 0;
	mIrdata.ir_periodTime = 0;
	mIrdata.ir_ScanCode = 0;
	mIrdata.ir_Count = 0;
	mIrdata.ir_keybdNum = 0;
	mIrdata.ir_Press = 0;
	mIrdata.ir_prepress = 0;
	mIrdata.ir_user_key = 0;
}

static void InitIrUserButton(void){
    struct RegistBuffer *Regist = (struct RegistBuffer*)RegistBuffer;  
	mIrUserButton.usercode = (uint16_t)Regist->flashRegist.ir_ucode_m << 8 | (uint16_t)Regist->flashRegist.ir_ucode_l;
	mIrUserButton.poweKey = Regist->flashRegist.ir_pwr_key;
}

struct TransBufferCallBack IrCallback = {
    	.name		= "Ir trans callback",
        .callback         = IrTransCallback,
};
   

int IrTransCallback(uint8_t reg,uint8_t value) {
    int ret = -1;
    struct RegistBuffer *Regist = (struct RegistBuffer*)RegistBuffer;
    switch(reg){
      case MCU_IR_WAKE:
          if(Regist->flashRegist.IrWake != value){
              Regist->flashRegist.IrWake = value;
              FlashRegistUpdate();
          } 
          ret = 0;          
          break;
      case MCU_IR_UCODE_M:
          if(Regist->flashRegist.ir_ucode_m != value){
              Regist->flashRegist.ir_ucode_m = value;
			  InitIrUserButton();
              FlashRegistUpdate();
          } 
          ret = 0;          
          break;
      case MCU_IR_UCODE_L:
          if(Regist->flashRegist.ir_ucode_l != value){
              Regist->flashRegist.ir_ucode_l = value;
			  InitIrUserButton();
              FlashRegistUpdate();
          } 
          ret = 0;          
          break;
      case MCU_IR_PWR_KEY:
          if(Regist->flashRegist.ir_pwr_key != value){
              Regist->flashRegist.ir_pwr_key = value;
			  InitIrUserButton();
              FlashRegistUpdate();
          } 
          ret = 0;          
          break;
      default:
        break;
    }
    return ret;
}

//Timer0

void TMR0_IRQHandler(void)
{
	IR_TIMER->TCMPR = 0xFFFFFF; 
}

void Timer_Reset(void){

	IR_TIMER->TCSR = IR_TIMER->TCSR |TIMER_TCSR_CRST_Msk;
}

void Timer_Restart(void){
	TIMER_Stop(IR_TIMER);
	Timer_Reset();
	IR_TIMER->TCSR = IR_TIMER->TCSR | TIMER_TCSR_CEN_Msk ;
}


void Ir_Timer_Init(void){

	IR_TIMER->TCMPR = 0xFFFFFF;
    IR_TIMER->TCSR =  TIMER_TCSR_IE_Msk | TIMER_CONTINUOUS_MODE | (0x7f);  //128 frequency  and pulse 10us
	TIMER_Start(IR_TIMER);
}


uint32_t Timer_GetCounter(void)
{
	return TIMER_GetCounter(TIMER1);
}

static uint16_t Ir_usercode_lookup(uint32_t IR_ScanCode)
{	
	
    if (mIrUserButton.usercode == (IR_ScanCode&0xFFFF)){			
        mIrdata.ir_user_key = 1;
        return 1;
    }
    return 0;
}

static uint16_t Ir_keybdNum_lookup(uint32_t IR_ScanCode)
{	
    int i;	

    for (i = 0; i < sizeof(mIrbutton)/sizeof(struct ir_button); i++){		
        if (mIrbutton[i].usercode == (IR_ScanCode&0xFFFF)){			
            mIrdata.ir_keybdNum = i;
            return 1;
        }
    }
	return Ir_usercode_lookup(IR_ScanCode);
}

static uint16_t Ir_user_keycode_lookup(uint32_t IR_ScanCode)
{	
    unsigned char keyData = ((IR_ScanCode >> 8) & 0xff);
	if(mIrdata.ir_user_key) {
		if (mIrUserButton.poweKey == keyData){
			mIrdata.ir_user_key = 0;
			return 1;
		}
	}
    return 0;
}


static uint16_t Ir_keycode_lookup(uint32_t IR_ScanCode)
{	
    unsigned char keyData = ((IR_ScanCode >> 8) & 0xff);
    if (mIrbutton[mIrdata.ir_keybdNum].poweKey == keyData){			
        return 1;
    }
    return Ir_user_keycode_lookup(IR_ScanCode);
}

 void IrServiceCore(void)
{
	 mIrdata.TimerPre = mIrdata.TimerCur ;
	 mIrdata.TimerCur = Timer_GetCounter();
 
	 if(mIrdata.TimerCur > mIrdata.TimerPre){
		 mIrdata.ir_periodTime = (mIrdata.TimerCur - mIrdata.TimerPre)*10;
	 }
	 else {
		 mIrdata.ir_periodTime = (0xffffff +(  mIrdata.TimerCur -mIrdata.TimerPre))*10 ; //0xffffff
	 }
	 
	 mIrdata.TimerPre = mIrdata.TimerCur ;
	 switch (mIrdata.ir_state)
      {				
        case IR_PRELOAD:
          {
              if ((IR_TIME_PRE_MIN < mIrdata.ir_periodTime) && (mIrdata.ir_periodTime < IR_TIME_PRE_MAX)){     
                  mIrdata.ir_ScanCode = 0;
                  mIrdata.ir_Count = 0;
                  mIrdata.ir_state = IR_USERCODE;		
              }else{
  
                  mIrdata.ir_state = IR_PRELOAD;
              }
              mIrdata.TimerPre = mIrdata.TimerCur;				
          }
          break;
				
        case IR_USERCODE:
        {
             mIrdata.ir_ScanCode <<= 1;
             mIrdata.ir_Count ++;

             if ((IR_TIME_BIT1_MIN < mIrdata.ir_periodTime) && (mIrdata.ir_periodTime < IR_TIME_BIT1_MAX)){
                 mIrdata.ir_ScanCode |= 0x01;
             }

              if (mIrdata.ir_Count == 0x10){//16 bit user code
                  if (Ir_keybdNum_lookup(mIrdata.ir_ScanCode)){
                      mIrdata.ir_state = IR_GETDATA;
                      mIrdata.ir_ScanCode = 0;
                      mIrdata.ir_Count = 0;
                  }else{                //user code error
                      mIrdata.ir_state = IR_PRELOAD;
                  }
              }
          }
          break;
			
		 case IR_GETDATA:
         {
              mIrdata.ir_Count ++;
              mIrdata.ir_ScanCode <<= 1;

          
              if ((IR_TIME_BIT1_MIN < mIrdata.ir_periodTime) && (mIrdata.ir_periodTime < IR_TIME_BIT1_MAX)){
                  mIrdata.ir_ScanCode |= 0x01;
              }           
              if (mIrdata.ir_Count == 0x10){

					mIrdata.ir_ScanCode = (mIrdata.ir_ScanCode&0x0ff) << 8;
					mIrdata.ir_ScanCode |= ((~mIrdata.ir_ScanCode >> 8)&0x0ff);
					if ((mIrdata.ir_ScanCode&0x0ff) == ((~mIrdata.ir_ScanCode >> 8)&0x0ff)){

                      if (Ir_keycode_lookup(mIrdata.ir_ScanCode)){
                          mIrdata.ir_Press = 1;
						  ir_Operation();

                          mIrdata.ir_state = IR_SEQUENCE;
                      }else{
                          mIrdata.ir_state = IR_PRELOAD;
                      }
					}else{
                      mIrdata.ir_state = IR_PRELOAD;
					}
              }
          }
          break;
 	            
          case IR_SEQUENCE:{
              if ((mIrdata.ir_periodTime > IR_TIME_RPT_MIN) && (mIrdata.ir_periodTime < IR_TIME_RPT_MAX)){
            		 Timer_Restart();
								  mIrdata.ir_state = IR_PRELOAD;
              }else if ((IR_TIME_SEQ1_MIN < mIrdata.ir_periodTime) && (mIrdata.ir_periodTime < IR_TIME_SEQ1_MAX)){
								 mIrdata.ir_state = IR_PRELOAD;
								 Timer_Restart();
              }else if ((IR_TIME_SEQ2_MIN < mIrdata.ir_periodTime) && (mIrdata.ir_periodTime < IR_TIME_SEQ2_MAX)){
								 mIrdata.ir_state = IR_PRELOAD;
            		 Timer_Restart();
              }else{
                	 mIrdata.ir_state = IR_PRELOAD;
                	 mIrdata.ir_Press = 0;
              }
          }
          break;
       
          default:
              break;	
			
	 	}
 }	 

void ir_init(void)
{ 	
    struct RegistBuffer *Regist = (struct RegistBuffer*)RegistBuffer;  
    IrTransItem.trans = &IrCallback;
    RegistTransBufferCallback(&IrTransItem);  
	Ir_Timer_Init();
	InitIrData();
	InitIrUserButton();
	if(Regist->flashRegist.IrWake != 0){
		ENABLED_IR_INTERRUPT(0);
	}

}










