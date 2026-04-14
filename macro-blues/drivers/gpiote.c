#include "nrf52832.h"
#include "board.h"
#include "keyswitch.h"
#include "gpiote.h"
#include "ble_stack.h"
#include "encoder.h"

/*
 * gpiote.c — GPIOTE PORT event driver
 *
 * Uses the PORT event (not the 8 GPIOTE channels) so we can monitor
 * all 12 key pins with a single interrupt.  Each key pin's SENSE field
 * in PIN_CNF is set to SENSE_LOW — when any key is pressed (active-low),
 * the PORT event fires.
 *
 * In DETECTMODE=0 (default), the PORT event fires on the 0→1 edge of
 * the combinatorial DETECT signal (OR of all SENSE matches).  This
 * means it fires ONCE when the first key goes low, and won't re-fire
 * until all keys release (DETECT→0) and another is pressed (DETECT→1).
 * No IRQ storm, no need to disable/re-enable the interrupt.
 *
 * Important:
 *   - We do NOT clear GPIO_LATCH anywhere.  In Default DETECTMODE the
 *     DETECT signal is purely combinatorial — LATCH is just a record
 *     and does not affect DETECT or PORT events.  The nRF5 SDK driver
 *     follows this same pattern.  Clearing LATCH is unnecessary and
 *     can interact badly with Errata 173 (write propagation delay)
 *     and Errata 210 (spurious LATCH bits).
 *   - After clearing EVENTS_PORT we read the register back and issue
 *     a DSB to ensure the peripheral has actually dropped the IRQ line
 *     before we clear the NVIC pending bit.
 */

static volatile int event_flag;

void gpiote_init(void)
{
	/*
	 * Configure SENSE_LOW on each key pin.
	 *
	 * Errata 210 workaround: writing PIN_CNF with INPUT=Connected
	 * and SENSE≠Disabled in the SAME register write can spuriously
	 * set LATCH bits.  We avoid this by first writing with SENSE
	 * disabled, then setting SENSE in a separate write.
	 *
	 * key_init() already configured INPUT=0 (connected), so SENSE is
	 * the only field we're changing — but the hardware sees the full
	 * register write, so we still need the two-step sequence.
	 */
	for (int i = 0; i < NUM_KEYS; i++) {
		uint32_t pin = key_pins[i];
		uint32_t cnf = GPIO_PIN_CNF(pin);

		/* Step 1: ensure SENSE=Disabled (INPUT already 0) */
		cnf &= ~(3u << PIN_CNF_SENSE);
		GPIO_PIN_CNF(pin) = cnf;

		/* Step 2: set SENSE_LOW in a separate write */
		cnf |= (3u << PIN_CNF_SENSE);   /* SENSE_LOW = 3 */
		GPIO_PIN_CNF(pin) = cnf;
	}

	/* Default DETECTMODE (0) — combinatorial level-based detect.
	 * PORT fires on the 0→1 edge of DETECT (no storm). */
	GPIO_DETECTMODE = 0;

	/* Clear any stale PORT event with read-back barrier */
	GPIOTE_EVENTS_PORT = 0;
	(void)GPIOTE_EVENTS_PORT;   /* read-back: wait for peripheral */
	__asm volatile ("dsb" ::: "memory");

	/* Enable PORT event interrupt in GPIOTE peripheral (bit 31) */
	GPIOTE_INTENSET = (1u << 31);

	/* Enable GPIOTE IRQ in NVIC (IRQ 6) at app-level priority.
	 * Must use sd_nvic_* wrappers — direct NVIC writes are ignored
	 * when the SoftDevice is active. */
	sd_nvic_irq_set_priority(GPIOTE_IRQN, 7);
	sd_nvic_irq_clear_pending(GPIOTE_IRQN);
	sd_nvic_irq_enable(GPIOTE_IRQN);

	event_flag = 0;
}

void gpiote_arm(void)
{
	/*
	 * Clear stale PORT event so sd_app_evt_wait() doesn't return
	 * immediately.  The read-back + DSB ensures the peripheral has
	 * dropped the IRQ line before we clear the NVIC pending bit.
	 */
	GPIOTE_EVENTS_PORT = 0;
	(void)GPIOTE_EVENTS_PORT;              /* read-back barrier */
	__asm volatile ("dsb" ::: "memory");   /* complete all writes */
	sd_nvic_irq_clear_pending(GPIOTE_IRQN);

	event_flag = 0;
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
 * Fires once when DETECT transitions 0→1 (any key pressed).
 * We clear EVENTS_PORT with a read-back barrier (required by the
 * nRF52 peripheral bus) but do NOT clear LATCH — it is not needed
 * in Default DETECTMODE and the SDK driver follows the same pattern.
 */
void GPIOTE_IRQHandler(void)
{
	/* Encoder channels — service first for lowest latency */
	if (GPIOTE_EVENTS_IN(0))
	{
		GPIOTE_EVENTS_IN(0) = 0;
		encoder_isr_update();
	}
	if (GPIOTE_EVENTS_IN(1))
	{
		GPIOTE_EVENTS_IN(1) = 0;
		encoder_isr_update();
	}

	if (GPIOTE_EVENTS_PORT) {
		GPIOTE_EVENTS_PORT = 0;
		(void)GPIOTE_EVENTS_PORT;   /* read-back barrier */
		__asm volatile ("dsb" ::: "memory");
		event_flag = 1;
	}
}
