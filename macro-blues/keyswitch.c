#include <stdint.h>
#include <stdbool.h>

#include "debug.h"
#include "gpio.h"

void gpio_init(void) {
	in_pin_init(7);
}

int check_input(uint32_t pin){
	return (GPIO_IN >> pin) & 1;
}

int key_main(void) {
    gpio_init();

    while (true) {
		if (check_input(7) == 0) {
			success();
		}
		else {
			fail();
		}
    }

    return 0;
}
