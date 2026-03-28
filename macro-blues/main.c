#include <stdbool.h>

#include "board.h"
#include "nrf52832.h"
#include "timer.h"
#include "led.h"
#include "keyswitch.h"
#include "debounce.h"
#include "gpiote.h"

int main(void)
{
	/* ---- hardware init ---- */
	timer_init();
	led_init();
	key_init();
	debounce_init();
	gpiote_init();     /* must come after key_init (needs pins configured) */

	/* ---- main loop ---- */
	led_all_off();

	int raw[NUM_KEYS];

	while (true)
	{
		/*
		 * IDLE: CPU sleeps via WFE until GPIOTE PORT event fires
		 * (any key pin goes low).  Current draw drops to ~1.5 µA
		 * while sleeping — critical for battery life.
		 *
		 * The WFE pattern is race-free: if the GPIOTE ISR fires
		 * between the flag check and WFE, the Cortex-M event
		 * register is set, and WFE returns immediately.
		 */
		gpiote_arm();
		while (!gpiote_event_fired())
			__WFE();

		/*
		 * ACTIVE: scan and debounce at 10 ms intervals until all
		 * keys are released and debounce counters have settled.
		 * Then go back to sleep.
		 */
		do {
			key_scan(raw);
			debounce_update(raw);

			for (int i = 0; i < NUM_KEYS; i++) {
				if (debounce_fell(i))
					led_toggle(LED_RED);
				if (debounce_rose(i))
					led_toggle(LED_BLUE);
			}

			wait_ms(10);
		} while (key_any_pressed() || debounce_settling());
	}

	return 0;
}