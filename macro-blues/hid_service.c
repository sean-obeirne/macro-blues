#include <stdint.h>
#include <string.h>

#include "ble.h"
#include "ble_gatts.h"
#include "ble_gap.h"
#include "ble_gatt.h"
#include "nrf_error.h"

#include "hid_service.h"
#include "ble_stack.h"
#include "led.h"

/*
 * hid_service.c — BLE HID keyboard service (HOGP)
 *
 * Registers the HID Service (UUID 0x1812) with the SoftDevice's GATT
 * server.  The service contains:
 *
 *   1. HID Information  (0x2A4A) — version, country, flags
 *   2. Report Map       (0x2A4B) — USB HID report descriptor
 *   3. Report           (0x2A4D) — 8-byte keyboard input report (notify)
 *   4. Protocol Mode    (0x2A4E) — report mode vs boot mode
 *
 * All characteristic values are stored in the SD stack (VLOC_STACK)
 * except the Report, which uses VLOC_STACK + notification via HVX.
 *
 * The standard 8-byte keyboard report format:
 *   Byte 0: Modifier keys (bitmask: Ctrl, Shift, Alt, GUI)
 *   Byte 1: Reserved (0x00)
 *   Bytes 2-7: Up to 6 simultaneous keycodes (USB HID usage codes)
 */

/* ---- Handles ---- */

static uint16_t service_handle;
static ble_gatts_char_handles_t report_handles;
static ble_gatts_char_handles_t report_map_handles;
static ble_gatts_char_handles_t hid_info_handles;
static ble_gatts_char_handles_t protocol_mode_handles;

/* Track whether the central has enabled notifications on the Report
 * characteristic (by writing 0x0001 to the CCCD). */
static uint8_t notifications_enabled;

/* ---- HID Report Descriptor (Report Map) ----
 *
 * This is a USB HID report descriptor that tells the host:
 *   "I'm a keyboard that sends an 8-byte input report."
 *
 * The descriptor is defined by the USB HID spec (not Bluetooth-specific).
 * Tools like https://eleccelerator.com/usbhidreportdescriptor/ can decode it.
 */
static const uint8_t report_map[] = {
    0x05,
    0x01, /* Usage Page (Generic Desktop) */
    0x09,
    0x06, /* Usage (Keyboard) */
    0xA1,
    0x01, /* Collection (Application) */
    0x85,
    0x01, /*   Report ID (1) */

    /* Modifier keys: 8 bits for Ctrl/Shift/Alt/GUI */
    0x05,
    0x07, /*   Usage Page (Keyboard/Keypad) */
    0x19,
    0xE0, /*   Usage Minimum (Left Control) */
    0x29,
    0xE7, /*   Usage Maximum (Right GUI) */
    0x15,
    0x00, /*   Logical Minimum (0) */
    0x25,
    0x01, /*   Logical Maximum (1) */
    0x75,
    0x01, /*   Report Size (1 bit) */
    0x95,
    0x08, /*   Report Count (8 bits) */
    0x81,
    0x02, /*   Input (Data, Variable, Absolute) */

    /* Reserved byte */
    0x95,
    0x01, /*   Report Count (1) */
    0x75,
    0x08, /*   Report Size (8 bits) */
    0x81,
    0x01, /*   Input (Constant) — padding */

    /* LED output report (5 bits for Num/Caps/Scroll/Compose/Kana) */
    0x95,
    0x05, /*   Report Count (5) */
    0x75,
    0x01, /*   Report Size (1) */
    0x05,
    0x08, /*   Usage Page (LEDs) */
    0x19,
    0x01, /*   Usage Minimum (Num Lock) */
    0x29,
    0x05, /*   Usage Maximum (Kana) */
    0x91,
    0x02, /*   Output (Data, Variable, Absolute) */

    /* LED padding (3 bits) */
    0x95,
    0x01, /*   Report Count (1) */
    0x75,
    0x03, /*   Report Size (3) */
    0x91,
    0x01, /*   Output (Constant) — padding */

    /* Keycode array: 6 keys */
    0x95,
    0x06, /*   Report Count (6) */
    0x75,
    0x08, /*   Report Size (8 bits) */
    0x15,
    0x00, /*   Logical Minimum (0) */
    0x26,
    0xFF,
    0x00, /*   Logical Maximum (255) */
    0x05,
    0x07, /*   Usage Page (Keyboard/Keypad) */
    0x19,
    0x00, /*   Usage Minimum (0) */
    0x2A,
    0xFF,
    0x00, /*   Usage Maximum (255) */
    0x81,
    0x00, /*   Input (Data, Array) */

    0xC0, /* End Collection */
};

/* ---- HID Information value ---- */

static const uint8_t hid_info_value[] = {
    0x11,
    0x01, /* bcdHID: HID version 1.11 */
    0x00, /* bCountryCode: 0 = not localized */
    0x02, /* Flags: bit 1 = Normally Connectable */
};

