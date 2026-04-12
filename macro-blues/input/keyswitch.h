#ifndef KEYSWITCH_H
#define KEYSWITCH_H

/*
 * keyswitch.h — Key input scanning
 *
 * Handles GPIO setup for key switches and reading their state.
 * The main loop lives in main.c — this module only provides
 * init and query functions.
 *
 * All 12 keys are direct-wired: GPIO → switch → GND, with the
 * nRF52's internal pull-up enabled. Pressing a key pulls the
 * pin LOW (active-low).
 */

#include <stdint.h>
#include "board.h"

/* Ordered array of key GPIO pin numbers, indexed 0..NUM_KEYS-1 */
extern const uint32_t key_pins[NUM_KEYS];

void key_init(void);                /* configure all 12 key GPIOs */
int  key_pressed(uint32_t pin);     /* 1 if individual pin is low */
void key_scan(int *state);          /* fill state[NUM_KEYS]: 1=pressed */
int  key_any_pressed(void);         /* 1 if any key is currently down */

#endif /* KEYSWITCH_H */
