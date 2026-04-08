#ifndef BATTERY_H
#define BATTERY_H

/*
 * battery.h — LiPo battery monitoring + BLE Battery Service
 *
 * Uses the nRF52832 SAADC to read battery voltage on AIN7 (P0.31)
 * through the Feather's on-board voltage divider.  Exposes battery
 * level (0-100%) over the standard BLE Battery Service (UUID 0x180F).
 *
 * Battery: EEMB LP103454-PCM-LD  3.7V / 2000mAh
 *   - Built-in PCM handles over-charge, over-discharge, short-circuit
 *   - Voltage range: ~3.0V (cutoff) to 4.2V (full)
 *
 * Call order:
 *   battery_service_init();   // after ble_stack_init(), before advertising
 *   battery_init();           // after timer_init() (needs SAADC, no SD dep)
 *   battery_update();         // periodically from main loop
 */

#include <stdint.h>

/* Register the BLE Battery Service with the GATT server.
 * Must be called BEFORE ble_stack_advertise(). */
void battery_service_init(void);

/* Initialize the SAADC for battery voltage measurement.
 * Call after timer_init(). */
void battery_init(void);

/* Sample battery voltage, update percentage, and notify the central
 * if the level changed.  Designed to be called periodically (~30 s). */
void battery_update(void);

/* Returns the last-measured battery level (0-100%). */
uint8_t battery_percent(void);

/* Returns the last-measured battery voltage in millivolts. */
uint16_t battery_voltage_mv(void);

/* Returns 1 if battery is below BAT_MV_LOW threshold. */
int battery_low(void);

/* Forward BLE GATTS write events (for CCCD tracking).
 * Called from ble_stack_process(). */
void battery_on_write(uint16_t handle, const uint8_t *data, uint16_t len);

#endif /* BATTERY_H */
