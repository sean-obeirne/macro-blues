#include "keymap.h"
#include "hid_service.h"

/*
 * Matrix transposed to 3 rows (A3/A4/A5) × 4 cols (SCK/MOSI/MISO/RX).
 * Physical key (old_row r, old_col c) now lives at index c*4+r.
 * old layout: rows=SCK/MOSI/MISO/RX, cols=A3/A4/A5, index=r*3+c
 *
 * Mapping old_idx → new_idx:
 *   0(r0c0)→0  1(r0c1)→4  2(r0c2)→8
 *   3(r1c0)→1  4(r1c1)→5  5(r1c2)→9
 *   6(r2c0)→2  7(r2c1)→6  8(r2c2)→10
 *   9(r3c0)→3 10(r3c1)→7 11(r3c2)→11
 */
const uint8_t keymap[NUM_KEYS] = {
    /* col: SCK  MOSI  MISO  RX  */
    /* A3 */ HID_KEY_V,
    HID_KEY_D,
    HID_KEY_G,
    HID_KEY_J,
    /* A4 */ HID_KEY_B,
    HID_KEY_E,
    HID_KEY_H,
    HID_KEY_K,
    /* A5 */ HID_KEY_C,
    HID_KEY_F,
    HID_KEY_I,
    HID_KEY_L,
};

const uint8_t keymod[NUM_KEYS] = {
    /* A3 */ HID_MOD_HYPER,
    0,
    0,
    0,
    /* A4 */ HID_MOD_HYPER,
    0,
    0,
    0,
    /* A5 */ 0,
    0,
    0,
    0,
};
