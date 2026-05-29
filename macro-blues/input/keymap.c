#include "keymap.h"
#include "hid_service.h"

/*
 * Written in physical reading order (top-left → bottom-right).
 * Scan index = r*NUM_COLS + c, where r=row(RX/MISO/MOSI/SCK), c=col(A3/A4/A5).
 * Physical row order top→bottom: RX[r0], MISO[r1], MOSI[r2], SCK[r3].
 *
 *            RX[r0] MISO[r1] MOSI[r2] SCK[r3]
 *   A3[c0]:   [0]     [3]      [6]      [9]
 *   A4[c1]:   [1]     [4]      [7]      [10]
 *   A5[c2]:   [2]     [5]      [8]      [11]
 */
const uint8_t keymap[NUM_KEYS] = {
    [0]  = HID_KEY_N,
    [1]  = HID_KEY_T,
    [2]  = HID_KEY_F19,

    [3]  = HID_KEY_P,
    [4]  = HID_KEY_M,
    [5]  = HID_KEY_NONE,

    [6]  = HID_KEY_ESCAPE,
    [7]  = HID_KEY_B,
    [8]  = HID_KEY_NONE,

    [9]  = HID_KEY_NONE,
    [10] = HID_KEY_V,
    [11] = HID_KEY_W,
};

const uint8_t keymod[NUM_KEYS] = {
    [0]  = HID_MOD_HYPER,
    [1]  = HID_MOD_HYPER,
    [2]  = 0,

    [3]  = HID_MOD_HYPER,
    [4]  = HID_MOD_HYPER,
    [5]  = HID_MOD_HYPER,

    [6]  = 0,
    [7]  = HID_MOD_HYPER,
    [8]  = HID_MOD_HYPER,

    [9]  = HID_MOD_HYPER,
    [10] = HID_MOD_HYPER,
    [11] = HID_MOD_LGUI,
};
