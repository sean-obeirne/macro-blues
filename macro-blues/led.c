#include "nrf52832.h"
#include "gpio.h"
#include "timer.h"
#include "led.h"

void led_init(void)
{
	gpio_pin_cfg_output(PIN_LED_RED);
	gpio_pin_cfg_output(PIN_LED_BLUE);

	led_all_off();
}

void led_on(uint32_t pin)
{
	gpio_pin_set(pin);
}

void led_off(uint32_t pin)
{
	gpio_pin_clear(pin);
}

int led_is_on(uint32_t pin)
{
	return (GPIO_OUT & (1 << pin)) != 0;
}

void led_toggle(uint32_t pin)
{
	if (led_is_on(pin))
		led_off(pin);
	else
		led_on(pin);
}

void led_all_off(void)
{
	led_off(PIN_LED_RED);
	led_off(PIN_LED_BLUE);
}

void led_flash(uint32_t pin, int ms)
{
	led_on(pin);
	wait_ms(ms);
	led_off(pin);
}
