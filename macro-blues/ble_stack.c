#include <stdint.h>
#include <string.h>

#include "nrf_sdm.h"
#include "nrf_soc.h"
#include "ble.h"
#include "ble_gap.h"
#include "ble_gatts.h"
#include "nrf_error.h"

#include "ble_stack.h"
#include "hid_service.h"
#include "led.h"

/*
 * ble_stack.c — SoftDevice BLE initialization and event handling
 *
 * This file brings up the full BLE peripheral stack in a few steps:
 *
 *   1. sd_softdevice_enable()  — starts the SD, takes over RADIO/RTC0/etc
 *   2. sd_ble_enable()         — allocates RAM for the BLE stack
 *   3. GAP configuration       — device name, appearance, conn params
 *   4. Advertising setup       — build ad data, configure, start
 *   5. Event loop              — drain events, handle connect/disconnect
 *
 * All SoftDevice calls are SVC instructions — the implementation lives in
 * the SoftDevice binary already flashed at address 0x00000.  We only need
 * the headers for the function signatures + struct definitions.
 */

/* ---- Configuration ---- */

#define DEVICE_NAME "Macro Blues"
#define DEVICE_NAME_LEN 11

/* Connection parameters (for a responsive keyboard).
 * The central (phone/PC) may negotiate different values. */
#define MIN_CONN_INTERVAL 12 /* 15 ms   (in 1.25 ms units) */
#define MAX_CONN_INTERVAL 24 /* 30 ms   (in 1.25 ms units) */
#define SLAVE_LATENCY 6      /* skip up to 6 events when idle */
#define CONN_SUP_TIMEOUT 400 /* 4000 ms (in 10 ms units) */

/* Advertising interval. Lower = discovered faster, but uses more power.
 * 100 ms is a reasonable default for HID devices. */
#define ADV_INTERVAL 160 /* 100 ms  (in 0.625 ms units) */

/* ---- State ---- */

static uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;
static uint8_t adv_handle = BLE_GAP_ADV_SET_HANDLE_NOT_SET;

/* Advertising data buffers — must persist because the SoftDevice holds
 * pointers to them for the lifetime of the advertising set. */
static uint8_t adv_data_buf[31];
static uint8_t srp_data_buf[31];

/* ---- SoftDevice fault handler ---- */

static void sd_fault_handler(uint32_t id, uint32_t pc, uint32_t info)
{
    (void)id;
    (void)pc;
    (void)info;
    /* Something went catastrophically wrong inside the SoftDevice.
     * In production you'd log this and reset.  For now, just spin. */
    while (1)
        ;
}

/* ---- Build advertising data (raw TLV format) ---- */

static uint8_t build_adv_data(uint8_t *buf)
{
    uint8_t pos = 0;

    /* Flags: LE General Discoverable + BR/EDR Not Supported */
    buf[pos++] = 2;
    buf[pos++] = BLE_GAP_AD_TYPE_FLAGS;
    buf[pos++] = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;

    /* Complete Local Name: "Macro Blues" */
    buf[pos++] = 1 + DEVICE_NAME_LEN;
    buf[pos++] = BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME;
    memcpy(&buf[pos], DEVICE_NAME, DEVICE_NAME_LEN);
    pos += DEVICE_NAME_LEN;

    /* Appearance: HID Keyboard (0x03C1 = 961) */
    buf[pos++] = 3;
    buf[pos++] = BLE_GAP_AD_TYPE_APPEARANCE;
    buf[pos++] = (uint8_t)(BLE_APPEARANCE_HID_KEYBOARD & 0xFF);
    buf[pos++] = (uint8_t)(BLE_APPEARANCE_HID_KEYBOARD >> 8);

    return pos; /* total: 3 + 13 + 5 = 21 bytes (under 31 max) */
}

static uint8_t build_scan_rsp_data(uint8_t *buf)
{
    uint8_t pos = 0;

    /* Complete list of 16-bit service UUIDs: HID Service (0x1812) */
    buf[pos++] = 3;
    buf[pos++] = BLE_GAP_AD_TYPE_16BIT_SERVICE_UUID_COMPLETE;
    buf[pos++] = 0x12; /* low byte of 0x1812 */
    buf[pos++] = 0x18; /* high byte */

    return pos;
}

