#include "debug.h"
#include "rtc.h"
#include "led.h"

void processing(void)
{
	led_on(RED);
}
void finish(void)
{
	led_off(RED);
}

void success(void)
{
	led_on(BLUE);
	wait_ms(5000);
	led_off(BLUE);
}
void clear(void)
{
	led_off(BLUE);
}
void fail(void)
{
	led_on(RED);
	wait_ms(5000);
	led_off(RED);
}

void kill(void)
{
	led_off(RED);
	led_off(BLUE);
}

void win(void)
{
	led_on(RED);
	led_on(BLUE);
}