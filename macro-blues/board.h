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

/* ---- Key switch inputs (active-low, internal pull-up) ---- */
/* 12-key macropad: each switch wired directly from GPIO to GND */
#define PIN_KEY1  2   /* A0  — left header  */
#define PIN_KEY2  3   /* A1  — left header  */
#define PIN_KEY3  4   /* A2  — left header  */
#define PIN_KEY4  5   /* A3  — left header  */
#define PIN_KEY5  28  /* A4  — left header  */
#define PIN_KEY6  29  /* A5  — left header  */
#define PIN_KEY7  30  /*     — right header */
#define PIN_KEY8  7   /*     — right header */
#define PIN_KEY9  11  /*     — right header */
#define PIN_KEY10 27  /*     — right header */
#define PIN_KEY11 15  /*     — right header */
#define PIN_KEY12 16  /*     — right header */

#define NUM_KEYS  12

/* ---- Analog inputs ---- */
#define PIN_AIN0 2
#define PIN_AIN1 3
#define PIN_AIN2 4
#define PIN_AIN3 5
#define PIN_AIN4 28
#define PIN_AIN5 29
#define PIN_AIN6 30
#define PIN_AIN7_BAT 31 /* battery voltage — do not use for general I/O */

/* ---- Battery (EEMB LP103454-PCM-LD, 3.7V / 2000mAh LiPo) ----
 * The Feather routes VBAT through a 2:1 voltage divider to AIN7.
 * Divider numerator/denominator lets battery.c recover the true
 * battery voltage from the ADC reading.  Adjust if your board
 * uses different resistor values. */
#define BAT_DIVIDER_NUM   2   /* Vbat = Vadc × (NUM / DEN) */
#define BAT_DIVIDER_DEN   1

/* LiPo voltage thresholds (millivolts) */
#define BAT_MV_FULL       4200  /* fully charged */
#define BAT_MV_NOMINAL    3700  /* nominal */
#define BAT_MV_LOW        3300  /* low-battery warning threshold */
#define BAT_MV_CUTOFF     3000  /* empty / PCM cutoff */

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
