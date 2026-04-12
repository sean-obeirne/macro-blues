#ifndef GPIOTE_H
#define GPIOTE_H

/*
 * gpiote.h — GPIOTE PORT event driver (interrupt-driven key detection)
 *
 * The nRF52832 GPIOTE peripheral can fire an interrupt when any pin
 * configured with SENSE detects a level change.  We use this to wake
 * the CPU from sleep (WFE) when a key is pressed, rather than polling
 * in a busy loop.
 *
 * How it works:
 *   - gpiote_init() configures SENSE_LOW on all key pins and enables
 *     the PORT event interrupt.
 *   - When any key pin goes low (pressed), the ISR fires, sets a flag,
 *     and disables itself (to prevent repeated firing while the key is
 *     held — it's level-triggered).
 *   - The main loop calls gpiote_arm() before sleeping to re-enable
 *     the interrupt for the next wake cycle.
 *
 * Usage pattern in main loop:
 *   gpiote_arm();                      // re-enable interrupt
 *   while (!gpiote_event_fired())      // check flag
 *       __asm volatile("wfe");         // sleep until event
 *   // ... scan/debounce keys ...
 */

/* Configure SENSE_LOW on all key pins, enable PORT event IRQ. */
void gpiote_init(void);

/* Prepare for sleep: clear stale events and re-enable the interrupt.
 * Call this right before entering a WFE sleep loop. */
void gpiote_arm(void);

/* Returns 1 (and clears flag) if the GPIOTE ISR has fired since
 * the last call to gpiote_arm() or gpiote_event_fired(). */
int gpiote_event_fired(void);

/* The actual ISR — defined in gpiote.c, placed in the vector table
 * by startup.c.  Not called from user code. */
void GPIOTE_IRQHandler(void);

#endif /* GPIOTE_H */
