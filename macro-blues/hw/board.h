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

/* ---- Key matrix (4 rows × 3 cols = 12 positions) ---- */
/* Rows: driven LOW one at a time to strobe.               */
/* Cols: read as active-low inputs with internal pull-up.  */
#define PIN_ROW0 12 /* SCK  — row 0 */
#define PIN_ROW1 13 /* MOSI — row 1 */
#define PIN_ROW2 14 /* MISO — row 2 */
#define PIN_ROW3 8  /* RX   — row 3 */
#define NUM_ROWS 4

#define PIN_COL0 5  /* A3 — col 0 */
#define PIN_COL1 28 /* A4 — col 1 */
#define PIN_COL2 29 /* A5 — col 2 */
#define NUM_COLS 3

#define NUM_KEYS (NUM_ROWS * NUM_COLS) /* 12 */

/* ---- Analog inputs ---- */
#define PIN_AIN7_BAT 31 /* battery voltage — do not use for general I/O */

/* ---- Battery (EEMB LP103454-PCM-LD, 3.7V / 2000mAh LiPo) ----
 * The Feather routes VBAT through a 2:1 voltage divider to AIN7.
 * Divider numerator/denominator lets battery.c recover the true
 * battery voltage from the ADC reading.  Adjust if your board
 * uses different resistor values. */
#define BAT_DIVIDER_NUM 2 /* Vbat = Vadc × (NUM / DEN) */
#define BAT_DIVIDER_DEN 1

/* LiPo voltage thresholds (millivolts) */
#define BAT_MV_FULL 4200    /* fully charged */
#define BAT_MV_NOMINAL 3700 /* nominal */
#define BAT_MV_LOW 3300     /* low-battery warning threshold */
#define BAT_MV_CUTOFF 3000  /* empty / PCM cutoff */

/* ---- Rotary encoder (A0/A1/A2, separate from matrix) ---- */
#define PIN_ENC_BTN 2 /* A0 — push button  */
#define PIN_ENC_A 3   /* A1 — quadrature A */
#define PIN_ENC_B 4   /* A2 — quadrature B */

/* ---- I2C ---- */
#define PIN_I2C_SCL 26
#define PIN_I2C_SDA 25

/* ---- NFC (directly on chip pads) ---- */
#define PIN_NFC1 9
#define PIN_NFC2 10

/* ---- Misc ---- */
#define PIN_DFU 20

#endif /* BOARD_H */
