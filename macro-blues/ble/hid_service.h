#ifndef HID_SERVICE_H
#define HID_SERVICE_H

/*
 * hid_service.h — BLE HID (Human Interface Device) over GATT
 *
 * Implements the HOGP (HID over GATT Profile) keyboard service.
 * Registers the HID Service (UUID 0x1812) with characteristics for:
 *   - HID Information   (read)
 *   - Report Map         (read)  — the USB HID report descriptor
 *   - Report             (notify + read) — 8-byte keyboard input report
 *   - Protocol Mode      (read + write-without-response)
 *
 * The report format is the standard 8-byte USB keyboard report:
 *   [modifier, reserved, key1, key2, key3, key4, key5, key6]
 *
 * Usage:
 *   hid_service_init();                      // after ble_stack_init()
 *   hid_service_send_key(HID_KEY_A);         // on key press
 *   hid_service_send_release();              // on key release
 */

#include <stdint.h>

/* Initialize the HID service (register with GATT server).
 * Call after ble_stack_init(). */
void hid_service_init(void);

/* Send an 8-byte keyboard input report as a notification.
 * Only works when connected and the central has enabled notifications
 * (written 0x0001 to the CCCD). */
void hid_service_send_report(uint8_t modifier, const uint8_t *keys, uint8_t num_keys);

/* Send a mouse scroll wheel delta (+ve = up, -ve = down) */
void hid_service_send_scroll(int8_t delta);

/* Convenience: send a single keycode press (no modifiers) */
void hid_service_send_key(uint8_t keycode);

/* Convenience: send an empty report (all keys released) */
void hid_service_send_release(void);

/* Send a mouse button report (Report ID 2, buttons only, no movement/scroll) */
void hid_service_send_mouse_buttons(uint8_t buttons);

/* Forward BLE GATTS write events to the HID service.
 * Called from ble_stack_process() for CCCD tracking. */
void hid_service_on_write(uint16_t handle, const uint8_t *data, uint16_t len);

/* ---- USB HID Keycodes (subset — from USB HID Usage Tables 1.12) ---- */

#define HID_KEY_NONE  0x00
#define HID_KEY_A     0x04
#define HID_KEY_B     0x05
#define HID_KEY_C     0x06
#define HID_KEY_D     0x07
#define HID_KEY_E     0x08
#define HID_KEY_F     0x09
#define HID_KEY_G     0x0A
#define HID_KEY_H     0x0B
#define HID_KEY_I     0x0C
#define HID_KEY_J     0x0D
#define HID_KEY_K     0x0E
#define HID_KEY_L     0x0F
#define HID_KEY_M     0x10
#define HID_KEY_N     0x11
#define HID_KEY_O     0x12
#define HID_KEY_P     0x13
#define HID_KEY_Q     0x14
#define HID_KEY_R     0x15
#define HID_KEY_S     0x16
#define HID_KEY_T     0x17
#define HID_KEY_U     0x18
#define HID_KEY_V     0x19
#define HID_KEY_W     0x1A
#define HID_KEY_X     0x1B
#define HID_KEY_Y     0x1C
#define HID_KEY_Z     0x1D

#define HID_KEY_1     0x1E
#define HID_KEY_2     0x1F
#define HID_KEY_3     0x20
#define HID_KEY_4     0x21
#define HID_KEY_5     0x22
#define HID_KEY_6     0x23
#define HID_KEY_7     0x24
#define HID_KEY_8     0x25
#define HID_KEY_9     0x26
#define HID_KEY_0     0x27

#define HID_KEY_ENTER       0x28
#define HID_KEY_ESCAPE      0x29
#define HID_KEY_BACKSPACE   0x2A
#define HID_KEY_TAB         0x2B
#define HID_KEY_SPACE       0x2C

#define HID_KEY_F1    0x3A
#define HID_KEY_F2    0x3B
#define HID_KEY_F3    0x3C
#define HID_KEY_F4    0x3D
#define HID_KEY_F5    0x3E
#define HID_KEY_F6    0x3F
#define HID_KEY_F7    0x40
#define HID_KEY_F8    0x41
#define HID_KEY_F9    0x42
#define HID_KEY_F10   0x43
#define HID_KEY_F11   0x44
#define HID_KEY_F12   0x45

#define HID_KEY_F13   0x68
#define HID_KEY_F14   0x69
#define HID_KEY_F15   0x6A
#define HID_KEY_F16   0x6B
#define HID_KEY_F17   0x6C
#define HID_KEY_F18   0x6D
#define HID_KEY_F19   0x6E
#define HID_KEY_F20   0x6F
#define HID_KEY_F21   0x70
#define HID_KEY_F22   0x71
#define HID_KEY_F23   0x72
#define HID_KEY_F24   0x73

/* Modifier bit masks (byte 0 of the report) */
#define HID_MOD_LCTRL   0x01
#define HID_MOD_LSHIFT  0x02
#define HID_MOD_LALT    0x04
#define HID_MOD_LGUI    0x08
#define HID_MOD_RCTRL   0x10
#define HID_MOD_RSHIFT  0x20
#define HID_MOD_RALT    0x40
#define HID_MOD_RGUI    0x80

/* Composite modifier combos */
// #define HID_MOD_HYPER   (HID_MOD_LSHIFT | HID_MOD_LCTRL | HID_MOD_LALT | HID_MOD_LGUI)
#define HID_MOD_HYPER   (HID_MOD_LSHIFT | HID_MOD_LCTRL | HID_MOD_LALT | HID_MOD_LGUI) /* no GUI — avoids weirdness on some hosts */

#define HYPER(key) (HID_MOD_HYPER | (key))

#endif /* HID_SERVICE_H */
