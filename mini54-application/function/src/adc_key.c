/*
 * Three keys on one ADC pin : MCU_ADC_CH1_IN, defined in board.h.
 *
 * Derived from the vendor sample in ADC_Convert/main.c, which converts
 * MCU_ADC_CH1_AIN and prints the result.  Two things differ : the conversion
 * is polled here instead of being interrupt driven, and the result is used to
 * pick a key out of a resistor ladder rather than just being reported.
 *
 * Scanning runs from a 10ms software timer (see core.c
 * RegistAlarmEventCallback) and the result is published to the host through
 * the MCU_ADC_KEY / MCU_ADC_KEY_RAW_H / MCU_ADC_KEY_RAW_L registers.
 */
#include "core.h"
#include "board.h"
#include "adc_key.h"

/* --------------------------------------------------------------------------
 * Thresholds - the only thing to touch when calibrating.
 *
 * The ADC result is 10 bit : 0 .. 1023, full scale = VDD.
 *
 * Measured on two boards, and they disagree.  The pin is pulled up by R417,
 * 10K to VCC3_MCU, on the main board, and the ladder (three rungs to ground)
 * sits on the key board behind AUDIO_CON1, so what the ADC sees is the rung
 * against that pull up - and the two boards sit a factor of two apart in
 * that ratio :
 *
 *                     board A   board B     rung
 *     ADC_KEY_1            19        10     ~0, the key to ground
 *     ADC_KEY_2           504       333     ~1/2 VDD
 *     ADC_KEY_3           770       617     ~3/4 VDD
 *     nothing pressed    1023      1023     open, sits at VDD
 *
 * All six numbers are the same three rungs seen through a 10K pull up on
 * one board and a 20K on the other; they are equally the 10K pull up behind
 * a ladder of half the resistance.  Which of the two moved is still open -
 * measure R417 on the main board (the schematic says 10K) and the rung
 * resistances on the key board (0.19 / 9.7 / 30.4 kOhm through the 10K) to
 * settle it.  The windows below hold both sets at once, so one image covers
 * either board; the tightest fit is board A's 504 to T2_HI and board B's
 * 617 to T3_LO, 48 counts either way.
 *
 * The lowest rung is not a ladder resistor : it ties the pin to ground
 * through the switch and the trace, which is why it reads ~0 instead of a
 * quarter point.  The middle and high rungs do sit on the 1/2 and 3/4
 * points.
 *
 * Anything that falls between the windows - including a pin shorted to
 * ground - reads as ADC_KEY_NONE, so a miscalibrated board reports "no key"
 * rather than a wrong one.
 *
 * TO RECALIBRATE : hold one key at a time and watch the "adc key : raw ..."
 * line on the UART (or read MCU_ADC_KEY_RAW_H/L over I2C), then move that
 * key's two bounds so the measured value sits centred.
 * ------------------------------------------------------------------------ */
#define ADC_KEY_T1_LO	0u	/* 19 on board A, 10 on board B */
#define ADC_KEY_T1_HI	160u
#define ADC_KEY_T2_LO	176u	/* 504 on board A, 333 on board B */
#define ADC_KEY_T2_HI	552u
#define ADC_KEY_T3_LO	568u	/* 770 on board A, 617 on board B */
#define ADC_KEY_T3_HI	896u
#define ADC_KEY_IDLE_LO	928u	/* 1023 = VDD, nothing pressed */
#define ADC_KEY_RAW_MAX	1023u	/* the ADC result is 10 bit */

/* One 10ms scan, and 3 identical scans (30ms) before a key is accepted.
   The scan has to be shorter than the shortest tap anyone makes : a press
   that starts and ends between two scans is never seen at all, and at the
   100ms this first ran at, an ordinary 50..150ms tap fell straight through
   that gap and read as "no key" - which is what the first board test showed.
   At 10ms a tap only has to last 30ms, and 3 samples still ride out a tactile
   switch's bounce, which is over well inside one scan period.
   10ms is also as fine as this can be asked for : core.c hands the alarm list
   a 10ms step, so anything shorter would silently round up to it. */
#define ADC_KEY_SCAN_MS		10u
#define ADC_KEY_DEBOUNCE_N	3u
/* Print again once the raw sample has moved by this much.  This is the gate of
   adc_key_debug().  The main loop calls that function with no delay at all, so
   without a gate it would print continuously; the step is far below the swing
   any real key produces, and far above the ADC's last-digit jitter. */
#define ADC_KEY_DBG_STEP	8u

/* The conversion has to be bounded : this runs in the SysTick interrupt and a
   stuck ADC must never be allowed to hold the tick.  With the 1024-clock
   sample window set up below, one conversion is about 520us at the 2MHz ADC
   clock, and the loop below runs at roughly 0.8us per pass, so a normal
   conversion is over after about 650 of these passes.  The fence itself is
   3.2ms - still inside one 10ms scan. */
#define ADC_KEY_SPIN_MAX	4000u