/* ---- Protocol Mode value ---- */

/* 0 = Boot Protocol, 1 = Report Protocol (default) */
static uint8_t protocol_mode_value = 0x01;

/* ---- Report Reference descriptor value ----
 * Tells the host this Report characteristic is Input Report ID 1. */
static const uint8_t report_ref_value[] = {
    0x01, /* Report ID: 1 */
    0x01, /* Report Type: Input */
};

/* ---- Helper: add a Report Reference descriptor ---- */

static void add_report_reference(uint16_t char_handle)
{
    ble_uuid_t desc_uuid = {
        .uuid = 0x2908, /* Report Reference */
        .type = BLE_UUID_TYPE_BLE,
    };

    ble_gatts_attr_md_t desc_md;
    memset(&desc_md, 0, sizeof(desc_md));
    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&desc_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&desc_md.write_perm);
    desc_md.vloc = BLE_GATTS_VLOC_STACK;

    ble_gatts_attr_t desc_attr = {
        .p_uuid = &desc_uuid,
        .p_attr_md = &desc_md,
        .init_len = sizeof(report_ref_value),
        .max_len = sizeof(report_ref_value),
        .p_value = (uint8_t *)report_ref_value,
    };

    uint16_t desc_handle;
    sd_ble_gatts_descriptor_add(char_handle, &desc_attr, &desc_handle);
}

/* ---- Error feedback ----
 * N red blinks = which GATTS call failed, M blue blinks = NRF error code.
 * Same pattern as ble_stack.c's error_blink but numbering starts at 6
 * to distinguish from ble_stack errors (1–5). */
static void hid_error_blink(int n, uint32_t err)
{
    volatile uint32_t d;
    while (1)
    {
        for (int i = 0; i < n; i++)
        {
            led_on(LED_RED);
            for (d = 0; d < 1600000; d++)
                ;
            led_off(LED_RED);
            for (d = 0; d < 1600000; d++)
                ;
        }
        for (d = 0; d < 3000000; d++)
            ;
        for (uint32_t i = 0; i < err; i++)
        {
            led_on(LED_BLUE);
            for (d = 0; d < 1600000; d++)
                ;
            led_off(LED_BLUE);
            for (d = 0; d < 1600000; d++)
                ;
        }
        for (d = 0; d < 5000000; d++)
            ;
    }
}

/* ---- Device Information Service (mandatory for HOGP) ---- */

static void dis_init(void)
{
    uint32_t err;
    uint16_t svc_handle;

    ble_uuid_t svc_uuid = {.uuid = 0x180A, .type = BLE_UUID_TYPE_BLE};
    err = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                                   &svc_uuid, &svc_handle);
    if (err != NRF_SUCCESS)
        hid_error_blink(11, err);

    /* PnP ID (0x2A50) — required by HOGP so the host knows this
     * is a HID device.  The values identify our "manufacturer". */
    static const uint8_t pnp_id[] = {
        0x02, /* Vendor ID Source: USB Implementers Forum */
        0x15,
        0x19, /* Vendor ID: 0x1915 (Nordic Semiconductor) */
        0x01,
        0x00, /* Product ID: 0x0001 */
        0x01,
        0x00, /* Product Version: 0x0001 */
    };

    ble_gatts_char_md_t char_md;
    memset(&char_md, 0, sizeof(char_md));
    char_md.char_props.read = 1;

    ble_gatts_attr_md_t attr_md;
    memset(&attr_md, 0, sizeof(attr_md));
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.write_perm);
    attr_md.vloc = BLE_GATTS_VLOC_STACK;

    ble_uuid_t uuid = {.uuid = 0x2A50, .type = BLE_UUID_TYPE_BLE};

    ble_gatts_attr_t attr = {
        .p_uuid = &uuid,
        .p_attr_md = &attr_md,
        .init_len = sizeof(pnp_id),
        .max_len = sizeof(pnp_id),
        .p_value = (uint8_t *)pnp_id,
    };

    ble_gatts_char_handles_t handles;
    err = sd_ble_gatts_characteristic_add(svc_handle, &char_md, &attr, &handles);
    if (err != NRF_SUCCESS)
        hid_error_blink(12, err);
}

/* ---- Battery Service (expected by iOS/macOS for HID devices) ----
 *
 * Registers the Battery Service (UUID 0x180F) with a single Battery
 * Level characteristic (UUID 0x2A19).  The Bluetooth SIG defines
 * this as a 1-byte value, 0–100, representing percent charge.
 *
 * iOS and macOS expect HID peripherals to expose this service — it's
 * how the system shows the battery icon next to the device name in
 * Bluetooth settings.
 *
 * For now the level is hardcoded to 100%.  Task 8 will replace this
 * with real ADC readings from the Feather's LiPo voltage divider.
 */
