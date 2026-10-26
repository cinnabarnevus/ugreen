#ifndef __LED_H__
#define __LED_H__
#include <stdio.h>
#include "Mini51Series.h"

typedef struct {
    uint32_t color;
    uint32_t color_off;
    uint8_t  blink_timer;
} led_color_t;

typedef struct {
    uint16_t tick_100ms;
    uint16_t last_tick;
    uint16_t phase_tick;
    uint8_t  led_on;
    uint8_t  breath_brightness;
    uint8_t  breath_up;
    led_color_t colors;
} led_ctrl_t;

typedef enum {
    LED_MODE_ON = 0,
    LED_MODE_BLINK,
    LED_MODE_BREATHING,
} led_mode_t;

extern volatile led_mode_t g_led_mode;

void RGB_SendData(uint32_t data);   /* send one 24bit GRB frame */
void RGB_Reset(uint32_t hi, uint32_t lo);   /* reset/latch pulse after a frame */
void Timer1_Init(void);
void RGB_SetMode(led_mode_t mode);
void init_led_colors(void);
void set_color(uint32_t value);
void set_blink_timer(uint8_t value);
int RGB_Process(void);

#endif