/* ---- Initialization steps ---- */

/*
 * Error blink:  N red blinks (which call), then M blue blinks (error code).
 *
 * NRF error codes:  0=OK, 1=SVC_MISSING, 2=SD_NOT_ENABLED, 3=INTERNAL,
 *   4=NO_MEM, 5=NOT_FOUND, 6=NOT_SUPPORTED, 7=INVALID_PARAM,
 *   8=INVALID_STATE, 16=INVALID_ADDR
 */
static void error_blink(int n, uint32_t err)
{
    volatile uint32_t d;
    while (1)
    {
        /* Red blinks — identifies which call failed */
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
            ; /* gap */
        /* Blue blinks — the actual NRF error code */
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
            ; /* long pause */
    }
}

static void softdevice_enable(void)
{
    nrf_clock_lf_cfg_t clock_cfg = {
        .source = NRF_CLOCK_LF_SRC_RC,
        .rc_ctiv = 16,     /* calibrate every 4 s */
        .rc_temp_ctiv = 2, /* temp-check every other calibration */
        .accuracy = NRF_CLOCK_LF_ACCURACY_500_PPM,
    };
    uint32_t err = sd_softdevice_enable(&clock_cfg, sd_fault_handler);
    if (err != NRF_SUCCESS)
        error_blink(1, err);

    /*
     * Tell the SD where our vector table lives.  Required when a
     * bootloader is present (Adafruit Feather ships with one) —
     * without this, the SD may forward app interrupts (like GPIOTE)
     * to the bootloader's vector table instead of ours.
     */
    err = sd_softdevice_vector_table_base_set(0x26000);
    if (err != NRF_SUCCESS)
        error_blink(1, err);
}

static void ble_enable(void)
{
    uint32_t ram_start = 0x20004000; /* must match linker script RAM origin */
    uint32_t err;

    /*
     * Increase the GATT attribute table from the default 1408 bytes
     * to 2048.  The HID service adds ~11 attributes (service decl,
     * 4 char decls, 4 values, CCCD, Report Reference) on top of the
     * default GAP/GATT services — the default table is too small and
     * sd_ble_gatts_characteristic_add returns NRF_ERROR_NOT_SUPPORTED.
     *
     * Must be called BEFORE sd_ble_enable().
     */
    ble_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.gatts_cfg.attr_tab_size.attr_tab_size = 2048;
    err = sd_ble_cfg_set(BLE_GATTS_CFG_ATTR_TAB_SIZE, &cfg, ram_start);
    if (err != NRF_SUCCESS)
        error_blink(2, err);

    err = sd_ble_enable(&ram_start);
    if (err == NRF_ERROR_NO_MEM)
    {
        /*
         * SD updated ram_start with the actual minimum it needs.
         * Retry with that value.  If this works but ram_start > our
         * linker ORIGIN(RAM), we need to bump the linker script to
         * match.  For now, accept and continue so BLE at least works.
         */
        err = sd_ble_enable(&ram_start);
    }
    if (err != NRF_SUCCESS)
        error_blink(2, err);
}

static void gap_params_init(void)
{
    uint32_t err;

    /* Device name — readable by anyone (no encryption required) */
    ble_gap_conn_sec_mode_t sec_mode;
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);
    err = sd_ble_gap_device_name_set(&sec_mode,
                                     (const uint8_t *)DEVICE_NAME,
                                     DEVICE_NAME_LEN);
    if (err != NRF_SUCCESS)
        error_blink(3, err);

    /* Appearance: keyboard */
    sd_ble_gap_appearance_set(BLE_APPEARANCE_HID_KEYBOARD);

    /* Preferred connection parameters (the central can use these as hints) */
    ble_gap_conn_params_t conn_params = {
        .min_conn_interval = MIN_CONN_INTERVAL,
        .max_conn_interval = MAX_CONN_INTERVAL,
        .slave_latency = SLAVE_LATENCY,
        .conn_sup_timeout = CONN_SUP_TIMEOUT,
    };
    sd_ble_gap_ppcp_set(&conn_params);
}

