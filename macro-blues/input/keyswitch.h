#ifndef KEYSWITCH_H
#define KEYSWITCH_H

/*
 * keyswitch.h — 4×3 matrix key scanner
 *
 * Rows (PIN_ROW0..3) are driven LOW one at a time.
 * Columns (PIN_COL0..2) are read with internal pull-ups.
 * A pressed key pulls its column LOW while its row is strobed.
 *
 * key_scan() fills state[NUM_KEYS] in row-major order:
 *   index = row * NUM_COLS + col
 */

#include <stdint.h>
#include "board.h"

extern const uint32_t row_pins[NUM_ROWS];
extern const uint32_t col_pins[NUM_COLS];

void key_init(void);       /* configure row and col GPIOs */
void key_scan(int *state); /* fill state[NUM_KEYS]: 1=pressed */
int key_any_pressed(void); /* 1 if any key is currently down */

#endif /* KEYSWITCH_H */
