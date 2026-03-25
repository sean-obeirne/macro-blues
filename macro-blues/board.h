#ifndef BOARD_H
#define BOARD_H

/*
 * board.h — Pin assignments and board-level configuration
 *
 * Everything that changes if you wire up a different board goes here.
 * The rest of the firmware references these names, never raw pin numbers.
 */

/* ---- LEDs (accent/status) ---- */
#define PIN_LED_RED 17
#define PIN_LED_BLUE 19

/* ---- Key switch inputs ---- */
#define PIN_KEY1 7
#define PIN_KEY2 11
#define PIN_KEY3 27

/* ---- Analog inputs ---- */
#define PIN_AIN0 2
#define PIN_AIN1 3
#define PIN_AIN2 4
#define PIN_AIN3 5
#define PIN_AIN4 28
#define PIN_AIN5 29
#define PIN_AIN6 30
#define PIN_AIN7_BAT 31 /* battery voltage — do not use for general I/O */

/* ---- SPI ---- */
#define PIN_SPI_CLK 12
#define PIN_SPI_MOSI 13
#define PIN_SPI_MISO 14

/* ---- UART ---- */
#define PIN_UART_RX 8
#define PIN_UART_TX 6
#define PIN_UART_AUX1 16
#define PIN_UART_AUX2 15

/* ---- I2C ---- */
#define PIN_I2C_SCL 26
#define PIN_I2C_SDA 25

/* ---- NFC (directly on chip pads) ---- */
#define PIN_NFC1 9
#define PIN_NFC2 10

/* ---- Misc ---- */
#define PIN_DFU 20

#endif /* BOARD_H */
