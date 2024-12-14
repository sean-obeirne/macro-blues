#ifndef LED_H
#define LED_H
#include <stdint.h>

void led_init(uint32_t pin);
int led_main(void);
void led_on(uint32_t pin);
void led_off(uint32_t pin);

#endif