typedef struct {
	uint16_t lo;	/* first count of the window, inclusive */
	uint16_t hi;	/* last  count of the window, inclusive */
	uint8_t  key;	/* adc_key_t reported inside this window */
} adc_key_win_t;

/* first match wins, anything that falls through is ADC_KEY_NONE */
static const adc_key_win_t adc_key_win[] = {
	{ ADC_KEY_IDLE_LO, ADC_KEY_RAW_MAX, ADC_KEY_NONE },
	{ ADC_KEY_T1_LO,   ADC_KEY_T1_HI,   ADC_KEY_1 },
	{ ADC_KEY_T2_LO,   ADC_KEY_T2_HI,   ADC_KEY_2 },
	{ ADC_KEY_T3_LO,   ADC_KEY_T3_HI,   ADC_KEY_3 },
};
#define ADC_KEY_WIN_NUM	(sizeof(adc_key_win) / sizeof(adc_key_win[0]))

/* debounced key, this is what the host reads */
static volatile uint8_t  adc_key_state = ADC_KEY_NONE;
/* latest conversion result, for calibration */
static volatile uint16_t adc_key_value = 0;
/* classification of the last scan, and how often it has repeated */
static uint8_t  adc_key_candidate = ADC_KEY_NONE;
static uint8_t  adc_key_count = 0;

static int AdcKeyAlarmCallback(struct timer_struct *__this);

/* There is deliberately no .name here : RegistAlarmEventCallback() hands the
   whole struct to list_add() as a list node, which overwrites offset 0 - the
   name field - with the next pointer.  A name would be a string no one can
   ever read, so it is left out to keep the string out of the image. */
struct timer_struct AdcKeyAlarmItem = {
	.enabled	= 0,
	.function	= AdcKeyAlarmCallback,
};

static uint8_t adc_key_classify(uint16_t value)
{
	uint8_t i;

	for (i = 0; i < ADC_KEY_WIN_NUM; i++) {
		if (value >= adc_key_win[i].lo && value <= adc_key_win[i].hi)
			return adc_key_win[i].key;
	}
	return ADC_KEY_NONE;
}

/*
 * One polled conversion.  The vendor sample waits for the conversion complete
 * interrupt; polling keeps the ADC out of the NVIC entirely.
 * Returns ADC_KEY_RAW_MAX + 1 when the conversion did not finish in time.
 */
static uint16_t adc_key_convert(void)
{
	uint32_t spin;

	ADC_START_CONV(ADC);
	for (spin = 0; spin < ADC_KEY_SPIN_MAX && ADC_IS_BUSY(ADC); spin++)
		;

	if (ADC_IS_BUSY(ADC))
		return ADC_KEY_RAW_MAX + 1;

	return (uint16_t)ADC_GET_CONVERSION_DATA(ADC, MCU_ADC_CH1_AIN);
}

/*
 * 10ms scan callback.  This runs in the SysTick interrupt, so it stays
 * short : no printf here, the UART blocks at 115200 for ~87us per character
 * and SysTick is the very time base that drives this scan, so printing would
 * feed back into its own schedule.  adc_key_debug() does the printing.
 */
static int AdcKeyAlarmCallback(struct timer_struct *__this)
{
	uint16_t value;
	uint8_t  key;

	/* RegistAlarmEventCallback() casts this struct to a list node, so
	   __this->name holds a pointer now - never read it. */

	value = adc_key_convert();
	if (value > ADC_KEY_RAW_MAX)	/* timed out, keep the previous state */
		return 0;

	adc_key_value = value;
	key = adc_key_classify(value);

	if (key == adc_key_candidate) {
		if (adc_key_count < ADC_KEY_DEBOUNCE_N)
			adc_key_count++;
	} else {
		adc_key_candidate = key;
		adc_key_count = 1;
	}

	/* one byte store, safe against the I2C interrupt reading it */
	if (adc_key_count >= ADC_KEY_DEBOUNCE_N)
		adc_key_state = adc_key_candidate;

	return 0;
}

