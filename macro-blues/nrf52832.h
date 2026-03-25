#ifndef NRF52832_H
#define NRF52832_H

/*
 * nrf52832.h — Peripheral register definitions for the nRF52832
 *
 * All hardware register addresses live here. Individual modules (gpio, timer,
 * led, etc.) include this single file instead of each defining their own
 * register maps. Keeps the hardware layer in one place so it's easy to
 * cross-reference against the datasheet.
 */

#include <stdint.h>

/* ---- CLOCK ---- */
#define CLOCK_BASE 0x40000000

#define TASKS_LFCLKSTART (*(volatile uint32_t *)(CLOCK_BASE + 0x008))
#define TASKS_LFCLKSTOP (*(volatile uint32_t *)(CLOCK_BASE + 0x00C))

#define EVENTS_LFCLKSTARTED (*(volatile uint32_t *)(CLOCK_BASE + 0x104))

#define CLOCK_INTENSET (*(volatile uint32_t *)(CLOCK_BASE + 0x304))
#define CLOCK_INTENCLR (*(volatile uint32_t *)(CLOCK_BASE + 0x308))
#define LFCLKRUN (*(volatile uint32_t *)(CLOCK_BASE + 0x408))
#define LFCLKSTAT (*(volatile uint32_t *)(CLOCK_BASE + 0x418))
#define LFCLKSRC (*(volatile uint32_t *)(CLOCK_BASE + 0x518))

/* ---- RTC0 ---- */
#define RTC0_BASE 0x4000B000

#define RTC_TASKS_START (*(volatile uint32_t *)(RTC0_BASE + 0x000))
#define RTC_TASKS_STOP (*(volatile uint32_t *)(RTC0_BASE + 0x004))
#define RTC_TASKS_CLEAR (*(volatile uint32_t *)(RTC0_BASE + 0x008))
#define RTC_TASKS_TRIGOVRFLW (*(volatile uint32_t *)(RTC0_BASE + 0x00C))

#define RTC_EVENTS_TICK (*(volatile uint32_t *)(RTC0_BASE + 0x100))
#define RTC_EVENTS_OVRFLW (*(volatile uint32_t *)(RTC0_BASE + 0x104))

#define RTC_INTENSET (*(volatile uint32_t *)(RTC0_BASE + 0x304))
#define RTC_INTENCLR (*(volatile uint32_t *)(RTC0_BASE + 0x308))
#define RTC_COUNTER (*(volatile uint32_t *)(RTC0_BASE + 0x504))
#define RTC_PRESCALER (*(volatile uint32_t *)(RTC0_BASE + 0x508))

/* ---- GPIO ---- */
#define GPIO_BASE 0x50000000

#define GPIO_OUT (*(volatile uint32_t *)(GPIO_BASE + 0x504))
#define GPIO_OUTSET (*(volatile uint32_t *)(GPIO_BASE + 0x508))
#define GPIO_OUTCLR (*(volatile uint32_t *)(GPIO_BASE + 0x50C))
#define GPIO_IN (*(volatile uint32_t *)(GPIO_BASE + 0x510))
#define GPIO_DIR (*(volatile uint32_t *)(GPIO_BASE + 0x514))
#define GPIO_DIRSET (*(volatile uint32_t *)(GPIO_BASE + 0x518))
#define GPIO_DIRCLR (*(volatile uint32_t *)(GPIO_BASE + 0x51C))

#define GPIO_PIN_CNF(n) (*(volatile uint32_t *)((uintptr_t)GPIO_BASE + 0x700 + ((n) * 4)))

/* PIN_CNF bit-field offsets */
#define PIN_CNF_DIR 0
#define PIN_CNF_INPUT 1
#define PIN_CNF_PULL 2
#define PIN_CNF_DRIVE 8
#define PIN_CNF_SENSE 16

#endif /* NRF52832_H */
