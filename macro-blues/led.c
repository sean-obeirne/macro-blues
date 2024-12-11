#include <stdint.h>
#include <stdbool.h>
#include "nrf.h"          // Core header for accessing nRF52 hardware
#include "nrf_gpio.h"     // GPIO functions
#include "nrf_delay.h"    // For adding delays (useful for testing)

#define LED_PIN 17        // Pin connected to LED1 (adjust based on your hardware setup)

void led_init(void) {
    // Configure the LED pin as an output
    nrf_gpio_cfg_output(LED_PIN);
}

int main(void) {
    // Initialize the GPIO subsystem
    led_init();

    // Turn on the LED by setting the pin high
    nrf_gpio_pin_set(LED_PIN);

	int cycle_num = 0;

    while (true) {

		nrf_gpio_pin_clear(LED_PIN);
		nrf_delay_ms(1000)
        // Keep the LED on indefinitely
        // Optionally add a toggle or delay here for blinking behavior
    }

    return 0; // Not typically reached in embedded systems
}
