#ifndef GPIO_H
#define GPIO_H

/*
 * gpio.h — Low-level GPIO pin configuration and access
 *
 * Thin abstraction over the nRF52832 GPIO peripheral.
 * Higher-level modules (led, keyswitch) call these instead of
 * touching registers directly.
 */

#include <stdint.h>

void gpio_pin_cfg_output(uint32_t pin);
void gpio_pin_cfg_input(uint32_t pin);
/* Configure pin as input with pull-up and sense-low wakeup.
 * Used before entering System OFF to wake on button press. */
void gpio_pin_cfg_sense_low(uint32_t pin);

void gpio_pin_set(uint32_t pin);
void gpio_pin_clear(uint32_t pin);
int gpio_pin_read(uint32_t pin);

#endif /* GPIO_H */
