#include "nrf52832.h"
#include "gpio.h"
#include "board.h"
#include "keyswitch.h"

const uint32_t row_pins[NUM_ROWS] = {PIN_ROW0, PIN_ROW1, PIN_ROW2, PIN_ROW3};
const uint32_t col_pins[NUM_COLS] = {PIN_COL0, PIN_COL1, PIN_COL2};

void key_init(void)
{
	/* Rows: start as inputs (HiZ — no pull needed while not strobing) */
	for (int r = 0; r < NUM_ROWS; r++)
		gpio_pin_cfg_input(row_pins[r]);

	/* Cols: inputs with pull-up — a pressed key pulls them LOW */
	for (int c = 0; c < NUM_COLS; c++)
		gpio_pin_cfg_input(col_pins[c]);
}

void key_scan(int *state)
{
	for (int r = 0; r < NUM_ROWS; r++)
	{
		/* Drive this row LOW */
		gpio_pin_cfg_output(row_pins[r]);
		gpio_pin_clear(row_pins[r]);

		/* Let signals settle */
		for (volatile int d = 0; d < 50; d++)
			;

		/* Read all columns: 0 = pressed (active-low through switch) */
		for (int c = 0; c < NUM_COLS; c++)
			state[r * NUM_COLS + c] = (gpio_pin_read(col_pins[c]) == 0) ? 1 : 0;

		/* Release row back to input-with-pull-up */
		gpio_pin_cfg_input(row_pins[r]);
	}
}

int key_any_pressed(void)
{
	int state[NUM_KEYS];
	key_scan(state);
	for (int i = 0; i < NUM_KEYS; i++)
		if (state[i])
			return 1;
	return 0;
}
