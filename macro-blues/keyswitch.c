#include "gpio.h"
#include "board.h"
#include "keyswitch.h"

void key_init(void)
{
	gpio_pin_cfg_input(PIN_KEY1);
	/* add more keys here as the macropad grows:
	 * gpio_pin_cfg_input(PIN_KEY2);
	 * gpio_pin_cfg_input(PIN_KEY3);
	 */
}

int key_pressed(uint32_t pin)
{
	/* active-low: pressed = 0 on the pin */
	return gpio_pin_read(pin) == 0;
}
