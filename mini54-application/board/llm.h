#ifndef __LLM_H__
#define __LLM_H__

#ifdef __cplusplus
extern "C"
{
#endif
  
#include <stdio.h>
#include "Mini51Series.h"

extern struct LLM_MODULE* gllm_module[6]; 
  
extern struct LLM_MODULE llm1_module;
extern struct LLM_MODULE llm2_module;
extern struct LLM_MODULE llm3_module;
extern struct LLM_MODULE llm4_module;
extern struct LLM_MODULE llm5_module;
extern struct LLM_MODULE llm6_module;

//状态		温度		风扇占空比
//1			50			30%
//2			60			45%
//3			70			60%
//4			80			100%

struct FAN_POLICY
{
	uint8_t	policy;
	uint8_t temperature;
	uint8_t duty;
};

typedef enum
{
	LLM_POWER_DOWN = 1,
        LLM_POWER_ON_FAILED = 2,
        LLM_POWER_OFF_FAILED = 3,
        LLM_POWER_ON = 4
} LLM_STATE;


typedef enum
{
    LLM_MODULE1	= 1,
    LLM_MODULE2	= 2,
    LLM_MODULE3	= 3,
    LLM_MODULE4	= 4,
    LLM_MODULE5	= 5,
    LLM_MODULE6	= 6
      
} MODEL_ID;

struct LLM_MODULE
{
	uint8_t id;  
	uint8_t status;
        uint8_t errom_addr;
	uint8_t am_here;
	void * brother;
	uint8_t fan_id;
	uint8_t cur_fan_policy;
        uint32_t pwm_channel;
        uint8_t have_reset;
        uint8_t report_status;        
	uint8_t (*get_fan_policy)(void *data);
	uint16_t (*get_fan_speed)(void *data);
	uint8_t (*process_powerbutton)(void *data);
	uint8_t (*process_resetbutton)(void *data);        
        
	

};
void smart_llm_init(void);
#endif