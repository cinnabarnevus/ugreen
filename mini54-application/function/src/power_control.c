/*
 * Power control for Nvidia board.
 *
 * VIN_PWR_ON_MINI54:	Control the power of Core Board.
 * NV_POWER_EN_MINI54:	Control the power of PMIC.
 *
 */

#include "board.h"
#include "pm.h"
#include <stdio.h>

#define DEBUG_NV_POWER_CONTROL	1

/* Pending request for nvidia power control */
uint32_t pending_power_request = 0;
/* Power on times */
static uint8_t power_on_times = 0;
uint8_t get_poweron_times()
{
	return power_on_times;
}

/* GPIO MAP */
#define PWR_BTN_N		PWR_BUTTON_IN
#define PWR_GOOD		HOST_PWR_IN

#define FORCE_SHUTDOWN_N	FORCE_OFF_MINI54
#define VIN_PWR_ON		VIN_PWR_ON_MINI54
#define PMIC_EN0		NV_POWER_EN_MINI54


#define TIMER1_POWER_ON_PMIC		80	//TIMER1_80MS
#define TIMER1_POWER_ON_WAIT_PWR_GOOD	200	//TIMER1_200MS
#define TIMER1_POWER_OFF_VIN		10	//TIMER1_10MS
#define TIMER1_RESET			100	//TIMER1_100MS
#define TIMER1_LEAK			200	//TIMER1_200MS


#define APP_VERSION_INFO "\r\nFW VER: 0.11\r\n"

// Timer 0: PWR_BTN_N long press
// Timer 1: delays while powering on/off
// Timer 2: debouncing

// Functions prototype
void init_power(void);
void PowerOn(void);
void PoweringOn(void);
void PoweringOff(void);
void PowerOff(void);
void Reset(void);

typedef enum
{
    NONE = 0,
    INIT,
    POWERING_ON_PMIC,
    POWERING_ON_WAIT_FOR_PWR_GOOD,
    POWERING_OFF_VIN_PWR_ON,
    POWERING_OFF_LEAK,
    RESET_IGNORE_PWR_GOOD,
    POWER_EVENT_ERROR
} power_event_t;


volatile power_event_t power_event = NONE;
static uint8_t save_ie_ea;
uint8_t power_off_state = 1; // Flag to optimize checking the power off state

static char *uart_app_version_p = APP_VERSION_INFO;
void (*run_power_state)(void);
void power_handler(void);

void enter_power_state(void (*funcptr)(void))
{
	run_power_state = funcptr;
	power_event = INIT;
}

void init_power(void)
{
#if DEBUG_NV_POWER_CONTROL
	printf( APP_VERSION_INFO );
#endif
	enter_power_state(PowerOff);
}

#if DEBUG_NV_POWER_CONTROL
void handle_event(void)
{
	switch(power_event) {
		case NONE:
			break;
		case INIT:
			printf("STATE: ");
			if (run_power_state == PowerOn)
				printf("PowerOn\n");
			if (run_power_state == PoweringOn)
				printf("PoweringOn\n");
			if (run_power_state == PoweringOff)
				printf("PoweringOff\n");
			if (run_power_state == PowerOff)
				printf("PowerOff\n");
			if (run_power_state == Reset)
				printf("Reset\n");
			break;
		case POWERING_ON_PMIC:
			break;
		case POWERING_ON_WAIT_FOR_PWR_GOOD:
			printf("EVENT: POWERING_ON_WAIT_FOR_PWR_GOOD\n");
			break;
		case POWERING_OFF_VIN_PWR_ON:
			break;
		case POWERING_OFF_LEAK:
			break;
		case RESET_IGNORE_PWR_GOOD:
			printf("EVENT: RESET_IGNORE_PWR_GOOD\n");
			break;
		case POWER_EVENT_ERROR:
			printf("EVENT: POWER_EVENT_ERROR\n");
			break;
		default:
			break;
	}
}
#else 
void handle_event(void) {};
#endif


/****************************************************************/
/* Virtual timers for State machine */
/****************************************************************/

struct gd_t {
	uint32_t timer0_flag;
	uint32_t timer1_flag;   //上电、下电、复位的PWR_GOOD的延时
	uint32_t timer2_flag;
	uint32_t timer3_flag;
} gd;

