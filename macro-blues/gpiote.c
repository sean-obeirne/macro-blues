#include "nrf52832.h"
#include "board.h"
#include "keyswitch.h"
#include "gpiote.h"

/*
 * gpiote.c — GPIOTE PORT event driver
 *
 * Uses the PORT event (not the 8 GPIOTE channels) so we can monitor
 * all 12 key pins with a single interrupt.  Each key pin's SENSE field
 * in PIN_CNF is set to SENSE_LOW — when any key is pressed (active-low),
 * the PORT event fires.
 *
 * The ISR disables its own interrupt to prevent an IRQ storm while a
 * key is held (PORT event is level-triggered).  The main loop calls
 * gpiote_arm() to re-enable it before going back to sleep.
 */

static volatile int event_flag;

void gpiote_init(void)
{
	/*
	 * Configure SENSE_LOW on each key pin.
	 *
	 * We only modify the SENSE field (bits 16-17) of PIN_CNF,
	 * leaving DIR, INPUT, PULL, DRIVE as already set by
	 * gpio_pin_cfg_input() during key_init().
	 *
	 * SENSE field values:
	 *   0 = disabled
	 *   2 = sense for high level
	 *   3 = sense for low level  ← what we want (active-low keys)
	 */
	for (int i = 0; i < NUM_KEYS; i++) {
		uint32_t pin = key_pins[i];
		uint32_t cnf = GPIO_PIN_CNF(pin);
		cnf &= ~(3u << PIN_CNF_SENSE);   /* clear SENSE field */
		cnf |=  (3u << PIN_CNF_SENSE);    /* SENSE_LOW = 3 */
		GPIO_PIN_CNF(pin) = cnf;
	}

	/* Default DETECTMODE (0) — combined level-based detect.
	 * All pins OR together into a single PORT event. */
	GPIO_DETECTMODE = 0;

	/* Clear any stale PORT event and LATCH bits */
	GPIO_LATCH = GPIO_LATCH;      /* write 1s to clear latched bits */
	GPIOTE_EVENTS_PORT = 0;

	/* Enable PORT event interrupt in GPIOTE peripheral (bit 31) */
	GPIOTE_INTENSET = (1u << 31);

	/* Enable GPIOTE IRQ in NVIC (IRQ 6) */
	NVIC_ICPR0 = (1u << GPIOTE_IRQN);   /* clear any pending */
	NVIC_ISER0 = (1u << GPIOTE_IRQN);   /* enable */

	event_flag = 0;
}

void gpiote_arm(void)
{
	event_flag = 0;

	/* Clear stale events so we don't wake immediately */
	GPIO_LATCH = GPIO_LATCH;
	GPIOTE_EVENTS_PORT = 0;
	NVIC_ICPR0 = (1u << GPIOTE_IRQN);

	/* Re-enable interrupt (the ISR disables it to prevent storm) */
	GPIOTE_INTENSET = (1u << 31);
	NVIC_ISER0 = (1u << GPIOTE_IRQN);
}

int gpiote_event_fired(void)
{
	if (event_flag) {
		event_flag = 0;
		return 1;
	}
	return 0;
}

/*
 * GPIOTE interrupt handler.
 *
 * Fires when any key pin goes low (SENSE_LOW → PORT event).
 * We disable the interrupt immediately to prevent an IRQ storm —
 * the PORT event is level-triggered, so it would keep firing as
 * long as the key is held.  gpiote_arm() re-enables it later.
 */
void GPIOTE_IRQHandler(void)
{
	if (GPIOTE_EVENTS_PORT) {
		GPIOTE_EVENTS_PORT = 0;
		GPIO_LATCH = GPIO_LATCH;     /* clear all latched pin flags */

		/* Disable interrupt to prevent storm while key is held.
		 * The main loop re-enables via gpiote_arm() before sleeping. */
		GPIOTE_INTENCLR = (1u << 31);

		event_flag = 1;
	}
}