static void bas_init(void)
{
    uint32_t err;
    uint16_t svc_handle;

    /* Register the Battery Service as a primary service */
    ble_uuid_t svc_uuid = {.uuid = 0x180F, .type = BLE_UUID_TYPE_BLE};
    err = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                                   &svc_uuid, &svc_handle);
    if (err != NRF_SUCCESS)
        hid_error_blink(13, err);

    /* Battery Level characteristic (0x2A19).
     * - Read: host can poll the current level at any time.
     * - Notify: we can push updates when the level changes.
     * Hardcoded to 100% for now. */
    static uint8_t battery_level = 100;

    ble_gatts_char_md_t char_md;
    memset(&char_md, 0, sizeof(char_md));
    char_md.char_props.read   = 1;
    char_md.char_props.notify = 1;

    /* CCCD for notifications — open access (no encryption needed
     * just to subscribe to battery updates). */
    ble_gatts_attr_md_t cccd_md;
    memset(&cccd_md, 0, sizeof(cccd_md));
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);
    cccd_md.vloc = BLE_GATTS_VLOC_STACK;
    char_md.p_cccd_md = &cccd_md;

    /* Attribute: readable by anyone, not writable by the central */
    ble_gatts_attr_md_t attr_md;
    memset(&attr_md, 0, sizeof(attr_md));
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.write_perm);
    attr_md.vloc = BLE_GATTS_VLOC_STACK;

    ble_uuid_t uuid = {.uuid = 0x2A19, .type = BLE_UUID_TYPE_BLE};

    /* 1 byte, fixed length, value stored in SoftDevice RAM */
    ble_gatts_attr_t attr_val = {
        .p_uuid    = &uuid,
        .p_attr_md = &attr_md,
        .init_len  = 1,
        .max_len   = 1,
        .p_value   = &battery_level,
    };

    ble_gatts_char_handles_t handles;
    err = sd_ble_gatts_characteristic_add(svc_handle, &char_md, &attr_val, &handles);
    if (err != NRF_SUCCESS)
        hid_error_blink(14, err);
}

/* ---- HID Service registration ---- */

