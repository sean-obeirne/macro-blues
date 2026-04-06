#ifndef BOND_H
#define BOND_H

/*
 * bond.h — Persistent bond key storage
 *
 * Stores one BLE bond (LTK + peer identity) in the last page before
 * the bootloader (0x73000).  Bootloader starts at 0x74000 per UICR.
 * Flash writes go through the SoftDevice's sd_flash_* API, which
 * handles timing constraints and coexistence with radio activity.
 *
 * Only one bond is stored at a time.  A new pairing overwrites the old
 * one.  This is fine for a personal keyboard — you typically only pair
 * with one device at a time.
 */

#include <stdint.h>
#include "ble_gap.h"

/* Initialize the bond module.  Reads flash to check for a stored bond. */
void bond_init(void);

/* Returns 1 if a valid bond is stored in flash. */
int bond_is_stored(void);

/* Save the bond keys from a completed pairing.
 * Call from BLE_GAP_EVT_AUTH_STATUS when status == SUCCESS. */
void bond_save(const ble_gap_enc_key_t *own_enc,
               const ble_gap_id_key_t *peer_id);

/* Delete the stored bond (erase flash page). */
void bond_delete(void);

/* Get the stored encryption info (LTK + master ID).
 * Returns NULL if no bond is stored. */
const ble_gap_enc_info_t *bond_enc_info(void);
const ble_gap_master_id_t *bond_master_id(void);

/* Get the stored peer identity (IRK + address).
 * Returns NULL if no bond is stored. */
const ble_gap_irk_t *bond_peer_irk(void);
const ble_gap_addr_t *bond_peer_addr(void);

#endif /* BOND_H */
