#include <stdint.h>
#include <stdbool.h>
#include "led.h"
#include "pins.h"

#define GPIO_BASE_ADDRESS 0x50000000
#define GPIO_OUTSET (*(volatile uint32_t *) (GPIO_BASE_ADDRESS + 0x508))
#define GPIO_OUTCLR (*(volatile uint32_t *) (GPIO_BASE_ADDRESS + 0x50C))
#define GPIO_DIRSET (*(volatile uint32_t *) (GPIO_BASE_ADDRESS + 0x518))
#define GPIO_DIRCLR (*(volatile uint32_t *) (GPIO_BASE_ADDRESS + 0x51C))

#define GPIO_PIN_CNF(n) (*(volatile uint32_t*) ((uintptr_t)GPIO_BASE_ADDRESS + 0x700 + (n * 4)))
#define DIR_OFFSET 0
#define INPUT_OFFSET 1
#define PULL_OFFSET 2
#define DRIVE_OFFSET 8
#define SENSE_OFFSET 16

void led_init(uint32_t pin) {
    // Configure the LED pin as an output
	GPIO_PIN_CNF(pin) =	(1 << DIR_OFFSET)
					  | (1 << INPUT_OFFSET)
					  | (0 << PULL_OFFSET)
					  | (0 << DRIVE_OFFSET)
					  | (0 << SENSE_OFFSET);
	GPIO_DIRSET = (1 << pin);
	GPIO_OUTSET = (1 << pin);
}

int led_main(void) {
	led_init(BLUE_LED);
	led_init(RED_LED);
	while(true){}
}
