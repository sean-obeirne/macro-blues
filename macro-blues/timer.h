#ifndef TIMER_H
#define TIMER_H

/*
 * timer.h — Clock and RTC timing services
 *
 * Handles low-frequency clock startup and provides millisecond-level
 * delays via RTC0.  Everything "time" lives here.
 */

void timer_init(void); /* start LFCLK + configure RTC0 */
int wait_ms(int ms);   /* busy-wait for `ms` milliseconds */
int wait(int seconds); /* convenience: wait whole seconds */

#endif /* TIMER_H */
