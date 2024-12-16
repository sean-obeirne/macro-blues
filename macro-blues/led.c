#include <stdint.h>
#include <stdbool.h>
#include "led.h"
#include "gpio.h"
#include "pins.h"

void led_init() {
	out_pin_init(RED_LED);
	out_pin_init(BLUE_LED);
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

