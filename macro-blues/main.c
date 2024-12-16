// #include "debug.h"
#include "led.h"
#include "clock.h"
#include "rtc.h"


int main() {
	led_main();
	clock_main();
	rtc_main();

	wait(10);
	// wait_ms(10000);

	return 0;
}
