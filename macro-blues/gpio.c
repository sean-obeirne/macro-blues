#include "nrf52832.h"
#include "gpio.h"

void gpio_pin_cfg_output(uint32_t pin)
{
	GPIO_PIN_CNF(pin) = (1 << PIN_CNF_DIR) | (1 << PIN_CNF_INPUT) /* disconnect input buffer */
						| (0 << PIN_CNF_PULL) | (0 << PIN_CNF_DRIVE) | (0 << PIN_CNF_SENSE);
	GPIO_DIRSET = (1 << pin);
}

void gpio_pin_cfg_input(uint32_t pin)
{
	GPIO_PIN_CNF(pin) = (0 << PIN_CNF_DIR) | (0 << PIN_CNF_INPUT) /* connect input buffer */
						| (3 << PIN_CNF_PULL)					  /* pull-up */
						| (0 << PIN_CNF_DRIVE) | (0 << PIN_CNF_SENSE);
	GPIO_DIRCLR = (1 << pin);
}

void gpio_pin_set(uint32_t pin)
{
	GPIO_OUTSET = (1 << pin);
}

void gpio_pin_clear(uint32_t pin)
{
	GPIO_OUTCLR = (1 << pin);
}

int gpio_pin_read(uint32_t pin)
{
	return (GPIO_IN >> pin) & 1;
}
