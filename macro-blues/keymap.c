#include "keymap.h"
#include "hid_service.h"

const uint8_t keymap[NUM_KEYS] = {
	HID_KEY_V,
	HID_KEY_B,
	HID_KEY_C,
	HID_KEY_D,
	HID_KEY_E,
	HID_KEY_F,
	HID_KEY_G,
	HID_KEY_H,
	HID_KEY_I,
	HID_KEY_J,
	HID_KEY_K,
	HID_KEY_L,
};

const uint8_t keymod[NUM_KEYS] = {
	HID_MOD_HYPER,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
