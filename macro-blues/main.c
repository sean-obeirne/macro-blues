// #include "debug.h"
#include "led.h"
#include "clock.h"
#include "rtc.h"
#include "keyswitch.h"


int main() {
	led_init();
	clock_init();
	rtc_init();
	key_main();

	return 0;
}
