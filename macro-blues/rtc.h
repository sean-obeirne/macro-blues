#ifndef RTC_H
#define RTC_H

#include <stdint.h>

// Base address for RTC
#define RTC0_BASE_ADDRESS 0x4000B000

// Real-time counter (RTC) register definitions //
#define TASKS_START			(*(volatile uint32_t *) (RTC0_BASE_ADDRESS + 0x000))	// start RTC
#define TASKS_STOP			(*(volatile uint32_t *) (RTC0_BASE_ADDRESS + 0x004))	// stop RTC
#define TASKS_CLEAR			(*(volatile uint32_t *) (RTC0_BASE_ADDRESS + 0x008))	// clear RTC
#define TASKS_TRIGOVRFLW	(*(volatile uint32_t *) (RTC0_BASE_ADDRESS + 0x00C))	// trigger RTC overflow

#define EVENTS_TICK		(*(volatile uint32_t *) (RTC0_BASE_ADDRESS + 0x100))	// COUNTER increment
#define EVENTS_OVRFLW	(*(volatile uint32_t *) (RTC0_BASE_ADDRESS + 0x104))	// COUNTER overflow

#define RTC_INTENSET	(*(volatile uint32_t *) (RTC0_BASE_ADDRESS + 0x304))	// enable interrupts
#define RTC_INTENCLR	(*(volatile uint32_t *) (RTC0_BASE_ADDRESS + 0x308))	// disable interrupts
#define COUNTER			(*(volatile uint32_t *) (RTC0_BASE_ADDRESS + 0x504))	// current RTC counts
#define PRESCALER		(*(volatile uint32_t *) (RTC0_BASE_ADDRESS + 0x508))	// scalar to change

int rtc_main(void);
void rtc_init(void);
int wait(int seconds);
int wait_ms(int milliseconds);

#endif

