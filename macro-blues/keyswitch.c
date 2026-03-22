#include <stdint.h>
#include <stdbool.h>

#include "debug.h"
#include "gpio.h"
#include "led.h"
#include "keyswitch.h"

void gpio_init(void)
{
	in_pin_init(7);
}

int check_input(uint32_t pin)
{
	return (GPIO_IN >> pin) & 1;
}

int key_main(void)
{
	gpio_init();

	kill();
	while (true)
	{
		if (check_input(7) == 0)
		{
			toggle_led(RED);
		}
	}

	return 0;
}
