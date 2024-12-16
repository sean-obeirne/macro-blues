#include <stdint.h>
#include <stdbool.h>
#include "led.h"
#include "gpio.h"
#include "pins.h"

void pin_init(uint32_t pin) {
    // Configure pin as an output
	GPIO_PIN_CNF(pin) =	(1 << DIR_OFFSET)
					  | (1 << INPUT_OFFSET)
					  | (0 << PULL_OFFSET)
					  | (0 << DRIVE_OFFSET)
					  | (0 << SENSE_OFFSET);
	GPIO_DIRSET = (1 << pin);
}

void led_init() {
	pin_init(RED_LED);
	pin_init(BLUE_LED);
}

void led_on(uint32_t pin){
	GPIO_OUTSET = (1 << pin);
}
void led_off(uint32_t pin){
	GPIO_OUTCLR = (1 << pin);
}

int is_led_on(uint32_t pin){
	return (GPIO_OUT & (1 << pin)) != 0; // is pin high?
}

void toggle_led(uint32_t pin){
	if (is_led_on(pin))
		led_off(pin);
	else
		led_on(pin);
}

