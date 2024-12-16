#include "debug.h"
#include "rtc.h"
#include "led.h"


void processing(void) {
	led_on(RED);
}
void finish(void) {
	led_off(RED);
}

void success(void) {
	led_on(BLUE);
	wait_ms(50);
	toggle_led(BLUE);
	wait_ms(50);
	led_off(BLUE);
}
void clear(void) {
	led_off(BLUE);
}
void fail(void) {
	led_on(RED);
	wait_ms(50);
	toggle_led(RED);
	wait_ms(50);
	led_off(RED);
}
