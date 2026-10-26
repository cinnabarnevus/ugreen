#ifndef __ADC_KEY_H__
#define __ADC_KEY_H__
#include <stdio.h>
#include "Mini51Series.h"

/*
 * Three keys sharing one ADC pin : MCU_ADC_CH1_IN, defined in board.h.
 *
 * The keys sit on a resistor ladder between VDD and GND, so each one pulls
 * the pin to a different voltage and the pressed key is identified by which
 * window the conversion result falls into.  Only the pin is read here - the
 * keys are not wired into the power state machine yet.
 *
 * The windows live in adc_key.c and are calibrated against the real ladder,
 * so the key numbering only means anything on a board that matches the one
 * they were measured on.
 */

typedef enum {
	ADC_KEY_NONE = 0,
	ADC_KEY_1,
	ADC_KEY_2,
	ADC_KEY_3,
} adc_key_t;

void     adc_key_init(void);	/* clock, pin, ADC and the scan timer */
uint8_t  adc_key_get(void);		/* debounced key, 0..3 */
uint16_t adc_key_raw(void);		/* latest conversion result, for calibration */
void     adc_key_debug(void);	/* call from the main loop */

#endif
