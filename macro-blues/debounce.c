#include "debounce.h"

/*
 * Per-key debounce state.  All arrays are indexed 0..NUM_KEYS-1.
 *
 * accepted[]  — the "clean" state we report to the rest of the firmware.
 * counter[]   — how many consecutive scans the raw reading has disagreed
 *               with accepted[].  Resets to 0 when they agree.
 * fell_flag[] — set to 1 for exactly one scan cycle when a key transitions
 *               from released to pressed.
 * rose_flag[] — same, but for pressed → released.
 */
static int accepted[NUM_KEYS];
static int counter[NUM_KEYS];
static int fell_flag[NUM_KEYS];
static int rose_flag[NUM_KEYS];

void debounce_init(void)
{
	for (int i = 0; i < NUM_KEYS; i++) {
		accepted[i]  = 0;
		counter[i]   = 0;
		fell_flag[i] = 0;
		rose_flag[i] = 0;
	}
}

void debounce_update(const int raw[NUM_KEYS])
{
	for (int i = 0; i < NUM_KEYS; i++) {
		/* Clear edge flags from the previous cycle */
		fell_flag[i] = 0;
		rose_flag[i] = 0;

		if (raw[i] != accepted[i]) {
			/* Raw disagrees with accepted — count up */
			counter[i]++;
			if (counter[i] >= DEBOUNCE_CYCLES) {
				/* Stable long enough — accept the new state */
				accepted[i] = raw[i];
				counter[i]  = 0;

				if (accepted[i])
					fell_flag[i] = 1;  /* just pressed */
				else
					rose_flag[i] = 1;  /* just released */
			}
		} else {
			/* Raw agrees with accepted — reset counter */
			counter[i] = 0;
		}
	}
}

int debounce_state(int key)
{
	return accepted[key];
}

int debounce_fell(int key)
{
	return fell_flag[key];
}

int debounce_rose(int key)
{
	return rose_flag[key];
}

int debounce_settling(void)
{
	for (int i = 0; i < NUM_KEYS; i++)
		if (counter[i] != 0)
			return 1;
	return 0;
}
