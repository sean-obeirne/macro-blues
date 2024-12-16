#include <stdint.h>

#include "gpio.h"

void out_pin_init(uint32_t pin) {
    // Configure pin as an output
	GPIO_PIN_CNF(pin) =	(1 << DIR_OFFSET)
					  | (1 << INPUT_OFFSET)
					  | (0 << PULL_OFFSET)
					  | (0 << DRIVE_OFFSET)
					  | (0 << SENSE_OFFSET);
	GPIO_DIRSET = (1 << pin);
}

void in_pin_init(uint32_t pin) {
    // Configure pin as an input
	GPIO_PIN_CNF(pin) =	(0 << DIR_OFFSET)
					  | (0 << INPUT_OFFSET)
					  | (3 << PULL_OFFSET)
					  | (0 << DRIVE_OFFSET)
					  | (0 << SENSE_OFFSET);
	GPIO_DIRCLR = (1 << pin);

}

