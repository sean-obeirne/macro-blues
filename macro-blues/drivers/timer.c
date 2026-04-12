#include "nrf52832.h"
#include "timer.h"

/*
 * timer.c — Millisecond timing via RTC1
 *
 * We use RTC1 (not RTC0) because the SoftDevice owns RTC0.
 * LFCLK is started by the SoftDevice when sd_softdevice_enable()
 * is called, so we don't start it here.  Call ble_stack_init()
 * before timer_init().
 *
 * We do NOT enable the RTC1 tick interrupt (INTENSET) — we poll
 * the EVENTS_TICK register directly.  This avoids spurious
 * wake-ups from sd_app_evt_wait() while the CPU is sleeping.
 */

static void rtc1_start(void)
{
	RTC1_PRESCALER    = 0x0000001F;   /* ~1 ms tick (32768 / 32 ≈ 1024 Hz) */
	RTC1_EVTENSET     = 1;            /* enable TICK event (bit 0) */
	RTC1_TASKS_START  = 0x00000001;
}

/* ---- public API ---- */

void timer_init(void)
{
	rtc1_start();
}

int wait_ms(int ms)
{
	for (int i = 0; i < ms; i++) {
		while (!RTC1_EVENTS_TICK)
			;
		RTC1_EVENTS_TICK = 0x0;
	}
	return 0;
}

int wait(int seconds)
{
	return wait_ms(seconds * 1000);
}
