#ifndef CLOCK_H
#define CLOCK_H

#include <stdint.h>

// Base address for GPIO
#define CLOCK_BASE_ADDRESS 0x40000000

// We will be using the LFCLK to save power
// Callibration is doable here but not necessary
// Thus those registers are commented out below

// Clock register definitions //
#define TASKS_LFCLKSTART	(*(volatile uint32_t *) (CLOCK_BASE_ADDRESS + 0x008))	// start clock
#define TASKS_LFCLKSTOP		(*(volatile uint32_t *) (CLOCK_BASE_ADDRESS + 0x00c))	// stop clock

#define EVENTS_LFCLKSTARTED (*(volatile uint32_t *) (CLOCK_BASE_ADDRESS + 0x104))	// clock has been started!

#define INTENSET	(*(volatile uint32_t *) (CLOCK_BASE_ADDRESS + 0x304))	// enable interrupts
#define INTENCLR	(*(volatile uint32_t *) (CLOCK_BASE_ADDRESS + 0x308))	// disable interrupts
#define LFCLKRUN	(*(volatile uint32_t *) (CLOCK_BASE_ADDRESS + 0x408))	// status on if START has been triggered
#define LFCLKSTAT	(*(volatile uint32_t *) (CLOCK_BASE_ADDRESS + 0x418))	// status of LFCLK
#define LFCLKSRC	(*(volatile uint32_t *) (CLOCK_BASE_ADDRESS + 0x518))	// configure source of LFCLK
////////////////////////////////////////////////////////////////////////////////

// Callibration register definitions //
// #define TASKS_CAL		(*(volatile uint32_t *) (CLOCK_BASE_ADDRESS + 0x010))	// begin callibration
// #define TASKS_CTSTART	(*(volatile uint32_t *) (CLOCK_BASE_ADDRESS + 0x014))	// start callibration timer
// #define TASKS_CTSTOP		(*(volatile uint32_t *) (CLOCK_BASE_ADDRESS + 0x018))	// stop callibration timer
//
// #define EVENTS_DONE	(*(volatile uint32_t *) (CLOCK_BASE_ADDRESS + 0x10C))	// callibration complete
// #define EVENTS_CTTO	(*(volatile uint32_t *) (CLOCK_BASE_ADDRESS + 0x110))	// callibrartion timeout
////////////////////////////////////////////////////////////////////////////////


void clock_init(void);
int clock_main(void);


#endif

