#include "clock.h"

void clock_init(void) {
	LFCLKSRC = 0x00000000;			// use RC oscillator
	TASKS_LFCLKSTART = 0x00000001;	// start LF clock
	while (!EVENTS_LFCLKSTARTED) {}	// wait for clock start
	EVENTS_LFCLKSTARTED = 0x0;		// reset event after clock starts
}

