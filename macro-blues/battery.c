#include <stdint.h>
#include <string.h>

#include "ble.h"
#include "ble_gatts.h"
#include "ble_gap.h"
#include "nrf_error.h"

#include "battery.h"
#include "ble_stack.h"
#include "board.h"
#include "nrf52832.h"
#include "led.h"

/*
 * battery.c — LiPo battery monitoring + BLE Battery Service
 *
 * SAADC configuration:
 *   - Channel 0, positive input = AIN7 (P0.31)
 *   - Gain = 1/6  →  input range 0 – 3.6 V
 *   - Reference = internal 0.6 V
 *   - Resolution = 10-bit (0 – 1023)
 *   - Single-ended, single-shot (no continuous sampling)
 *
 * The Feather routes VBAT through a 2:1 resistor divider to AIN7,
 * so the voltage at the pin is VBAT / 2.  To recover millivolts:
 *
 *   V(pin)  = result × 3600 / 1024          (gain=1/6, ref=0.6V)
 *   V(bat)  = V(pin) × BAT_DIVIDER_NUM / BAT_DIVIDER_DEN
 *
 * Battery: EEMB LP103454-PCM-LD  3.7V / 2000mAh with PCM
 */

/* ---- BLE Battery Service state ---- */

static uint16_t bas_level_handle;  /* value handle for Battery Level */
static uint16_t bas_cccd_handle;   /* CCCD handle for notifications  */
static uint8_t  bas_notify_enabled;

/* ---- Battery measurement state ---- */

static uint8_t  last_percent;
static uint16_t last_mv;

/* ---- SAADC single-shot read ---- */

static int16_t saadc_read_ain7(void)
{
	volatile int16_t result;

	/* Configure channel 0 for AIN7, single-ended */
	SAADC_CH0_PSELP  = SAADC_PSEL_AIN7;
	SAADC_CH0_PSELN  = SAADC_PSEL_NC;
	SAADC_CH0_CONFIG = SAADC_CONFIG_GAIN_1_6
	                 | SAADC_CONFIG_REFSEL_INT
	                 | SAADC_CONFIG_TACQ_40US
	                 | SAADC_CONFIG_MODE_SE;

	SAADC_RESOLUTION     = SAADC_RES_10BIT;
	SAADC_RESULT_PTR     = (uint32_t)&result;
	SAADC_RESULT_MAXCNT  = 1;

	/* Enable, start, sample, wait, stop, disable */
	SAADC_ENABLE = 1;

	SAADC_EVENTS_STARTED = 0;
	SAADC_TASKS_START    = 1;
	while (!SAADC_EVENTS_STARTED)
		;

	SAADC_EVENTS_END   = 0;
	SAADC_TASKS_SAMPLE = 1;
	while (!SAADC_EVENTS_END)
		;

	SAADC_EVENTS_STOPPED = 0;
	SAADC_TASKS_STOP     = 1;
	while (!SAADC_EVENTS_STOPPED)
		;

	SAADC_ENABLE = 0;

	return (result < 0) ? 0 : result;
}

/* ---- Voltage → percentage (piecewise-linear LiPo curve) ----
 *
 * LiPo discharge is non-linear.  A simple linear mapping from
 * 3.0–4.2 V over-reports in the mid-range and under-reports at
 * the knee.  This 5-segment approximation matches typical LiPo
 * discharge curves reasonably well:
 *
 *   4.20 V = 100%      3.80 V =  60%
 *   4.10 V =  90%      3.60 V =  20%
 *   3.95 V =  75%      3.00 V =   0%
 */
static uint8_t mv_to_percent(uint16_t mv)
{
	if (mv >= 4200) return 100;
	if (mv >= 4100) return 90 + (uint32_t)(mv - 4100) * 10 / 100;
	if (mv >= 3950) return 75 + (uint32_t)(mv - 3950) * 15 / 150;
	if (mv >= 3800) return 60 + (uint32_t)(mv - 3800) * 15 / 150;
	if (mv >= 3600) return 20 + (uint32_t)(mv - 3600) * 40 / 200;
	if (mv >= 3000) return      (uint32_t)(mv - 3000) * 20 / 600;
	return 0;
}

/* ---- BLE Battery Service (UUID 0x180F) ---- */

static void bas_error_blink(int n, uint32_t err)
{
	volatile uint32_t d;
	while (1)
	{
		for (int i = 0; i < n; i++)
		{
			led_on(LED_RED);
			for (d = 0; d < 1600000; d++) ;
			led_off(LED_RED);
			for (d = 0; d < 1600000; d++) ;
		}
		for (d = 0; d < 3000000; d++) ;
		for (uint32_t i = 0; i < err; i++)
		{
			led_on(LED_BLUE);
			for (d = 0; d < 1600000; d++) ;
			led_off(LED_BLUE);
			for (d = 0; d < 1600000; d++) ;
		}
		for (d = 0; d < 5000000; d++) ;
	}
}

