#include "keymap.h"
#include "hid_service.h"

/*
 * Written in physical reading order (top-left → bottom-right).
 * Scan index = r*NUM_COLS + c, where r=row(SCK/MOSI/MISO/RX), c=col(A3/A4/A5).
 * Designated initializers map visual position to the correct scan index.
 *
 *            SCK[r0] MOSI[r1] MISO[r2] RX[r3]
 *   A3[c0]:   [0]     [3]      [6]      [9]
 *   A4[c1]:   [1]     [4]      [7]      [10]
 *   A5[c2]:   [2]     [5]      [8]      [11]
 */
const uint8_t keymap[NUM_KEYS] = {
    /* top row */
    [0] = HID_KEY_NONE,
    [3] = HID_KEY_NONE,
    [6] = HID_KEY_NONE,
    [9] = HID_KEY_NONE,
    /* middle row */
    [1] = HID_KEY_F4,
    [4] = HID_KEY_F3,
    [7] = HID_KEY_F2,
    [10] = HID_KEY_F1,
    /* bottom row */
    [2] = HID_KEY_NONE,
    [5] = HID_KEY_NONE,
    [8] = HID_KEY_F6,
    [11] = HID_KEY_F5,
};

const uint8_t keymod[NUM_KEYS] = {
    /* top row */
    [0] = 0,
    [3] = 0,
    [6] = 0,
    [9] = 0,
    /* middle row */
    [1] = 0,
    [4] = 0,
    [7] = 0,
    [10] = 0,
    /* bottom row */
    [2] = 0,
    [5] = 0,
    [8] = 0,
    [11] = 0,
};
