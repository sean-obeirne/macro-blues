#include "gpio.h"
#include "board.h"
#include "keyswitch.h"

/*
 * Pin table — order matches key numbering (KEY1 = index 0, etc).
 * This lives in flash (.rodata) and is shared via keyswitch.h so
 * other modules (debounce, GPIOTE) can iterate over the pins.
 */
const uint32_t key_pins[NUM_KEYS] = {
	PIN_KEY1,  PIN_KEY2,  PIN_KEY3,  PIN_KEY4,
	PIN_KEY5,  PIN_KEY6,  PIN_KEY7,  PIN_KEY8,
	PIN_KEY9,  PIN_KEY10, PIN_KEY11, PIN_KEY12,
};

void key_init(void)
{
	for (int i = 0; i < NUM_KEYS; i++)
		gpio_pin_cfg_input(key_pins[i]);
}

int key_pressed(uint32_t pin)
{
	/* active-low: pressed = 0 on the pin */
	return gpio_pin_read(pin) == 0;
}

void key_scan(int *state)
{
	for (int i = 0; i < NUM_KEYS; i++)
		state[i] = key_pressed(key_pins[i]);
}

int key_any_pressed(void)
{
	for (int i = 0; i < NUM_KEYS; i++)
		if (key_pressed(key_pins[i]))
			return 1;
	return 0;
}
