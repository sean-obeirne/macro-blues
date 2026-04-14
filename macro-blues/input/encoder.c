#include "nrf52832.h"
#include "gpio.h"
#include "board.h"
#include "encoder.h"

/*
 * Rotary encoder on pins A1 (signal A) and A2 (signal B).
 * Both are active-low with internal pull-ups (configured by key_init()).
 *
 * Interrupt-driven via GPIOTE channels 0 (ENC_A) and 1 (ENC_B),
 * each configured for toggle (both-edge) mode.  On any edge the
 * ISR runs the gray-code state machine so no transitions are lost,
 * even during sleep or the 10 ms debounce wait.
 *
 * Gray code sequence (one detent, CW):
 *   AB: 11 → 01 → 00 → 10 → 11
 */

static const int8_t quad_table[4][4] = {
    /*            cur: 00  01  10  11  */
    /* prev 00 */ {0, +1, -1, 0},
    /* prev 01 */ {-1, 0, 0, +1},
    /* prev 10 */ {+1, 0, 0, -1},
    /* prev 11 */ {0, -1, +1, 0},
};

static uint8_t prev_state;
static volatile int position;

static uint8_t read_state(void)
{
    int a = gpio_pin_read(PIN_ENC_A);
    int b = gpio_pin_read(PIN_ENC_B);
    return (uint8_t)((a << 1) | b);
}

void encoder_init(void)
{
    prev_state = read_state();
    position = 0;

    /* Configure GPIOTE channel 0 for ENC_A, channel 1 for ENC_B.
     * Both in event mode, toggle polarity (fires on any edge). */
    GPIOTE_CONFIG(0) = GPIOTE_CONFIG_MODE_EVENT | ((uint32_t)PIN_ENC_A << 8) | GPIOTE_CONFIG_POL_TOGGLE;

    GPIOTE_CONFIG(1) = GPIOTE_CONFIG_MODE_EVENT | ((uint32_t)PIN_ENC_B << 8) | GPIOTE_CONFIG_POL_TOGGLE;

    /* Clear stale events */
    GPIOTE_EVENTS_IN(0) = 0;
    GPIOTE_EVENTS_IN(1) = 0;

    /* Enable IN[0] and IN[1] interrupts (bits 0 and 1) */
    GPIOTE_INTENSET = (1u << 0) | (1u << 1);
}

void encoder_isr_update(void)
{
    uint8_t cur = read_state();
    if (cur != prev_state)
    {
        position += quad_table[prev_state][cur];
        prev_state = cur;
    }
}

int encoder_poll(void)
{
    /* Drain all ISR-accumulated transitions */
    int steps = position;
    if (steps)
        position = 0;
    return steps;
}
