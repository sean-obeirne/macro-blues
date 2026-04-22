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

static volatile int btn_fell;

void encoder_init(void)
{
    /* Configure encoder pins as inputs with pull-ups.
     * The comment in the old code claimed key_init() did this, but
     * key_init() only touches the matrix pins.  Without the input
     * buffer connected the GPIOTE channels never fire. */
    gpio_pin_cfg_input(PIN_ENC_A);
    gpio_pin_cfg_input(PIN_ENC_B);
    gpio_pin_cfg_input(PIN_ENC_BTN);

    prev_state = read_state();
    position = 0;
    btn_fell = 0;

    /* Configure GPIOTE channel 0 for ENC_A, channel 1 for ENC_B.
     * Both in event mode, toggle polarity (fires on any edge). */
    GPIOTE_CONFIG(0) = GPIOTE_CONFIG_MODE_EVENT | ((uint32_t)PIN_ENC_A << 8) | GPIOTE_CONFIG_POL_TOGGLE;
    GPIOTE_CONFIG(1) = GPIOTE_CONFIG_MODE_EVENT | ((uint32_t)PIN_ENC_B << 8) | GPIOTE_CONFIG_POL_TOGGLE;

    /* Configure GPIOTE channel 2 for ENC_BTN: HiToLo = press.
     * Active-low button: idle=high, pressed=low. */
    GPIOTE_CONFIG(2) = GPIOTE_CONFIG_MODE_EVENT | ((uint32_t)PIN_ENC_BTN << 8) | (2u << 16);

    /* Clear stale events */
    GPIOTE_EVENTS_IN(0) = 0;
    GPIOTE_EVENTS_IN(1) = 0;
    GPIOTE_EVENTS_IN(2) = 0;

    /* Enable IN[0], IN[1], and IN[2] interrupts */
    GPIOTE_INTENSET = (1u << 0) | (1u << 1) | (1u << 2);
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

void encoder_btn_isr_update(void)
{
    btn_fell = 1;
}

int encoder_btn_fell(void)
{
    if (btn_fell)
    {
        btn_fell = 0;
        return 1;
    }
    return 0;
}
