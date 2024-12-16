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
	for(int ticks = 0; ticks < S_TO_MS(seconds); ticks++){
		while (!EVENTS_TICK);
		EVENTS_TICK = 0x0;

#ifdef DEBUG
		if (ticks % S_TO_MS(1) == 0) 
			toggle_led(RED);
#endif
	}
#ifdef DEBUG
	led_on(BLUE);
#endif
	return 0;
}

int wait_ms(int milliseconds) {
	for(int ticks = 0; ticks < milliseconds; ticks++){
		while (!EVENTS_TICK);
		EVENTS_TICK = 0x0;

#ifdef DEBUG
		if (ticks % S_TO_MS(1) == 0) 
			toggle_led(RED);
#endif
	}
#ifdef DEBUG
	led_on(BLUE);
#endif
	return 0;
}

int rtc_main(void) {
	rtc_init();
	return 0;
}
