#include "debug.h"
#include "rtc.h"
#include "led.h"

#define S_TO_MS(n) ((n) * 1000)

void rtc_init(void) {
	PRESCALER = 0x0000001F; // RTC count is 1ms
	// PRESCALER	= 0x00007FFF; // RTC count is 1s -- although, this specifically seems to not work
	RTC_INTENSET = 0x00000001; // turn on interrupts
	TASKS_START	= 0x00000001; // start the RTC
}


int wait(int seconds) {
	int ms = S_TO_MS(seconds);
	int ret = wait_ms(ms);
#ifdef DEBUG
	ret = wait_ms(100);
	led_off(BLUE);
#endif
	return ret;
}

int wait_ms(int milliseconds) {
	for(int ticks = 0; ticks < milliseconds; ticks++){
		while (!EVENTS_TICK);
		EVENTS_TICK = 0x0;

#ifdef DEBUG
		if (ticks % (S_TO_MS(1) / 2) == 0) { // every half-second
			toggle_led(RED);
		}
#endif
	}
#ifdef DEBUG
	led_on(BLUE);
#endif
	return 0;
}

