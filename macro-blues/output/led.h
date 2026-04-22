#ifndef LED_H
#define LED_H

/*
 * led.h — LED driver + debug indicator patterns
 *
 * Controls the on-board status LEDs.  Also provides simple visual
 * feedback helpers (flash, blink patterns) that used to live in debug.c.
 */

#include <stdint.h>
#include "board.h"

/* Convenience aliases so call-sites read nicely: led_on(LED_RED) */
#define LED_RED PIN_LED_RED
#define LED_BLUE PIN_LED_BLUE

void led_init(void);

void led_on(uint32_t pin);
void led_off(uint32_t pin);
int led_is_on(uint32_t pin);
void led_toggle(uint32_t pin);
void led_all_off(void);

/* Debug / status patterns */
void led_flash(uint32_t pin, int ms); /* on → wait → off */

#endif /* LED_H */
