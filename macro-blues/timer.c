#include "nrf52832.h"
#include "timer.h"

/* ---- internal helpers ---- */

static void lfclk_start(void)
{
    LFCLKSRC = 0x00000000; /* RC oscillator */
    TASKS_LFCLKSTART = 0x00000001;
    while (!EVENTS_LFCLKSTARTED)
    {
    }
    EVENTS_LFCLKSTARTED = 0x0;
}

static void rtc_start(void)
{
    RTC_PRESCALER = 0x0000001F; /* ~1 ms tick (32768 / (31+1) ≈ 1024 Hz) */
    RTC_INTENSET = 0x00000001;  /* enable TICK event */
    RTC_TASKS_START = 0x00000001;
}

/* ---- public API ---- */

void timer_init(void)
{
    lfclk_start();
    rtc_start();
}

int wait_ms(int ms)
{
    for (int i = 0; i < ms; i++)
    {
        while (!RTC_EVENTS_TICK)
        {
        }
        RTC_EVENTS_TICK = 0x0;
    }
    return 0;
}

int wait(int seconds)
{
    return wait_ms(seconds * 1000);
}
