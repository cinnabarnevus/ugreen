#include "core.h"
#include "board.h"
#include "led.h"

/*
 * ARGB data line : P2.2 (MCU_RGB_CTL, see board.h).
 * Drives the H3500 light board, whose LEDs are HZ3210ARGB (see the datasheet
 * PDF in the project root).
 *
 * P2_DOUT = P2_BASE(0x50004080) + 0x08
 *
 * NEVER write a plain 0 / 0x04 word into P2_DOUT : P2.3..P2.6 drive 12V enable,
 * SATA power (active low), the host power button and the host reset line.
 * Both levels are written back with all other P2 outputs preserved, using the
 * "hi" / "lo" values latched once per frame.
 *
 * On the board the data line is driven by an open drain stage (Q60/Q62 with a
 * 470ohm pull up to 5V, the Q7 level shifter is not fitted).  It does NOT
 * invert the signal.  The strip latches on a LOW level and idles LOW.
 */
#define P2_DOUT   (*(volatile uint32_t *)0x50004088)
#define RGB_BIT   (1u << 2)

/*
 * Bit timing.  12MHz core, 1 nop = 1 cycle = 83.3ns.
 *
 * Values from the HZ3210ARGB datasheet (the LED used on the H3500 light
 * board).  The bit value is carried by the HIGH duration :
 *      '0' : T0H 0.3us / T0L 0.6us        '1' : T1H 0.6us / T1L 0.3us
 *      reset : line LOW for >= 80us       data : 24bit, GRB, MSB first
 * Note the chip also says "does not light up at power on" - a strip that
 * shows white is receiving data, it just reads every bit as '1' because the
 * '0' HIGH is too long (> ~0.45us).
 *
 * Both stores of one bit are emitted from a SINGLE asm block on purpose :
 * that forces the compiler to load both values into registers BEFORE the
 * first store, so it cannot slip a load in between and stretch the HIGH
 * time.  The nops inside the block are the only variable.
 */
/* '0' : HIGH = 2 store + 2 nop = 4 cycles = 333ns */
#define RGB_SEND_BIT_0(hi, lo)                                              \
    __asm volatile ("str %[hi], [%[a]]        \n\t"                         \
                    "nop; nop                 \n\t"                         \
                    "str %[lo], [%[a]]        \n\t"                         \
                    "nop                       \n\t"                        \
                    : : [a] "r" (&P2_DOUT), [hi] "r" (hi), [lo] "r" (lo) : "memory")
/* '1' : HIGH = 2 store + 5 nop = 7 cycles = 583ns */
#define RGB_SEND_BIT_1(hi, lo)                                              \
    __asm volatile ("str %[hi], [%[a]]        \n\t"                         \
                    "nop; nop; nop; nop; nop  \n\t"                         \
                    "str %[lo], [%[a]]        \n\t"                         \
                    : : [a] "r" (&P2_DOUT), [hi] "r" (hi), [lo] "r" (lo) : "memory")
/* the line idles LOW (the strip latches on a low level) */
#define RGB_RESET_LEVEL(hi, lo)  RGB_Low(lo)

volatile led_mode_t g_led_mode;
static led_ctrl_t led_ctrl;

static inline __attribute__((always_inline)) void RGB_Store(uint32_t v)
{
    __asm volatile (
        "str %[v], [%[reg]]"
        :
        : [reg] "r" (&P2_DOUT),
          [v] "r" (v)
        : "memory"
    );
}

static inline __attribute__((always_inline)) void RGB_High(uint32_t hi)
{
    RGB_Store(hi);
}

static inline __attribute__((always_inline)) void RGB_Low(uint32_t lo)
{
    RGB_Store(lo);
}

/* drive the level the strip has to see to latch a frame.  The line is left at
 * that same level until the next frame (the strip idle level). */
void RGB_Reset(uint32_t hi, uint32_t lo)
{
    RGB_RESET_LEVEL(hi, lo);
    Delay1us(1200);     /* ~100us, the HZ3210 needs >= 80us low to latch */
}

/* RGB data send (software bit bang) - the frame is repeated 4 times, one for
 * each of the 4 LEDs.  See the bit timing macros above. */
void RGB_SendData(uint32_t data)
{
    uint8_t led_count;
    uint8_t i;
    uint32_t mask;
    uint32_t hi;
    uint32_t lo;

    /* keep the interrupts off, the timing is cycle sensitive */
    __disable_irq();

    /* latch the other P2 outputs once, only P2.2 may change from here on */
    hi = P2_DOUT | RGB_BIT;
    lo = P2_DOUT & ~RGB_BIT;

    for (led_count = 0; led_count < 4; led_count++) {
        mask = 0x800000;

        for (i = 0; i < 24; i++) {
            if (data & mask) {
                RGB_SEND_BIT_1(hi, lo);
            } else {
                RGB_SEND_BIT_0(hi, lo);
            }
            mask >>= 1;
        }
    }

    RGB_Reset(hi, lo);

    __enable_irq();
}

