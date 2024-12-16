#ifndef LED_H
#define LED_H
#include <stdint.h>

#define RED 17
#define BLUE 19

void led_init(void);
void led_on(uint32_t pin);
void led_off(uint32_t pin);
int is_led_on(uint32_t pin);
void toggle_led(uint32_t pin);

#endif
