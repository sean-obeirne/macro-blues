#include <stdbool.h>

#include "board.h"
#include "gpio.h"
#include "timer.h"
#include "led.h"
#include "keyswitch.h"
#include "debounce.h"
#include "gpiote.h"
#include "ble_stack.h"

int main(void)
{
	/*
	 * ---- Phase 1: Pre-SoftDevice init ----
	 * GPIO, LEDs, keys, debounce — pure hardware, no SD dependency.
	 */
	led_init();
	key_init();
	debounce_init();

	/*
	 * ---- Phase 2: SoftDevice + BLE ----
	 * Enables the SD (takes over RADIO, RTC0, LFCLK, etc).
	 * After this call the device is advertising as "Macro Blues".
	 */
	ble_stack_init();

	/*
	 * ---- Phase 3: Post-SoftDevice init ----
	 * RTC1 needs LFCLK running (the SD started it in phase 2).
	 * GPIOTE priority must be set for SD coexistence.
	 */
	timer_init();
	gpiote_init();

	/* ---- Main loop ---- */
	led_all_off();

	int raw[NUM_KEYS];

	/*
	 * Simplified architecture: always scan keys + debounce on every
	 * iteration.  GPIOTE is used purely as a wake source, not for
	 * state management.  The loop decides whether to busy-poll (10 ms)
	 * or deep-sleep (sd_app_evt_wait) based on direct GPIO reads.
	 *
	 * This avoids the fragile IDLE/SCAN state machine and its race
	 * condition where gpiote_arm() could clear event_flag after the
	 * ISR already set it during ble_stack_process().
	 */
	while (true)
	{
		ble_stack_process();

		key_scan(raw);
		debounce_update(raw);

		for (int i = 0; i < NUM_KEYS; i++) {
			if (debounce_fell(i))
				led_toggle(LED_RED);
		}

		if (key_any_pressed() || debounce_settling()) {
			/* Keys are active — poll at ~10 ms for debounce. */
			wait_ms(10);
		} else {
			/* Idle — arm GPIOTE and sleep until a key press
			 * or BLE event wakes the CPU. */
			gpiote_arm();
			ble_stack_wait();
		}
	}

	return 0;
}