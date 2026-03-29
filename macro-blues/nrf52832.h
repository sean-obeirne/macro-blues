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

/*
 * CLOCK and RTC0 are owned by the SoftDevice — do not access directly.
 * LFCLK is started by sd_softdevice_enable().  Use RTC1 for app timing.
 */

/* ---- RTC1 (application timer) ---- */
#define RTC1_BASE 0x40011000

#define RTC1_TASKS_START (*(volatile uint32_t *)(RTC1_BASE + 0x000))
#define RTC1_TASKS_STOP  (*(volatile uint32_t *)(RTC1_BASE + 0x004))
#define RTC1_TASKS_CLEAR (*(volatile uint32_t *)(RTC1_BASE + 0x008))

#define RTC1_EVENTS_TICK (*(volatile uint32_t *)(RTC1_BASE + 0x100))

#define RTC1_INTENSET  (*(volatile uint32_t *)(RTC1_BASE + 0x304))
#define RTC1_INTENCLR  (*(volatile uint32_t *)(RTC1_BASE + 0x308))
#define RTC1_EVTENSET  (*(volatile uint32_t *)(RTC1_BASE + 0x344))
#define RTC1_COUNTER   (*(volatile uint32_t *)(RTC1_BASE + 0x504))
#define RTC1_PRESCALER (*(volatile uint32_t *)(RTC1_BASE + 0x508))

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

/* ---- GPIOTE ---- */
#define GPIOTE_BASE 0x40006000

#define GPIOTE_EVENTS_PORT (*(volatile uint32_t *)(GPIOTE_BASE + 0x17C))
#define GPIOTE_INTENSET    (*(volatile uint32_t *)(GPIOTE_BASE + 0x304))
#define GPIOTE_INTENCLR    (*(volatile uint32_t *)(GPIOTE_BASE + 0x308))

#define GPIOTE_IRQN 6  /* NVIC IRQ number for GPIOTE */

/* GPIO PORT event helpers (used with GPIOTE PORT event) */
#define GPIO_LATCH       (*(volatile uint32_t *)(GPIO_BASE + 0x520))
#define GPIO_DETECTMODE  (*(volatile uint32_t *)(GPIO_BASE + 0x524))

/* ---- NVIC (Nested Vectored Interrupt Controller) ---- */
#define NVIC_ISER0 (*(volatile uint32_t *)0xE000E100)  /* Interrupt Set-Enable  */
#define NVIC_ICER0 (*(volatile uint32_t *)0xE000E180)  /* Interrupt Clear-Enable */
#define NVIC_ICPR0 (*(volatile uint32_t *)0xE000E280)  /* Interrupt Clear-Pending */

/* Set IRQ priority.  Cortex-M4 nRF52 implements 3 priority bits (0-7),
 * stored in the top 3 bits of an 8-bit register.  SoftDevice reserves
 * priorities 0, 1, and 4.  Application code should use 2, 3, 5, 6, or 7. */
#define NVIC_SET_PRIORITY(irqn, prio) \
	(*(volatile uint8_t *)(0xE000E400 + (irqn)) = (uint8_t)((prio) << 5))

#endif /* NRF52832_H */