void adc_key_init(void)
{
	uint16_t value;

	/* the mux register, APBCLK, CLKSEL1 and CLKDIV are write protected and we
	   are called with the registers locked, so unlock around the setup. */
	SYS_UnlockReg();

	/* MCU_ADC_CH1_IN as analog input AIN5.  The mux register it sits in may
	   carry other pins too (the UART pins on P1, set in board.c SYS_Clock),
	   so read-modify-write only. */
	MCU_ADC_CH1_MFP = (MCU_ADC_CH1_MFP & ~MCU_ADC_CH1_MFP_Msk) | MCU_ADC_CH1_MFP_AIN;

	/* the digital input buffer of an analog pin draws current around mid
	   rail and loads the ladder - the vendor sample disables it too */
	GPIO_DISABLE_DIGITAL_PATH(MCU_ADC_CH1_GPIO, MCU_ADC_CH1_BIT);

	CLK_EnableModuleClock(ADC_MODULE);
	/* ADC clock = XTL 12MHz / 6 = 2MHz, as in the vendor sample.  The slow
	   clock with a long sample window below settles better on a ladder that
	   presents a few kOhm of source impedance.

	   The equivalent driver call would be
	   CLK_SetModuleClock(ADC_MODULE, CLK_CLKSEL1_ADC_S_XTAL, CLK_CLKDIV_ADC(6)).
	   That routine decodes the module index into a register offset, a mask
	   and a shift at run time, and adc_key.c is its only caller in the whole
	   project, so writing the two registers out directly keeps 64 bytes of
	   generic decoding out of the image.  ADC_MODULE encodes CLKSEL1 with
	   mask 0x3 at bit 2 and CLKDIV with mask 0xff at bit 16, which is exactly
	   what these two read-modify-writes apply. */
	CLK->CLKSEL1 = (CLK->CLKSEL1 & ~CLK_CLKSEL1_ADC_S_Msk) | CLK_CLKSEL1_ADC_S_XTAL;
	CLK->CLKDIV  = (CLK->CLKDIV & ~(0xFFu << 16)) | CLK_CLKDIV_ADC(6);

	SYS_LockReg();

	/* The ADC registers are not write protected.  ADCR is cleared first (it
	   carries ADEN), so powering on has to come after it.

	   ADC_Open() and ADC_SetExtraSampleTime() are written out for the same
	   reason as the clock above : they are the only two functions of adc.c
	   that this project links.  On Mini51 ADC_Open() ignores its u32InputMode
	   and u32OpMode arguments - it only clears ADCR and sets the channel mask
	   - and ADC_SetExtraSampleTime() ignores u32ChNum, since ADSAMP has the
	   single SAMPCNT field.  Each one is really just the register write
	   shown below. */
	ADC->ADCR   = 0;
	ADC->ADCHER = (ADC->ADCHER & ~ADC_ADCHER_CHEN_Msk) | (0x01u << MCU_ADC_CH1_AIN);
	/* 1024 ADC clocks, i.e. a 512us sample window at the 2MHz clock above,
	   where the vendor sample uses 16 clocks (8us).

	   This came out of an A/B test that has since been voided : the key
	   connector was on backwards for both builds, so the pin was looking at
	   the wrong net and neither reading described this ladder.  It is kept
	   because the ladder's source impedance is still unknown and a long
	   window is the safe side of that unknown.  It does cost something now
	   that the scan runs at 10ms : this conversion holds the SysTick handler
	   for about 520us out of every 10ms, which is why core_init() drops
	   SysTick to the lowest interrupt priority - the I2C slave then preempts
	   it rather than waiting it out.  A 16-clock window would cut that to
	   about 10us, but re-run the A/B (16 vs 1024, same board state, against
	   a multimeter on P1.5) before changing it. */
	ADC->ADSAMP = (ADC->ADSAMP & ~ADC_ADSAMP_SAMPCNT_Msk) | ADC_SAMPLE_CLOCK_1024;
	ADC_POWER_ON(ADC);

	/* let the analog front end settle, then drop the first conversion */
	Delay1ms(2);
	value = adc_key_convert();
	if (value <= ADC_KEY_RAW_MAX)
		adc_key_value = value;

	/* Register and arm last : the callback must not run before the ADC is
	   up.  Note that RegistAlarmEventCallback() casts the item to a list
	   node, so AdcKeyAlarmItem.name is overwritten from here on. */
	RegistAlarmEventCallback(&AdcKeyAlarmItem);
	UpdateAlarmEventTime(&AdcKeyAlarmItem, ADC_KEY_SCAN_MS, 1);

	//DEBUG("adc key : init raw %u\n", adc_key_value);
}

uint8_t adc_key_get(void)
{
	return adc_key_state;
}

uint16_t adc_key_raw(void)
{
	return adc_key_value;
}

/*
 * Called from the main loop, and the only place a sample reaches the console.
 * That loop has no delay in it, so this runs far faster than the 10ms scan -
 * the gate below is what keeps it to one line per change rather than a stream.
 *
 * A line is printed when the debounced key changes, or when the raw sample has
 * moved by ADC_KEY_DBG_STEP since the last line.  The raw half is what makes
 * this usable for calibration : press one key, read its value off the console,
 * and move that key's two bounds so the measured value sits centred.  It stays
 * in the shipped build : the raw value is also what tells a miscalibrated key
 * board apart from a broken one.
 *
 * Reading the two volatile halves separately is safe here : a scan can land in
 * between, which at worst pairs this scan's key with the previous scan's raw in
 * a single line.  Both are 16 bit, so neither read can tear.
 */
/*调试用，不是任务函数，adc_key按键的任务函数由按键触发*/
void adc_key_debug(void)
{
	static uint8_t  last_key = 0xFF;
	static uint16_t last_raw = 0;
	uint16_t raw = adc_key_value;
	uint8_t  key = adc_key_state;
	uint16_t diff;

	if (key == last_key) {
		diff = (raw > last_raw) ? (raw - last_raw) : (last_raw - raw);
		if (diff < ADC_KEY_DBG_STEP)
			return;
	}

	last_key = key;
	last_raw = raw;

	//DEBUG("adc key : raw %u key %d\n", raw, key);
}
