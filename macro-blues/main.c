#include <stdbool.h>
#include <string.h>

#include "board.h"
#include "gpio.h"
#include "timer.h"
#include "led.h"
#include "keyswitch.h"
#include "debounce.h"
#include "gpiote.h"
#include "ble_stack.h"
#include "hid_service.h"
#include "keymap.h"

int main(void)
{
	/*
	 * ---- Phase 1: Pre-SoftDevice init ----
	 * GPIO, LEDs, keys, debounce — pure hardware, no SD dependency.
	 */
	led_init();

	/* Diagnostic: 3 quick red blinks to prove we're alive.
	 * If you see these, the bootloader jumped to our code OK. */
	for (int i = 0; i < 3; i++)
	{
		led_on(LED_RED);
		for (volatile int d = 0; d < 400000; d++)
			;
		led_off(LED_RED);
		for (volatile int d = 0; d < 400000; d++)
			;
	}

	key_init();
	debounce_init();

	/*
	 * ---- Phase 2: SoftDevice + BLE ----
	 * Enables the SD (takes over RADIO, RTC0, LFCLK, etc).
	 * After this call the device is advertising as "Macro Blues".
	 */
	ble_stack_init();
	hid_service_init();

	/* All GATT services are registered — NOW start advertising.
	 * This must come after hid_service_init() because adding GATT
	 * attributes while advertising is active can silently stop the
	 * SoftDevice's radio. */
	ble_stack_advertise();

	/* Diagnostic: 3 quick blue blinks = BLE + HID init OK */
	for (int i = 0; i < 3; i++)
	{
		led_on(LED_BLUE);
		for (volatile int d = 0; d < 400000; d++)
			;
		led_off(LED_BLUE);
		for (volatile int d = 0; d < 400000; d++)
			;
	}

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
	int prev_pressed[NUM_KEYS];
	memset(prev_pressed, 0, sizeof(prev_pressed));

	/*
	 * Simplified architecture: always scan keys + debounce on every
	 * iteration.  GPIOTE is used purely as a wake source, not for
	 * state management.  The loop decides whether to busy-poll (10 ms)
	 * or deep-sleep (sd_app_evt_wait) based on direct GPIO reads.
	 *
	 * On key state change, we build an HID report with all currently
	 * pressed keys and send it as a notification.
	 */
	while (true)
	{
		ble_stack_process();

		key_scan(raw);
		debounce_update(raw);

		/* Check if any key changed state (fell or rose) */
		int changed = 0;
		for (int i = 0; i < NUM_KEYS; i++)
		{
			if (debounce_fell(i) || debounce_rose(i))
				changed = 1;
		}

		if (changed && ble_stack_connected())
		{
				/* Build a report with all currently pressed keycodes */
			uint8_t keys[6];
			uint8_t mod = 0;
			uint8_t count = 0;
			for (int i = 0; i < NUM_KEYS && count < 6; i++)
			{
				if (debounce_state(i))
				{
					mod |= keymod[i];
					keys[count++] = keymap[i];
				}
			}
			hid_service_send_report(mod, keys, count);
			led_toggle(LED_RED);
		}

		if (key_any_pressed() || debounce_settling())
		{
			/* Keys are active — poll at ~10 ms for debounce. */
			wait_ms(10);
		}
		else
		{
			/* Idle — arm GPIOTE and sleep until a key press
			 * or BLE event wakes the CPU. */
			gpiote_arm();
			ble_stack_wait();
		}
	}

	return 0;
}