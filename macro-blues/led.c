#include <stdint.h>
#include <stdbool.h>
#include "led.h"
#include "gpio.h"
#include "pins.h"

void led_init(uint32_t pin) {
    // Configure the LED pin as an output
	GPIO_PIN_CNF(pin) =	(1 << DIR_OFFSET)
					  | (1 << INPUT_OFFSET)
					  | (0 << PULL_OFFSET)
					  | (0 << DRIVE_OFFSET)
					  | (0 << SENSE_OFFSET);
	GPIO_DIRSET = (1 << pin);
}

void led_on(uint32_t pin){
	GPIO_OUTSET = (1 << pin);
}
void led_off(uint32_t pin){
	GPIO_OUTSET = (0 << pin);
}

int led_main(void) {
	led_init(BLUE_LED);
	led_init(RED_LED);

	led_on(RED_LED);
	led_on(BLUE_LED);

	return 0;
}