void battery_service_init(void)
{
	uint32_t err;
	uint16_t svc_handle;

	ble_uuid_t svc_uuid = {.uuid = 0x180F, .type = BLE_UUID_TYPE_BLE};
	err = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
	                               &svc_uuid, &svc_handle);
	if (err != NRF_SUCCESS)
		bas_error_blink(13, err);

	/* Battery Level characteristic (0x2A19): read + notify, 1 byte 0-100 */
	static uint8_t initial_level = 100;

	ble_gatts_char_md_t char_md;
	memset(&char_md, 0, sizeof(char_md));
	char_md.char_props.read   = 1;
	char_md.char_props.notify = 1;

	ble_gatts_attr_md_t cccd_md;
	memset(&cccd_md, 0, sizeof(cccd_md));
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);
	cccd_md.vloc = BLE_GATTS_VLOC_STACK;
	char_md.p_cccd_md = &cccd_md;

	ble_gatts_attr_md_t attr_md;
	memset(&attr_md, 0, sizeof(attr_md));
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
	BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.write_perm);
	attr_md.vloc = BLE_GATTS_VLOC_STACK;

	ble_uuid_t uuid = {.uuid = 0x2A19, .type = BLE_UUID_TYPE_BLE};

	ble_gatts_attr_t attr_val = {
		.p_uuid    = &uuid,
		.p_attr_md = &attr_md,
		.init_len  = 1,
		.max_len   = 1,
		.p_value   = &initial_level,
	};

	ble_gatts_char_handles_t handles;
	err = sd_ble_gatts_characteristic_add(svc_handle, &char_md,
	                                      &attr_val, &handles);
	if (err != NRF_SUCCESS)
		bas_error_blink(14, err);

	bas_level_handle  = handles.value_handle;
	bas_cccd_handle   = handles.cccd_handle;
	bas_notify_enabled = 0;
}

/* ---- Public API ---- */

void battery_init(void)
{
	/* Take an initial reading so battery_percent() is valid immediately */
	int16_t raw = saadc_read_ain7();
	uint32_t pin_mv = (uint32_t)raw * 3600 / 1024;
	last_mv      = (uint16_t)(pin_mv * BAT_DIVIDER_NUM / BAT_DIVIDER_DEN);
	last_percent = mv_to_percent(last_mv);

	/* Push the initial level into the GATT attribute so a read
	 * before the first update returns a real value. */
	ble_gatts_value_t val = {
		.len     = 1,
		.offset  = 0,
		.p_value = &last_percent,
	};
	sd_ble_gatts_value_set(BLE_CONN_HANDLE_INVALID,
	                       bas_level_handle, &val);
}

void battery_update(void)
{
	int16_t raw = saadc_read_ain7();
	uint32_t pin_mv = (uint32_t)raw * 3600 / 1024;
	uint16_t mv = (uint16_t)(pin_mv * BAT_DIVIDER_NUM / BAT_DIVIDER_DEN);
	uint8_t pct = mv_to_percent(mv);

	last_mv = mv;

	/* Only push a GATT update when the percentage actually changes.
	 * Avoids flooding the link with identical notifications. */
	if (pct == last_percent)
		return;

	last_percent = pct;

	/* Update the stored attribute value (for reads) */
	ble_gatts_value_t val = {
		.len     = 1,
		.offset  = 0,
		.p_value = &last_percent,
	};
	sd_ble_gatts_value_set(BLE_CONN_HANDLE_INVALID,
	                       bas_level_handle, &val);

	/* Send a notification if the central subscribed */
	if (bas_notify_enabled && ble_stack_connected())
	{
		uint16_t len = 1;
		ble_gatts_hvx_params_t hvx = {
			.handle = bas_level_handle,
			.type   = BLE_GATT_HVX_NOTIFICATION,
			.offset = 0,
			.p_len  = &len,
			.p_data = &last_percent,
		};
		sd_ble_gatts_hvx(ble_stack_conn_handle(), &hvx);
	}
}

uint8_t battery_percent(void)
{
	return last_percent;
}

uint16_t battery_voltage_mv(void)
{
	return last_mv;
}

int battery_low(void)
{
	return last_mv < BAT_MV_LOW;
}

void battery_on_write(uint16_t handle, const uint8_t *data, uint16_t len)
{
	if (handle == bas_cccd_handle && len >= 2)
		bas_notify_enabled = (data[0] & 0x01);
}