struct vtimer_t {
	uint32_t time;
	uint32_t load;
	uint32_t enable;
	void (*func)(void);
};
#define VTIMER_NUM	5
static struct vtimer_t vtimer_list[VTIMER_NUM] = {0};

static int vtimer_hander_init = 0;

void timer0_handler(void) {
	gd.timer0_flag = 1;
	vtimer_list[0].enable = 0;
}

void timer1_handler(void) {
	gd.timer1_flag = 1;
	vtimer_list[1].enable = 0;
}

void timer2_handler(void) {
	gd.timer2_flag = 1;
	vtimer_list[2].enable = 0;
}

void timer3_handler(void) {
	gd.timer3_flag = 1;
	vtimer_list[3].enable = 0;
}

/*每tick调power_handler()*/
void mainloop_handler(void) {
	power_handler();
}

#define add_vtimer(n, handler, delay_ms)	\
do {						\
	vtimer_list[n].func = handler;		\
	vtimer_list[n].time = delay_ms;		\
	vtimer_list[n].load = delay_ms;		\
	vtimer_list[n].enable = 1;		\
}while(0)

#define del_vtimer(n)				\
do {						\
	vtimer_list[n].enable = 0;		\
}while(0)

void clear_flag_and_start_timer1(unsigned int delay_ms)
{
	gd.timer1_flag = 0;
	add_vtimer(1, timer1_handler, delay_ms);// start timer1
}

void vtimer_hander( void )
{
	if (vtimer_hander_init == 0) {
		vtimer_hander_init = 1;
		add_vtimer(4, mainloop_handler, 1);	// start main loop timer
	}
	
	int i = 0;
	for (i = 0; i < VTIMER_NUM; i++) {
		if (vtimer_list[i].enable == 0) {
			continue;
		}

		if (vtimer_list[i].time == 0) {
			(* vtimer_list[i].func)();
			vtimer_list[i].time = vtimer_list[i].load;
		} else {
			vtimer_list[i].time--;
		}
	}
}


/****************************************************************/
/* Power state machine */
/****************************************************************/

static uint8_t init = 0;

void power_handler(void)
{
	if (init == 0) {
		init_power();
		init = 1;
	}

	(*run_power_state)();

	/* output log */
	handle_event();
}


void PowerOff(void)
{
    static uint8_t pwr_btn_n_released = 0;
    if (power_event == INIT)
    {
        power_event = NONE; // clear power event
        power_off_state = 1; // set power off state flag
        pwr_btn_n_released = 0;
        PMIC_EN0 = 0;
        VIN_PWR_ON = 0;
        gd.timer1_flag = 0;
        pending_power_request &= (~NV_REQ_POWEROFF);// clear the pending request.
    }
    
    if (pending_power_request & NV_REQ_POWERON)// Received the request of power on
    {
        pending_power_request &= (~NV_REQ_POWERON); // clear flag when exiting power off state
        enter_power_state(PoweringOn);
    }

    if (PWR_BTN_N)
    {
        // Only if PWR_BTN_N is released, the system can be powered on again
        pwr_btn_n_released = 1;
    }

		if (pwr_btn_n_released && ((PWR_BTN_N) == 0))
    {
        enter_power_state(PoweringOn);
        power_off_state = 0; // clear flag when exiting power off state
    }
}


void PoweringOn(void)
{
		struct RegistBuffer *Regist = (struct RegistBuffer*)RegistBuffer;

    if (power_event == INIT)
    {
        power_event = NONE; // clear power event
    }

    if (power_event == NONE)
    {
			HOST_POWER_ENABLE = 1;
			  if(PWR_GOOD){
					VIN_PWR_ON = 1;
					clear_flag_and_start_timer1(TIMER1_POWER_ON_PMIC);
					power_event = POWERING_ON_PMIC;
				}
    }
    else if (power_event == POWERING_ON_PMIC)
    {
        if (gd.timer1_flag)
        {
            PMIC_EN0 = 1;
            clear_flag_and_start_timer1(TIMER1_POWER_ON_WAIT_PWR_GOOD);
            power_event = POWERING_ON_WAIT_FOR_PWR_GOOD;
        }
    }
    else if (power_event == POWERING_ON_WAIT_FOR_PWR_GOOD)
    {
				if (gd.timer1_flag)
        { // timer expired or interrupted when PWR_GOOD goes high
            gd.timer1_flag = 0;
            del_vtimer(1);	// stop timer1
						if ((PWR_GOOD == 1) && (get_host_power_status() == POWER_ON))
            {
                enter_power_state(PowerOn);
            }else{
								enter_power_state(PoweringOff);
								Regist->soc_reboot_flag = 1;
            }
        }
    }
    else
    {
        // do nothing
    }
}


