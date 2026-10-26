/**************************************************************************//**
 * @file     timer.c
 * @version  V1.00
 * $Revision: 11 $
 * $Date: 14/01/28 10:29a $
 * @brief    MINI51 series TIMER driver source file
 *
 * @note
 * Copyright (C) 2013 Nuvoton Technology Corp. All rights reserved.
*****************************************************************************/
#include "Mini51Series.h"

/** @addtogroup MINI51_Device_Driver MINI51 Device Driver
  @{
*/

/** @addtogroup MINI51_TIMER_Driver TIMER Driver
  @{
*/


/** @addtogroup MINI51_TIMER_EXPORTED_FUNCTIONS TIMER Exported Functions
  @{
*/

/**
  * @brief This API is used to configure timer to operate in specified mode
  *        and frequency. If timer cannot work in target frequency, a closest
  *        frequency will be chose and returned.
  * @param[in] timer The base address of Timer module
  * @param[in] u32Mode Operation mode. Possible options are
  *                 - \ref TIMER_ONESHOT_MODE
  *                 - \ref TIMER_PERIODIC_MODE
  *                 - \ref TIMER_TOGGLE_MODE
  *                 - \ref TIMER_CONTINUOUS_MODE
  * @param[in] u32Freq Target working frequency
  * @return Real Timer working frequency
  * @note After calling this API, Timer is \b NOT running yet. But could start timer running be calling
  *       \ref TIMER_Start macro or program registers directly
  */
uint32_t TIMER_Open(TIMER_T *timer, uint32_t u32Mode, uint32_t u32Freq)
{
    uint32_t u32Clk = TIMER_GetModuleClock(timer);
    uint32_t u32Cmpr = 0, u32Prescale = 0;

    // Fastest possible timer working freq is u32Clk / 2. While cmpr = 2, pre-scale = 0
    if(u32Freq > (u32Clk / 2)) {
        u32Cmpr = 2;
    } else {
        if(u32Clk > 0xFFFFFF) { // For Mini, only needs to consider 24MHz at most
            u32Prescale = 1;
            u32Clk >>= 1;
        }
        u32Cmpr = u32Clk / u32Freq;
    }

    timer->TCSR = u32Mode | u32Prescale;
    timer->TCMPR = u32Cmpr;

    return(u32Clk / (u32Cmpr * (u32Prescale + 1)));
}

/**
  * @brief This API stops Timer counting and disable the Timer interrupt function
  * @param[in] timer The base address of Timer module
  * @return None
  */
void TIMER_Close(TIMER_T *timer)
{
    timer->TCSR = 0;
    timer->TEXCON = 0;

}

/**
  * @brief This API is used to create a delay loop for u32usec micro seconds
  * @param[in] timer The base address of Timer module
  * @param[in] u32Usec Delay period in micro seconds with 10 usec every step. Valid values are between 10~1000000 (10 micro second ~ 1 second)
  * @return None
  * @note This API overwrites the register setting of the timer used to count the delay time.
  * @note This API use polling mode. So there is no need to enable interrupt for the timer module used to generate delay
  */
void TIMER_Delay(TIMER_T *timer, uint32_t u32Usec)
{
    uint32_t u32Clk = TIMER_GetModuleClock(timer);
    uint32_t u32Prescale = 0, delay = SystemCoreClock / u32Clk;

    // Clear current timer configuration
    timer->TCSR = 0;
    timer->TEXCON = 0;

    if(u32Clk == 10000) {         // min delay is 100us if timer clock source is LIRC 10k
        u32Usec = ((u32Usec + 99) / 100) * 100;
    } else {    // 10 usec every step
        u32Usec = ((u32Usec + 9) / 10) * 10;
    }

    if(u32Clk > 0xFFFFFF) { // For Mini, only needs to consider 24MHz at most
        u32Prescale = 1;
        u32Clk >>= 1;
    }

    // u32Usec * u32Clk might overflow if using uint32_t : up to 1e6 usec at
    // 24 MHz is 2.4e13, which needs 45 bits.  The original code formed the
    // product in float, and on this FPU-less Cortex-M0 that one line pulled
    // the whole soft float library into the image - fdiv, fmul, ffixui,
    // ffltui and fepilogue, 430 bytes - to compute what only ever becomes a
    // 24-bit compare value.
    //
    // Splitting both operands at 1000 keeps every step inside 32 bits, so
    // this needs neither float nor a 64-bit multiply helper (the M0 has no
    // UMULL, so forming the product in uint64_t would pull in __aeabi_lmul) :
    //
    //     a*b = (1000*a1 + a0) * (1000*b1 + b0)
    //         = 1e6*a1*b1 + 1000*s + t     s = a1*b0 + a0*b1
    //                                      t = a0*b0
    //
    // so  a*b/1e6 = a1*b1 + (1000*s + t)/1e6, and with s = 1000*q + r that
    // second term is q + ((1000*r + t) >= 1e6), since 1000*r + t < 2e6.  The
    // last line below is that carry.  So this is the exact quotient, not an
    // approximation - the float version could not even be exact, carrying
    // only 24 mantissa bits, which at the 1.2e12 of a 100ms delay at 12MHz
    // resolve no finer than 131072.
    //
    // Bounds : u32Clk is at most 0xFFFFFF here (the prescale branch above)
    // and TIMER_Delay is documented for u32Usec up to 1e6, so a1 <= 1000,
    // b1 <= 16777, s <= 1.78e7 and 1000*r + t <= 2.0e6 - all far inside
    // 32 bits, and the quotient stays well inside the 24-bit TCMPR.
    {
        uint32_t a1 = u32Usec / 1000, a0 = u32Usec % 1000;
        uint32_t b1 = u32Clk  / 1000, b0 = u32Clk  % 1000;
        uint32_t s  = a1 * b0 + a0 * b1;

        timer->TCMPR = a1 * b1 + s / 1000
                     + (((s % 1000) * 1000 + a0 * b0) >= 1000000u);
    }
    timer->TCSR = TIMER_TCSR_CEN_Msk | u32Prescale; // one shot mode

    // When system clock is faster than timer clock, it is possible timer active bit cannot set in time while we check it.
    // And the while loop below return immediately, so put a tiny delay here allowing timer start counting and raise active flag.
    for(; delay > 0; delay--) {
        __NOP();
    }

    while(timer->TCSR & TIMER_TCSR_CACT_Msk);

}

