#ifndef KEYSWITCH_H
#define KEYSWITCH_H

/*
 * keyswitch.h — Key input scanning
 *
 * Handles GPIO setup for key switches and reading their state.
 * The main loop lives in main.c — this module only provides
 * init and query functions.
 */

#include <stdint.h>

void key_init(void);
int key_pressed(uint32_t pin); /* returns 1 if key is currently down */

#endif /* KEYSWITCH_H */
