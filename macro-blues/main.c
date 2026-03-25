#include <stdbool.h>

#include "board.h"
#include "timer.h"
#include "led.h"
#include "keyswitch.h"

int main(void)
{
	/* ---- hardware init ---- */
	timer_init();
	led_init();
	key_init();

	/* ---- main loop ---- */
	led_all_off();

	while (true)
	{
		if (key_pressed(PIN_KEY1))
		{
			led_toggle(LED_RED);
			wait_ms(200); /* simple debounce */
		}
	}

	return 0;
}