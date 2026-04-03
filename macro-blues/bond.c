#include <stdint.h>
#include <string.h>

#include "nrf_soc.h"
#include "nrf_error.h"
#include "ble_gap.h"

#include "bond.h"

/*
 * bond.c — Persistent bond key storage in flash
 *
 * Uses a flash page BELOW the Adafruit bootloader for bond storage.
 *
 * nRF52832 flash map (Adafruit Feather):
 *   0x00000–0x25FFF  SoftDevice S132
 *   0x26000–0x77FFF  Application (our code — currently only a few KB)
 *   0x78000–0x7DFFF  Adafruit bootloader (~24 KB)
 *   0x7E000          MBR parameters page (DO NOT TOUCH)
 *   0x7F000          Bootloader settings page (DO NOT TOUCH)
 *
 * We use page 0x77000 — the last 4 KB page in the application region,
 * safely below the bootloader.
 *
 * Double-tap reset does NOT work on nRF52832 because "all its SRAM got cleared with GPIO reset" (from the bootloader source code comment)
 *
 * Flash layout at BOND_PAGE:
 *   [0x00] uint32_t  magic       — BOND_MAGIC if valid data present
 *   [0x04] enc_info  (28 bytes)  — LTK, auth, key size, etc
 *   [0x20] master_id (10 bytes)  — EDIV + rand
 *   [0x2C] irk       (16 bytes)  — peer IRK
 *   [0x3C] addr      (7 bytes)   — peer address
 *
 * All flash writes go through sd_flash_write / sd_flash_page_erase,
 * which the SoftDevice schedules around radio activity.  We busy-wait
 * for the NRF_EVT_FLASH_OPERATION_SUCCESS event for simplicity.
 */

#define BOND_PAGE 0x77000
#define BOND_MAGIC 0xB09D0001 /* "BOND" version 1 */

/* On-flash bond record.  Must be a multiple of 4 bytes for sd_flash_write. */
typedef struct __attribute__((packed, aligned(4)))
{
    uint32_t magic;
    ble_gap_enc_info_t enc_info;   /* LTK + metadata */
    uint8_t _pad1[2];              /* align to 4 */
    ble_gap_master_id_t master_id; /* EDIV + rand */
    uint8_t _pad2[2];              /* align to 4 */
    ble_gap_irk_t irk;             /* peer IRK */
    ble_gap_addr_t addr;           /* peer address */
    uint8_t _pad3[1];              /* align to 4 */
} bond_record_t;

_Static_assert(sizeof(bond_record_t) % 4 == 0,
               "bond_record_t must be a multiple of 4 bytes for flash writes");

/* RAM copy of the stored bond */
static bond_record_t bond;
static int bond_valid;

/* ---- Flash helpers ---- */

/*
 * Wait for the SoftDevice to complete a flash operation.
 * sd_flash_page_erase and sd_flash_write are asynchronous — the SD
 * posts NRF_EVT_FLASH_OPERATION_SUCCESS/ERROR to the SoC event queue.
 */
static void wait_for_flash(void)
{
    while (1)
    {
        uint32_t evt;
        uint32_t err = sd_evt_get(&evt);
        if (err == NRF_SUCCESS)
        {
            if (evt == NRF_EVT_FLASH_OPERATION_SUCCESS)
                return;
            if (evt == NRF_EVT_FLASH_OPERATION_ERROR)
                return; /* best-effort */
        }
        /* Let the CPU sleep while waiting — saves power and lets the
         * SD do its thing. */
        sd_app_evt_wait();
    }
}

/* ---- Public API ---- */

void bond_init(void)
{
    /* Read the flash page into RAM */
    const bond_record_t *flash = (const bond_record_t *)BOND_PAGE;
    memcpy(&bond, flash, sizeof(bond));

    bond_valid = (bond.magic == BOND_MAGIC);
}

int bond_is_stored(void)
{
    return bond_valid;
}

void bond_save(const ble_gap_enc_key_t *own_enc,
               const ble_gap_id_key_t *peer_id)
{
    /* Build the record in RAM */
    memset(&bond, 0, sizeof(bond));
    bond.magic = BOND_MAGIC;
    bond.enc_info = own_enc->enc_info;
    bond.master_id = own_enc->master_id;
    if (peer_id)
    {
        bond.irk = peer_id->id_info;
        bond.addr = peer_id->id_addr_info;
    }

    /* Erase the page (sets all bits to 1) */
    sd_flash_page_erase(BOND_PAGE / 4096);
    wait_for_flash();

    /* Write the record (length in 32-bit words) */
    sd_flash_write((uint32_t *)BOND_PAGE,
                   (const uint32_t *)&bond,
                   sizeof(bond) / 4);
    wait_for_flash();

    bond_valid = 1;
}

void bond_delete(void)
{
    sd_flash_page_erase(BOND_PAGE / 4096);
    wait_for_flash();
    memset(&bond, 0xFF, sizeof(bond));
    bond_valid = 0;
}

const ble_gap_enc_info_t *bond_enc_info(void)
{
    return bond_valid ? &bond.enc_info : NULL;
}

const ble_gap_master_id_t *bond_master_id(void)
{
    return bond_valid ? &bond.master_id : NULL;
}

const ble_gap_irk_t *bond_peer_irk(void)
{
    return bond_valid ? &bond.irk : NULL;
}

const ble_gap_addr_t *bond_peer_addr(void)
{
    return bond_valid ? &bond.addr : NULL;
}
