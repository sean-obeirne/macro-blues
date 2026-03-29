#ifndef BLE_STACK_H
#define BLE_STACK_H

/*
 * ble_stack.h — SoftDevice BLE initialization and event handling
 *
 * Wraps all SoftDevice / BLE calls into a small, focused API.
 * Handles:  SD enable → BLE enable → GAP config → advertising → events
 *
 * After ble_stack_init(), the device advertises as "Macro Blues" and
 * appears as a connectable BLE peripheral (HID keyboard appearance).
 * Pairing is rejected for now — that will be added with the HID service.
 *
 * The SoftDevice takes ownership of several peripherals once enabled:
 *   - RADIO, TIMER0, RTC0, RNG, ECB, CCM, AAR, SWI0-5
 *   - NVIC priorities 0, 1, and 4
 *   - LFCLK management (clock source + calibration)
 *
 * Because of this, the rest of the firmware must:
 *   - Use RTC1 (not RTC0) for timing
 *   - NOT start LFCLK manually (the SD handles it)
 *   - Use sd_app_evt_wait() instead of __WFE() for sleeping
 *   - Keep app interrupt priorities at 5, 6, or 7
 */

#include <stdint.h>

/* Initialize SoftDevice + BLE stack and begin advertising.
 * Call this BEFORE timer_init() (SD starts LFCLK, RTC depends on it). */
void ble_stack_init(void);

/* Drain all pending BLE events from the SoftDevice queue.
 * Call this frequently (every main-loop iteration). */
void ble_stack_process(void);

/* Returns 1 if a BLE central is currently connected. */
int ble_stack_connected(void);

/* Restart advertising (e.g. after disconnect or manual stop). */
void ble_stack_advertise(void);

/* Sleep until the next event (BLE, GPIOTE, etc).
 * Wraps sd_app_evt_wait() so main.c doesn't need SDK headers. */
void ble_stack_wait(void);

/* Returns the current BLE connection handle (BLE_CONN_HANDLE_INVALID if none).
 * Needed by GATT service modules (HID, battery, etc). */
uint16_t ble_stack_conn_handle(void);

/* ---- NVIC wrappers ----
 * The SoftDevice owns the NVIC — direct register writes are ignored.
 * These wrap sd_nvic_* so modules outside ble_stack.c don't need SDK headers. */
void sd_nvic_irq_enable(uint8_t irqn);
void sd_nvic_irq_disable(uint8_t irqn);
void sd_nvic_irq_clear_pending(uint8_t irqn);
void sd_nvic_irq_set_priority(uint8_t irqn, uint8_t priority);

#endif /* BLE_STACK_H */