void TMR1_IRQHandler(void)
{
    /* check and clear the Timer1 interrupt flag */
    if (TIMER1->TISR & TIMER_TISR_TIF_Msk)
    {
        TIMER_ClearIntFlag(TIMER1);
        led_ctrl.tick_100ms++;
    }
}

/**
 * @brief Initialize Timer1 : 10ms periodic interrupt (12MHz clock)
 */
void Timer1_Init(void)
{
    /* 1. enable Timer1 clock */
    CLK_EnableModuleClock(TMR1_MODULE);

    /* 2. reset Timer1 */
    SYS_ResetModule(SYS_IPRSTC2_TMR1_RST_Msk);

    /* 3. clear the interrupt flag */
    TIMER1->TISR = TIMER_TISR_TIF_Msk;

    /* 4. stop and clear the control register */
    TIMER1->TCSR = 0;

    /*
     * Timer_clk = PCLK / (PRESCALE + 1)
     * 12MHz / (11 + 1) = 1MHz , 1MHz x 10000 = 10ms
     */
    TIMER1->TCSR |= (11 << TIMER_TCSR_PRESCALE_Pos);
    TIMER1->TCMPR = 10000;

    /* 5. periodic mode */
    TIMER1->TCSR &= ~TIMER_TCSR_MODE_Msk;
    TIMER1->TCSR |= TIMER_PERIODIC_MODE;

    /* 6. enable the interrupt */
    TIMER1->TCSR |= TIMER_TCSR_IE_Msk;

    /* 7. enable the Timer1 IRQ in the NVIC */
    NVIC_EnableIRQ(TMR1_IRQn);

    /* 8. start Timer1 */
    TIMER1->TCSR |= TIMER_TCSR_CEN_Msk;
}

void RGB_SetMode(led_mode_t mode)
{
    g_led_mode = mode;
    led_ctrl.phase_tick = 0;
}

void init_led_colors(void)
{
    struct RegistBuffer *Regist = (struct RegistBuffer*)RegistBuffer;
    led_ctrl.colors.color = (uint32_t)Regist->flashRegist.color[0] << 16 | (uint32_t)Regist->flashRegist.color[1] << 8 | (uint32_t)Regist->flashRegist.color[2];
    led_ctrl.colors.blink_timer = Regist->flashRegist.blinkTimer;
    led_ctrl.colors.color_off = 0x000000;

    /* The LED fields are new : a board updating from an older firmware still
       carries the old data flash layout, so blinkTimer reads back as 0 there
       (0xFF on a never programmed flash).  With blinkTimer 0 the blink mode
       would look like a steady light, so fall back to a sane default.
       The color is not touched, a 0x000000 written by the host means "off". */
    if (led_ctrl.colors.blink_timer == 0xFF || led_ctrl.colors.blink_timer == 0) {
        led_ctrl.colors.blink_timer = 0x05;
    }
    if (led_ctrl.colors.color == 0xFFFFFF) {
        /* completely erased data flash, never written by the host */
        led_ctrl.colors.color = 0xff0000;
    }

    /* RgbState is the register the host reads/writes, keep the running mode
       in sync with it - otherwise "set the same value again" would be
       ignored by the write callback in pm.c and the LED would not change. */
    RGB_SetMode((led_mode_t)Regist->RgbState);
}

void set_color(uint32_t value)
{
    led_ctrl.colors.color = value;
}

void set_blink_timer(uint8_t value)
{
    led_ctrl.colors.blink_timer = value;
}

int RGB_Process(void)
{
    uint32_t grb_data;

    if (led_ctrl.tick_100ms == led_ctrl.last_tick)
        return 0;

    led_ctrl.last_tick = led_ctrl.tick_100ms;

    if (g_led_mode == LED_MODE_BLINK && led_ctrl.colors.blink_timer != 0)
    {
        led_ctrl.phase_tick++;
    }

    switch (g_led_mode)
    {
    case LED_MODE_ON:
        led_ctrl.led_on = 1;
        RGB_SendData(led_ctrl.colors.color);
        break;
    case LED_MODE_BLINK:
        if (led_ctrl.colors.blink_timer == 0)
        {
            RGB_SendData(led_ctrl.colors.color);
        }else if(led_ctrl.phase_tick >= led_ctrl.colors.blink_timer*10){
            led_ctrl.phase_tick = 0;
            led_ctrl.led_on ^= 1;
            RGB_SendData(led_ctrl.led_on ? led_ctrl.colors.color : led_ctrl.colors.color_off);
        }
        break;
    case LED_MODE_BREATHING:
        if (led_ctrl.breath_up) {
            if (led_ctrl.breath_brightness >= 255 ) {
                led_ctrl.breath_brightness = 255;
                led_ctrl.breath_up = 0;
            } else {
                led_ctrl.breath_brightness += 1;
            }
        } else {
            if (led_ctrl.breath_brightness <= 1) {
                led_ctrl.breath_brightness = 0;
                led_ctrl.breath_up = 1;
            } else {
                led_ctrl.breath_brightness -= 1;
            }
        }

        grb_data = ((uint32_t)led_ctrl.breath_brightness << 16) | (0U << 8) | (0U);
        RGB_SendData(grb_data);
        break;
    default:
        break;
    }
    return 0;
}