void PowerOn(void)
{
    static unsigned int pwr_btn_sec_counter = 0;

    if (power_event == INIT)
    {
        power_event = NONE; // clear power event
        del_vtimer(0);
        gd.timer0_flag = 0;

        pwr_btn_sec_counter = 0;
        PMIC_EN0 = 1;
        VIN_PWR_ON = 1;
        pending_power_request &= (~NV_REQ_POWERON);// clear the pending request.
        power_on_times++;
    }

    if ((PWR_BTN_N) == 0)
    {
        if (gd.timer0_flag == 1)
        {
            pwr_btn_sec_counter++;
        }
        if (vtimer_list[0].enable == 0)
        {
            // In case other interrupts trigger this function multiple times
            gd.timer0_flag = 0;
            // start timer0
						add_vtimer(0, timer0_handler, 100);
        }
    }
    else
    {
        pwr_btn_sec_counter = 0;
				del_vtimer(0);	// stop timer0
    }

    if (pending_power_request & NV_REQ_POWEROFF)// Received the request of power off
    {
        pending_power_request &= (~NV_REQ_POWEROFF);
        enter_power_state(PoweringOff);
    }

    // 10s long press / 100ms = Timer0 expires 100 times
		if ((pwr_btn_sec_counter >= 100) || (PWR_GOOD == 0) || (FORCE_SHUTDOWN_N ==0)|| (get_host_power_status() == POWER_OFF))
    {

				DEBUG("shutdown...\n");
        // If PWR_BTN_N asserted for more than 10s, FORCE_SHUTDOWN_N is asserted or PWR_GOOD drops, power off
        enter_power_state(PoweringOff);
    }
    else
    {
        // do nothing
    }
}


void PoweringOff(void)
{
		struct RegistBuffer *Regist = (struct RegistBuffer*)RegistBuffer;
	
    if (power_event == INIT)
    {
        power_event = NONE; // clear power event
    }

    if (power_event == NONE)
    {
			if(Regist->flashRegist.control.bits.wol_wake_en != 1){
				HOST_POWER_ENABLE = 0;
				if(!PWR_GOOD){
					PMIC_EN0 = 0;
					// Set up timer for POWERING_OFF_VIN_PWR_ON
					clear_flag_and_start_timer1(TIMER1_POWER_OFF_VIN);
					power_event = POWERING_OFF_VIN_PWR_ON;
				}
			}else{
				PMIC_EN0 = 0;
				// Set up timer for POWERING_OFF_VIN_PWR_ON
				clear_flag_and_start_timer1(TIMER1_POWER_OFF_VIN);
				power_event = POWERING_OFF_VIN_PWR_ON;
			}
    }
    else if (power_event == POWERING_OFF_VIN_PWR_ON)
    {
        if (gd.timer1_flag)
        {
            gd.timer1_flag = 0;
            VIN_PWR_ON = 0;
            clear_flag_and_start_timer1(TIMER1_LEAK);
            power_event = POWERING_OFF_LEAK;
        }
    }
    else if (power_event == POWERING_OFF_LEAK)
    {
				HOST_POWER_ENABLE = 0;
        if (gd.timer1_flag)
        {
            gd.timer1_flag = 0;
            enter_power_state(PowerOff);
        }
    }
    else
    {
        // do nothing
    }
}

void Reset(void)
{
    if (power_event == INIT)
    {
        power_event = NONE; // clear power event
    }

    if (power_event == NONE)
    {
        // exit reset state after 100ms to ignore PWR_GOOD and FORCE_SHUTDOWN while Tegra is being reset
        clear_flag_and_start_timer1(TIMER1_RESET);
        power_event = RESET_IGNORE_PWR_GOOD;
    }
    else if (power_event == RESET_IGNORE_PWR_GOOD)
    {
        if (gd.timer1_flag == 1)
        {
            enter_power_state(PowerOn);
        }
    }
    else
    {
        // do nothing
    }
}

