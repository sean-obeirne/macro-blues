#include "nrf52832.h"
#include "board.h"
#include "i2c.h"

void i2c_init(void)
{
	/* Configure SCL and SDA pins for I2C open-drain operation:
	 *   DIR=Input, INPUT=Connect, PULL=Pullup, DRIVE=S0D1, SENSE=Off
	 *   S0D1 = standard-0, disconnect-1 (open-drain required by I2C) */
	GPIO_PIN_CNF(PIN_I2C_SCL) = (0 << PIN_CNF_DIR)
								| (0 << PIN_CNF_INPUT)
								| (3 << PIN_CNF_PULL)   /* pull-up  */
								| (6 << PIN_CNF_DRIVE)  /* S0D1     */
								| (0 << PIN_CNF_SENSE);

	GPIO_PIN_CNF(PIN_I2C_SDA) = (0 << PIN_CNF_DIR)
								| (0 << PIN_CNF_INPUT)
								| (3 << PIN_CNF_PULL)
								| (6 << PIN_CNF_DRIVE)
								| (0 << PIN_CNF_SENSE);

	/* Assign pins to TWIM0 */
	TWIM0_PSEL_SCL = PIN_I2C_SCL;
	TWIM0_PSEL_SDA = PIN_I2C_SDA;

	/* 400 kHz — well within SSD1306 spec */
	TWIM0_FREQUENCY = TWIM_FREQ_400K;

	/* Auto-stop after last TX byte */
	TWIM0_SHORTS = TWIM_SHORTS_LASTTX_STOP;

	/* Enable TWIM0 */
	TWIM0_ENABLE = TWIM_ENABLE_VAL;
}

int i2c_write(uint8_t addr, const uint8_t *data, uint32_t len)
{
	TWIM0_ADDRESS = addr;
	TWIM0_TXD_PTR = (uint32_t)data;
	TWIM0_TXD_MAXCNT = len;

	/* Clear events from any previous transfer */
	TWIM0_EVENTS_STOPPED = 0;
	TWIM0_EVENTS_ERROR = 0;

	TWIM0_TASKS_STARTTX = 1;

	/* Poll for completion */
	while (!TWIM0_EVENTS_STOPPED && !TWIM0_EVENTS_ERROR)
		;

	if (TWIM0_EVENTS_ERROR)
	{
		TWIM0_EVENTS_ERROR = 0;
		uint32_t src = TWIM0_ERRORSRC;
		TWIM0_ERRORSRC = src; /* W1C — clear error flags */
		TWIM0_TASKS_STOP = 1;
		while (!TWIM0_EVENTS_STOPPED)
			;
		TWIM0_EVENTS_STOPPED = 0;
		return -1;
	}

	TWIM0_EVENTS_STOPPED = 0;
	return 0;
}