static void advertising_init(void)
{
    uint8_t adv_len = build_adv_data(adv_data_buf);
    uint8_t srp_len = build_scan_rsp_data(srp_data_buf);

    /*
     * Point the SD at our pre-built advertising + scan-response buffers.
     * These must persist (static globals) because the SD holds pointers
     * to them for the lifetime of the advertising set.
     */
    ble_gap_adv_data_t adv_data = {
        .adv_data = {.p_data = adv_data_buf, .len = adv_len},
        .scan_rsp_data = {.p_data = srp_data_buf, .len = srp_len},
    };

    /*
     * Advertising parameters:
     *   - CONNECTABLE_SCANNABLE_UNDIRECTED: any central can see us,
     *     request our scan-response data, and connect.
     *   - FP_ANY: no whitelist filtering — accept all centrals.
     *   - interval: how often we send an advertisement packet (100 ms).
     *     Lower = faster discovery, higher = less power.
     *   - duration 0: keep advertising forever until a connection or
     *     explicit stop.
     */
    ble_gap_adv_params_t adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.properties.type = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED;
    adv_params.filter_policy = BLE_GAP_ADV_FP_ANY;
    adv_params.interval = ADV_INTERVAL;
    adv_params.duration = 0;

    /* Register the advertising set with the SD (data + params together) */
    uint32_t err = sd_ble_gap_adv_set_configure(&adv_handle, &adv_data, &adv_params);
    if (err != NRF_SUCCESS)
        error_blink(4, err);
}

/* ---- Public API ---- */

void ble_stack_init(void)
{
    softdevice_enable();
    ble_enable();
    gap_params_init();
    advertising_init();

    /* Start advertising immediately */
    uint32_t err = sd_ble_gap_adv_start(adv_handle, BLE_CONN_CFG_TAG_DEFAULT);
    if (err != NRF_SUCCESS)
        error_blink(5, err);
}

void ble_stack_advertise(void)
{
    if (conn_handle == BLE_CONN_HANDLE_INVALID)
        sd_ble_gap_adv_start(adv_handle, BLE_CONN_CFG_TAG_DEFAULT);
}

void ble_stack_wait(void)
{
    sd_app_evt_wait();
}

int ble_stack_connected(void)
{
    return conn_handle != BLE_CONN_HANDLE_INVALID;
}

uint16_t ble_stack_conn_handle(void)
{
    return conn_handle;
}

