#ifndef LED_H
#define LED_H
#include <stdint.h>

void led_init(uint32_t pin);
int led_main(void);
void turn_on_led(void);
void turn_off_led(void);

#endif