/**
  * @brief This API is used to enable timer capture function with specified mode and capture edge
  * @param[in] timer The base address of Timer module
  * @param[in] u32CapMode Timer capture mode. Could be
  *                 - \ref TIMER_CAPTURE_FREE_COUNTING_MODE
  *                 - \ref TIMER_CAPTURE_TRIGGER_COUNTING_MODE
  *                 - \ref TIMER_CAPTURE_COUNTER_RESET_MODE
  * @param[in] u32Edge Timer capture edge. Possible values are
  *                 - \ref TIMER_CAPTURE_FALLING_EDGE
  *                 - \ref TIMER_CAPTURE_RISING_EDGE
  *                 - \ref TIMER_CAPTURE_FALLING_THEN_RISING_EDGE
  *                 - \ref TIMER_CAPTURE_RISING_THEN_FALLING_EDGE
  * @return None
  * @note Timer frequency should be configured separately by using \ref TIMER_Open API, or program registers directly
  */
void TIMER_EnableCapture(TIMER_T *timer, uint32_t u32CapMode, uint32_t u32Edge)
{

    timer->TEXCON = (timer->TEXCON & ~(TIMER_TEXCON_CAP_MODE_Msk |
                                       TIMER_TEXCON_RSTCAPSEL_Msk |
                                       TIMER_TEXCON_TEX_EDGE_Msk)) |
                    u32CapMode | u32Edge | TIMER_TEXCON_TEXEN_Msk;
}

/**
  * @brief This API is used to disable the Timer capture function
  * @param[in] timer The base address of Timer module
  * @return None
  */
void TIMER_DisableCapture(TIMER_T *timer)
{
    timer->TEXCON &= ~TIMER_TEXCON_TEXEN_Msk;

}

/**
  * @brief This function is used to enable the Timer counter function with specify detection edge
  * @param[in] timer The base address of Timer module
  * @param[in] u32Edge Detection edge of counter pin. Could be ether
  *             - \ref TIMER_COUNTER_RISING_EDGE, or
  *             - \ref TIMER_COUNTER_FALLING_EDGE
  * @return None
  * @note Timer compare value should be configured separately by using \ref TIMER_SET_CMP_VALUE macro or program registers directly
  */
void TIMER_EnableEventCounter(TIMER_T *timer, uint32_t u32Edge)
{
    timer->TEXCON = (timer->TEXCON & ~TIMER_TEXCON_TX_PHASE_Msk) | u32Edge;
    timer->TCSR |= TIMER_TCSR_CTB_Msk;
}

/**
  * @brief This API is used to disable the Timer event counter function.
  * @param[in] timer The base address of Timer module
  * @return None
  */
void TIMER_DisableEventCounter(TIMER_T *timer)
{
    timer->TCSR &= ~TIMER_TCSR_CTB_Msk;
}

/**
  * @brief This API is used to get the clock frequency of Timer
  * @param[in] timer The base address of Timer module
  * @return Timer clock frequency
  * @note This API cannot return correct clock rate if timer source is external clock input.
  */
uint32_t TIMER_GetModuleClock(TIMER_T *timer)
{
    uint32_t u32Src;
    if(timer == TIMER0)
        u32Src = (CLK->CLKSEL1 & CLK_CLKSEL1_TMR0_S_Msk) >> CLK_CLKSEL1_TMR0_S_Pos;
    else
        u32Src = (CLK->CLKSEL1 & CLK_CLKSEL1_TMR1_S_Msk) >> CLK_CLKSEL1_TMR1_S_Pos;

    if(u32Src == 0)
        return((CLK->PWRCON & CLK_PWRCON_XTLCLK_EN_Msk) == 1 ? __XTAL12M : __XTAL32K);
    else if(u32Src == 1)
        return __IRC10K;
    else if(u32Src == 2)
        return SystemCoreClock;
    else
        return __IRC22M;

}

/*@}*/ /* end of group MINI51_TIMER_EXPORTED_FUNCTIONS */

/*@}*/ /* end of group MINI51_TIMER_Driver */

/*@}*/ /* end of group MINI51_Device_Driver */

/*** (C) COPYRIGHT 2013 Nuvoton Technology Corp. ***/
