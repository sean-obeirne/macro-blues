#include <stdbool.h>

#include "board.h"
#include "gpio.h"
#include "timer.h"
#include "led.h"
#include "keyswitch.h"
#include "debounce.h"
#include "gpiote.h"
#include "ble_stack.h"
#include "hid_service.h"
#include "battery.h"
#include "keymap.h"
#include "i2c.h"
#include "ssd1306.h"
#include "encoder.h"

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

	/* ---- OLED display (Featherwing 128×32 via I2C) ---- */
	i2c_init();
	ssd1306_init();
	ssd1306_fill(0xFF); /* all pixels on — proof of life */

	key_init();
	debounce_init();
	encoder_init();

	/*
	 * ---- Phase 2: SoftDevice + BLE ----
	 * Enables the SD (takes over RADIO, RTC0, LFCLK, etc).
	 * After this call the device is advertising as "Macro Blues".
	 */
	ble_stack_init();
	hid_service_init();
	battery_service_init();

	/* All GATT services are registered — NOW start advertising.
	 * This must come after hid_service_init() / battery_service_init()
	 * because adding GATT attributes while advertising is active can
	 * silently stop the SoftDevice's radio. */
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
	battery_init();

	/* ---- Main loop ---- */
	led_all_off();

	int raw[NUM_KEYS];

	/* Battery check counter.  Each main-loop idle sleep is
	 * roughly 1 event period; we sample every ~3000 iterations
	 * which works out to roughly every 30-60 seconds. */
	int bat_counter = 0;
	int bat_low_blink = 0;

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

		/* Debug: blue LED reflects live "any key pressed" state.
		 * ON while one or more of the NUM_KEYS pins reads low. */
		if (key_any_pressed())
			led_on(LED_BLUE);
		else
			led_off(LED_BLUE);

		/* Rotary encoder — scroll display vertically */
		int enc = encoder_poll();
		if (enc)
			ssd1306_scroll(enc);

		/* Encoder button — toggle display on/off */
		if (encoder_btn_fell())
			ssd1306_display_toggle();

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
					if (keymap[i] != HID_KEY_NONE)
					{
						mod |= keymod[i];
						keys[count++] = keymap[i];
					}
				}
			}
			hid_service_send_report(mod, keys, count);
			led_toggle(LED_RED);
		}

		/* ---- Periodic battery check ---- */
		if (++bat_counter >= 3000)
		{
			bat_counter = 0;
			battery_update();

			/* Quick red blink if battery is low */
			if (battery_low() && !bat_low_blink)
			{
				led_flash(LED_RED, 50);
				bat_low_blink = 1;
			}
			else if (!battery_low())
			{
				bat_low_blink = 0;
			}
		}

		if (key_any_pressed() || debounce_settling())
		{
			/* Keys active — poll at ~10 ms for debounce. */
			wait_ms(10);
		}
		else
		{
			/* Idle — arm GPIOTE (drives rows low) and sleep. */
			gpiote_arm();
			ble_stack_wait();
		}
	}

	return 0;
}