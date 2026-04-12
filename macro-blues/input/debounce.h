#ifndef DEBOUNCE_H
#define DEBOUNCE_H

/*
 * debounce.h — Per-key debounce filter
 *
 * Mechanical switches "bounce" — the contacts open and close several
 * times over a few milliseconds before settling.  Without filtering,
 * a single press looks like a rapid burst of press/release events.
 *
 * Algorithm:
 *   Each key has a small counter.  Every scan cycle (call to
 *   debounce_update), if the raw GPIO state disagrees with the
 *   accepted state, the counter increments.  Once it reaches
 *   DEBOUNCE_CYCLES the accepted state flips and we flag a
 *   press or release event.  If the raw state agrees, the
 *   counter resets to 0.
 *
 * Usage:
 *   1. debounce_init()            — call once at startup
 *   2. debounce_update(raw[])     — call every scan cycle (~10 ms)
 *   3. debounce_state(i)          — clean debounced state (1=down)
 *   4. debounce_fell(i) / _rose(i) — edge events since last update
 */

#include "board.h"

/* Number of consecutive agreeing samples required to change state.
 * At a 10 ms scan rate, 5 cycles = 50 ms of debounce time. */
#define DEBOUNCE_CYCLES 5

void debounce_init(void);

/* Feed raw[] array from key_scan().  Call once per scan cycle. */
void debounce_update(const int raw[NUM_KEYS]);

/* Debounced (clean) key state: 1 = pressed, 0 = released */
int debounce_state(int key);

/* Edge detectors — true only on the scan cycle the transition happened.
 *   fell = just pressed  (went from released → pressed, active-low "fell")
 *   rose = just released (went from pressed → released)  */
int debounce_fell(int key);
int debounce_rose(int key);

/* Returns 1 if any key's debounce counter is non-zero, meaning a
 * transition is still being validated and we must keep scanning. */
int debounce_settling(void);

#endif /* DEBOUNCE_H */