void ble_stack_process(void)
{
    __attribute__((aligned(4)))
    uint8_t evt_buf[256];
    uint16_t evt_len;

    while (1)
    {
        evt_len = sizeof(evt_buf);
        uint32_t err = sd_ble_evt_get(evt_buf, &evt_len);
        if (err != NRF_SUCCESS)
            break; /* NRF_ERROR_NOT_FOUND = no more events */

        ble_evt_t *evt = (ble_evt_t *)evt_buf;

        switch (evt->header.evt_id)
        {

            /* ---- Connection management ---- */

        case BLE_GAP_EVT_CONNECTED:
            conn_handle = evt->evt.gap_evt.conn_handle;
            led_on(LED_BLUE);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            conn_handle = BLE_CONN_HANDLE_INVALID;
            led_off(LED_BLUE);
            /* Restart advertising so the device is discoverable again */
            sd_ble_gap_adv_start(adv_handle, BLE_CONN_CFG_TAG_DEFAULT);
            break;

            /* ---- Security (Just Works pairing for HID) ---- */

        case BLE_GAP_EVT_SEC_PARAMS_REQUEST: {
            /* Accept pairing with Just Works (no MITM, no display).
             * The central initiates pairing; we respond with our params
             * and a keyset so the SD has buffers for key exchange. */
            static ble_gap_enc_key_t own_enc_key;
            static ble_gap_id_key_t  own_id_key;
            static ble_gap_enc_key_t peer_enc_key;
            static ble_gap_id_key_t  peer_id_key;

            ble_gap_sec_keyset_t keyset;
            memset(&keyset, 0, sizeof(keyset));
            keyset.keys_own.p_enc_key  = &own_enc_key;
            keyset.keys_own.p_id_key   = &own_id_key;
            keyset.keys_peer.p_enc_key = &peer_enc_key;
            keyset.keys_peer.p_id_key  = &peer_id_key;

            ble_gap_sec_params_t sec_params;
            memset(&sec_params, 0, sizeof(sec_params));
            sec_params.bond         = 1;
            sec_params.mitm         = 0;
            sec_params.lesc         = 0;
            sec_params.keypress     = 0;
            sec_params.io_caps      = BLE_GAP_IO_CAPS_NONE;
            sec_params.oob          = 0;
            sec_params.min_key_size = 7;
            sec_params.max_key_size = 16;
            sec_params.kdist_own.enc  = 1;
            sec_params.kdist_own.id   = 1;
            sec_params.kdist_peer.enc = 1;
            sec_params.kdist_peer.id  = 1;

            sd_ble_gap_sec_params_reply(
                evt->evt.gap_evt.conn_handle,
                BLE_GAP_SEC_STATUS_SUCCESS,
                &sec_params, &keyset);
            break;
        }

        case BLE_GAP_EVT_AUTH_STATUS:
            /* Pairing complete (success or failure).
             * We don't persist bonds yet — that comes with flash storage. */
            break;

        case BLE_GAP_EVT_SEC_INFO_REQUEST:
            /* Central is asking for stored bond keys (reconnection).
             * We don't persist bonds yet, so reply with NULLs. */
            sd_ble_gap_sec_info_reply(
                evt->evt.gap_evt.conn_handle,
                NULL, NULL, NULL);
            break;

        case BLE_GAP_EVT_CONN_SEC_UPDATE:
            /* Connection security level changed — nothing to do. */
            break;

            /* ---- Connection parameter negotiation ---- */

        case BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST:
        {
            ble_gap_conn_params_t *p =
                &evt->evt.gap_evt.params.conn_param_update_request.conn_params;
            sd_ble_gap_conn_param_update(
                evt->evt.gap_evt.conn_handle, p);
            break;
        }

            /* ---- PHY / data length updates (accept defaults) ---- */

        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
        {
            ble_gap_phys_t phys = {
                .tx_phys = BLE_GAP_PHY_AUTO,
                .rx_phys = BLE_GAP_PHY_AUTO,
            };
            sd_ble_gap_phy_update(
                evt->evt.gap_evt.conn_handle, &phys);
            break;
        }

        case BLE_GAP_EVT_DATA_LENGTH_UPDATE_REQUEST:
            sd_ble_gap_data_length_update(
                evt->evt.gap_evt.conn_handle, NULL, NULL);
            break;

            /* ---- GATT events ---- */

        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            sd_ble_gatts_sys_attr_set(
                evt->evt.gatts_evt.conn_handle, NULL, 0, 0);
            break;

        case BLE_GATTS_EVT_WRITE: {
            /* Forward CCCD writes (and any other GATTS writes)
             * to the HID service so it can track notification state. */
            ble_gatts_evt_write_t *w = &evt->evt.gatts_evt.params.write;
            hid_service_on_write(w->handle, w->data, w->len);
            break;
        }

        case BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST:
            sd_ble_gatts_exchange_mtu_reply(
                evt->evt.gatts_evt.conn_handle, 23);
            break;

            /* ---- Catch-all ---- */

        default:
            break;
        }
    }
}

/* ---- NVIC wrappers ----
 * The SoftDevice intercepts NVIC register writes.  These wrappers
 * call the SD-safe inline functions from nrf_nvic.h so that other
 * modules (gpiote, etc.) don't need to include SDK headers. */

#include "nrf_nvic.h"

/* Global state variable required by sd_nvic_* inline functions.
 * The SDK normally expects the app to define this somewhere. */
nrf_nvic_state_t nrf_nvic_state = {0};

void sd_nvic_irq_enable(uint8_t irqn)
{
    sd_nvic_EnableIRQ((IRQn_Type)irqn);
}

void sd_nvic_irq_disable(uint8_t irqn)
{
    sd_nvic_DisableIRQ((IRQn_Type)irqn);
}

void sd_nvic_irq_clear_pending(uint8_t irqn)
{
    sd_nvic_ClearPendingIRQ((IRQn_Type)irqn);
}

void sd_nvic_irq_set_priority(uint8_t irqn, uint8_t priority)
{
    sd_nvic_SetPriority((IRQn_Type)irqn, (uint32_t)priority);
}
