extern "C" {
	#include "led.h"
}

void setup() {
	led_init();
}

void loop() {
	led_main();
    // digitalWrite(LED_BUILTIN, HIGH);
    // delay(1000);
    // digitalWrite(LED_BUILTIN, LOW);
    // delay(500);
}