void hid_service_init(void)
{
    uint32_t err;

    /* Device Information Service must exist for HOGP compliance */
    dis_init();

    /* Battery Service — iOS/macOS expect this for HID keyboards */
    bas_init();

    /* Register the HID Service (UUID 0x1812) */
    ble_uuid_t svc_uuid = {
        .uuid = 0x1812,
        .type = BLE_UUID_TYPE_BLE,
    };
    err = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                                   &svc_uuid, &service_handle);
    if (err != NRF_SUCCESS)
        hid_error_blink(6, err);

    /* ---- 1. HID Information (0x2A4A) — read only ---- */
    {
        ble_gatts_char_md_t char_md;
        memset(&char_md, 0, sizeof(char_md));
        char_md.char_props.read = 1;

        ble_gatts_attr_md_t attr_md;
        memset(&attr_md, 0, sizeof(attr_md));
        BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&attr_md.read_perm);
        BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.write_perm);
        attr_md.vloc = BLE_GATTS_VLOC_STACK;

        ble_uuid_t uuid = {.uuid = 0x2A4A, .type = BLE_UUID_TYPE_BLE};

        ble_gatts_attr_t attr = {
            .p_uuid = &uuid,
            .p_attr_md = &attr_md,
            .init_len = sizeof(hid_info_value),
            .max_len = sizeof(hid_info_value),
            .p_value = (uint8_t *)hid_info_value,
        };

        err = sd_ble_gatts_characteristic_add(service_handle,
                                              &char_md, &attr,
                                              &hid_info_handles);
        if (err != NRF_SUCCESS)
            hid_error_blink(7, err);
    }

    /* ---- 2. Report Map (0x2A4B) — read only ---- */
    {
        ble_gatts_char_md_t char_md;
        memset(&char_md, 0, sizeof(char_md));
        char_md.char_props.read = 1;

        ble_gatts_attr_md_t attr_md;
        memset(&attr_md, 0, sizeof(attr_md));
        BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&attr_md.read_perm);
        BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.write_perm);
        attr_md.vloc = BLE_GATTS_VLOC_STACK;

        ble_uuid_t uuid = {.uuid = 0x2A4B, .type = BLE_UUID_TYPE_BLE};

        ble_gatts_attr_t attr = {
            .p_uuid = &uuid,
            .p_attr_md = &attr_md,
            .init_len = sizeof(report_map),
            .max_len = sizeof(report_map),
            .p_value = (uint8_t *)report_map,
        };

        err = sd_ble_gatts_characteristic_add(service_handle,
                                              &char_md, &attr,
                                              &report_map_handles);
        if (err != NRF_SUCCESS)
            hid_error_blink(8, err);
    }

    /* ---- 3. Report (0x2A4D) — notify + read, input report ---- */
    {
        ble_gatts_char_md_t char_md;
        memset(&char_md, 0, sizeof(char_md));
        char_md.char_props.read = 1;
        char_md.char_props.notify = 1;

        /* CCCD needs its own attr_md — the central writes to it
         * to enable/disable notifications. */
        ble_gatts_attr_md_t cccd_md;
        memset(&cccd_md, 0, sizeof(cccd_md));
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
        BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&cccd_md.write_perm);
        cccd_md.vloc = BLE_GATTS_VLOC_STACK;
        char_md.p_cccd_md = &cccd_md;

        ble_gatts_attr_md_t attr_md;
        memset(&attr_md, 0, sizeof(attr_md));
        BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&attr_md.read_perm);
        BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.write_perm);
        attr_md.vloc = BLE_GATTS_VLOC_STACK;
        attr_md.vlen = 1; /* variable length — matches SDK pattern */

        ble_uuid_t uuid = {.uuid = 0x2A4D, .type = BLE_UUID_TYPE_BLE};

        /* No initial value; 8-byte max = modifier + reserved + 6 keys.
         * Report ID is NOT in the BLE characteristic value — it's
         * conveyed by the Report Reference descriptor instead. */
        ble_gatts_attr_t attr = {
            .p_uuid = &uuid,
            .p_attr_md = &attr_md,
            .init_len = 0,
            .max_len = 8,
            .p_value = NULL,
        };

        err = sd_ble_gatts_characteristic_add(service_handle,
                                              &char_md, &attr,
                                              &report_handles);
        if (err != NRF_SUCCESS)
            hid_error_blink(9, err);

        /* Add the Report Reference descriptor so the host knows
         * this is Input Report ID 1. */
        add_report_reference(report_handles.value_handle);
    }

    /* ---- 4. Protocol Mode (0x2A4E) — read + write-no-response ---- */
    {
        ble_gatts_char_md_t char_md;
        memset(&char_md, 0, sizeof(char_md));
        char_md.char_props.read = 1;
        char_md.char_props.write_wo_resp = 1;

        ble_gatts_attr_md_t attr_md;
        memset(&attr_md, 0, sizeof(attr_md));
        BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&attr_md.read_perm);
        BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&attr_md.write_perm);
        attr_md.vloc = BLE_GATTS_VLOC_STACK;

        ble_uuid_t uuid = {.uuid = 0x2A4E, .type = BLE_UUID_TYPE_BLE};

        ble_gatts_attr_t attr = {
            .p_uuid = &uuid,
            .p_attr_md = &attr_md,
            .init_len = sizeof(protocol_mode_value),
            .max_len = sizeof(protocol_mode_value),
            .p_value = &protocol_mode_value,
        };

        err = sd_ble_gatts_characteristic_add(service_handle,
                                              &char_md, &attr,
                                              &protocol_mode_handles);
        if (err != NRF_SUCCESS)
            hid_error_blink(10, err);
    }

    notifications_enabled = 0;
}

/* ---- CCCD tracking ---- */

void hid_service_on_write(uint16_t handle, const uint8_t *data, uint16_t len)
{
    /* The central enables notifications by writing 0x0001 to the
     * Report characteristic's CCCD. */
    if (handle == report_handles.cccd_handle && len >= 2)
    {
        notifications_enabled = (data[0] & 0x01);
    }
}

/* ---- Report sending ---- */

void hid_service_send_report(uint8_t modifier, const uint8_t *keys, uint8_t num_keys)
{
    uint16_t conn = ble_stack_conn_handle();
    if (conn == 0xFFFF || !notifications_enabled)
        return;

    /* Build the 8-byte keyboard report: modifier + reserved + 6 keys.
     * Report ID is NOT included — the Report Reference descriptor
     * tells the host this is Report ID 1. */
    uint8_t report[8];
    report[0] = modifier;
    report[1] = 0x00; /* reserved */

    /* Fill in up to 6 keycodes */
    for (int i = 0; i < 6; i++)
    {
        report[2 + i] = (i < num_keys) ? keys[i] : 0x00;
    }

    uint16_t len = sizeof(report);
    ble_gatts_hvx_params_t hvx = {
        .handle = report_handles.value_handle,
        .type = BLE_GATT_HVX_NOTIFICATION,
        .offset = 0,
        .p_len = &len,
        .p_data = report,
    };

    sd_ble_gatts_hvx(conn, &hvx);
    /* Ignore errors — the central may not be ready or the TX queue
     * may be full.  Next key event will try again. */
}

void hid_service_send_key(uint8_t keycode)
{
    hid_service_send_report(0, &keycode, 1);
}

void hid_service_send_release(void)
{
    hid_service_send_report(0, NULL, 0);
}
