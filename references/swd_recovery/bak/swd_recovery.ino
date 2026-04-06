/*
 * SWD Bootloader Repair for Adafruit Feather nRF52832
 *
 * The bootloader lives at 0x74000 (per UICR).  Our firmware's
 * bond_save() erased page 0x77000, which is bootloader code.
 * This sketch writes back the correct data from bootloader v0.9.1
 * (feather_nrf52832_bootloader-0.9.1_s132_6.1.1.hex).
 *
 * After restoring the page, it erases:
 *   - 0x7F000  bootloader settings  (forces DFU on next boot)
 *   - 0x7E000  MBR params
 *   - 0x26000  app start page       (prevents boot to crashed app)
 *
 * Wiring (Pro Micro -> Feather nRF52832):
 *   Pin 10 -> SWD  (SWDIO)
 *   Pin 9  -> SWC  (SWDCLK)
 *   GND    -> GND
 */

#include "Adafruit_DAP.h"
#include "bl_page_77000.h"

#define SWDIO_PIN  10
#define SWDCLK_PIN 9
#define SWDRST_PIN 8   /* not driven, library needs it */

/* nRF52832 NVMC registers */
#define NVMC_READY     0x4001E400
#define NVMC_CONFIG    0x4001E504
#define NVMC_ERASEPAGE 0x4001E508

/* Pages */
#define SETTINGS_PAGE    0x0007F000
#define MBR_PARAMS_PAGE  0x0007E000
#define APP_START_PAGE   0x00026000

/* Cortex-M4 debug */
#define DHCSR 0xE000EDF0
#define DEMCR 0xE000EDFC

/* SWD DP registers */
#define SWD_DP_W_ABORT      0x00
#define SWD_DP_W_CTRL_STAT  0x04
#define SWD_DP_R_CTRL_STAT  0x06
#define SWD_DP_W_SELECT     0x08
#define SWD_AP_CSW          0x01

/* CTRL/STAT bits */
#define CDBGPWRUPREQ  (1UL << 28)
#define CDBGPWRUPACK  (1UL << 29)
#define CSYSPWRUPREQ  (1UL << 30)
#define CSYSPWRUPACK  (1UL << 31)
#define POWERUP_REQ   (CDBGPWRUPREQ | CSYSPWRUPREQ | 0x00000F00)
#define POWERUP_ACK   (CDBGPWRUPACK | CSYSPWRUPACK)

Adafruit_DAP_nRF5x dap;
bool done = false;

void error_cb(const char *text) {
  (void)text;  /* silent — Serial inside error_cb kills SWD timing */
}

/*
 * Interrupt-safe SWD wrappers.
 * USB interrupts on ATmega32U4 fire every 1ms (SOF) and can hit
 * mid-bit-toggle during SWD bit-bang, stretching one clock cycle
 * by the ISR duration (~10µs) while normal cycles are ~1µs.
 * After a few such glitches the target DP enters error state and
 * the bus is dead.  Wrapping each SWD transaction in cli/sei
 * makes the bit-bang immune to interrupt jitter.
 */
static uint32_t swd_read_word(uint32_t addr) {
  noInterrupts(); uint32_t v = dap.dap_read_word(addr); interrupts();
  return v;
}
static void swd_write_word(uint32_t addr, uint32_t val) {
  noInterrupts(); dap.dap_write_word(addr, val); interrupts();
}
static uint32_t swd_read_reg(uint8_t reg) {
  noInterrupts(); uint32_t v = dap.dap_read_reg(reg); interrupts();
  return v;
}
static void swd_write_reg(uint8_t reg, uint32_t val) {
  noInterrupts(); dap.dap_write_reg(reg, val); interrupts();
}
static bool swd_reset_link(void) {
  noInterrupts(); bool ok = dap.dap_reset_link(); interrupts();
  return ok;
}

/* Wait for NVMC READY, returns true if ready */
static bool nvmc_wait(uint16_t timeout_ms) {
  for (uint16_t i = 0; i < timeout_ms; i++) {
    if (swd_read_word(NVMC_READY) & 1) return true;
    delay(1);
  }
  Serial.println(F("NVMC timeout"));
  return false;
}

/* Erase one 4KB flash page */
static bool erase_page(uint32_t addr) {
  swd_write_word(NVMC_CONFIG, 2);   /* erase enable */
  delay(10);
  if (!nvmc_wait(500)) return false;

  swd_write_word(NVMC_ERASEPAGE, addr);
  delay(100);
  if (!nvmc_wait(1000)) return false;

  swd_write_word(NVMC_CONFIG, 0);   /* read-only */
  delay(10);
  return true;
}

/*
 * Power up debug/system domains manually.
 * Library's dap_target_prepare() doesn't poll for ACK.
 * Retries with full link resets — the previous session's bus death
 * can leave the target DP in a wedged state that clears after
 * a fresh JTAG-to-SWD sequence.
 */
static bool manual_target_prepare(void) {
  dap.dap_quiet = true;

  for (int attempt = 0; attempt < 5; attempt++) {
    if (attempt > 0) {
      /* Full link reset between retries — clears DP state machine */
      delay(200);
      swd_reset_link();
      delay(100);
    }

    swd_write_reg(SWD_DP_W_ABORT, 0x1E);
    swd_write_reg(SWD_DP_W_SELECT, 0x00000000);
    swd_write_reg(SWD_DP_W_CTRL_STAT, POWERUP_REQ);

    for (int i = 0; i < 100; i++) {
      uint32_t stat = swd_read_reg(SWD_DP_R_CTRL_STAT);
      if ((stat & POWERUP_ACK) == POWERUP_ACK) {
        Serial.print(F("powered up... "));
        swd_write_reg(SWD_AP_CSW, 0x23000052);
        return true;
      }
      delay(10);
    }

    Serial.print(F("(retry) "));
  }

  Serial.println(F("power-up ACK timeout — try power-cycling the Feather"));
  return false;
}

/*
 * Reinitialize the SWD bus from scratch.
 * The bit-banged SWD bus on ATmega32U4 accumulates timing errors
 * and dies after ~300 consecutive operations.  Calling this
 * periodically during long write/read sequences keeps it alive.
 * Target CPU stays halted; NVMC CONFIG register persists in HW.
 */
static bool swd_reinit(void) {
  for (int attempt = 0; attempt < 3; attempt++) {
    if (attempt > 0) delay(100);

    swd_write_reg(SWD_DP_W_ABORT, 0x1E);  /* best-effort */
    swd_reset_link();
    delay(20);  /* let DP state machine settle after line reset */

    swd_write_reg(SWD_DP_W_ABORT, 0x1E);
    delay(5);
    swd_write_reg(SWD_DP_W_SELECT, 0x00000000);
    swd_write_reg(SWD_DP_W_CTRL_STAT, POWERUP_REQ);

    for (int i = 0; i < 50; i++) {
      uint32_t stat = swd_read_reg(SWD_DP_R_CTRL_STAT);
      if ((stat & POWERUP_ACK) == POWERUP_ACK) {
        swd_write_reg(SWD_AP_CSW, 0x23000052);
        /* CPU should still be halted (DHCSR persists), but poke it */
        swd_write_word(DHCSR, 0xA05F0003);
        return true;
      }
      delay(10);
    }
  }
  return false;
}

/*
 * Write a PROGMEM page to nRF52 flash.
 * Erases first, writes word-by-word, reinits SWD every 64 words.
 */
static bool write_page(uint32_t addr, const uint32_t *pgm_data,
                       uint16_t word_count) {
  /* Fresh bus for the erase — previous operations may have
   * degraded the link */
  if (!swd_reinit()) {
    Serial.println(F("  reinit before erase failed"));
    return false;
  }

  /* Erase */
  Serial.print(F("  Erase... "));
  if (!erase_page(addr)) return false;
  Serial.println(F("OK"));

  /* Enable write mode */
  swd_write_word(NVMC_CONFIG, 1);
  delay(10);
  if (!nvmc_wait(500)) return false;

  /* Write all words in batches of 32 with full SWD bus reinit
   * between batches.  The bit-banged SWD link on ATmega32U4
   * dies after ~300 consecutive operations; at ~4 low-level ops
   * per word, 32 words = ~128 ops — safe margin. */
  Serial.print(F("  Write "));
  Serial.print(word_count);
  Serial.print(F(" words "));
  Serial.flush();

  for (uint16_t i = 0; i < word_count; i++) {
    /* Full SWD bus reinit every 32 words */
    if (i > 0 && (i & 0x1F) == 0) {
      nvmc_wait(100);
      if (!swd_reinit()) {
        Serial.print(F("reinit fail@"));
        Serial.println(i);
        return false;
      }
      /* NVMC CONFIG=1 (write enable) persists in HW, but
       * re-assert it after bus reinit just to be safe */
      swd_write_word(NVMC_CONFIG, 1);
      delay(1);
    }

    uint32_t word = pgm_read_dword(&pgm_data[i]);
    swd_write_word(addr + ((uint32_t)i * 4), word);
    delay(1);

    /* NVMC check every 16 words */
    if ((i & 0x0F) == 0x0F) {
      swd_write_reg(SWD_DP_W_ABORT, 0x1E);
      if (!nvmc_wait(100)) {
        Serial.print(F("nvmc fail@"));
        Serial.println(i);
        return false;
      }
    }

    /* progress dot every 32 words */
    if (i > 0 && (i & 0x1F) == 0) {
      Serial.print('.');
      Serial.flush();
    }
  }

  /* Final NVMC wait */
  if (!nvmc_wait(500)) {
    return false;
  }

  /* Back to read-only */
  swd_write_word(NVMC_CONFIG, 0);
  delay(10);
  Serial.println(F("OK"));
  return true;
}

/*
 * Verify a page against PROGMEM data.
 */
static bool verify_page(uint32_t addr, const uint32_t *pgm_data,
                        uint16_t word_count) {
  Serial.print(F("  Verify "));
  Serial.flush();
  uint16_t errors = 0;

  for (uint16_t i = 0; i < word_count; i++) {
    /* Full SWD bus reinit every 32 reads */
    if (i > 0 && (i & 0x1F) == 0) {
      if (!swd_reinit()) {
        Serial.print(F("reinit fail@"));
        Serial.println(i);
        return false;
      }
    }

    uint32_t expected = pgm_read_dword(&pgm_data[i]);
    uint32_t actual   = swd_read_word(addr + ((uint32_t)i * 4));
    delay(1);  /* reduce timing pressure between reads */
    if (actual != expected) {
      errors++;
      if (errors <= 3) {
        Serial.println();
        Serial.print(F("    @0x"));
        Serial.print(addr + ((uint32_t)i * 4), HEX);
        Serial.print(F(" exp=0x"));
        Serial.print(expected, HEX);
        Serial.print(F(" got=0x"));
        Serial.print(actual, HEX);
        Serial.flush();
      }
    }

    /* progress dot every 32 words */
    if (i > 0 && (i & 0x1F) == 0) {
      Serial.print('.');
      Serial.flush();
    }
  }

  if (errors == 0) {
    Serial.println(F("OK (1024 words match)"));
    return true;
  }
  Serial.println();
  Serial.print(F("  FAILED: "));
  Serial.print(errors);
  Serial.println(F(" mismatches"));
  return false;
}

/*
 * Erase a page if it's not already erased.
 */
static void erase_if_needed(uint32_t addr, const __FlashStringHelper *name) {
  uint32_t val = swd_read_word(addr);
  Serial.print(name);
  Serial.print(F(" [0x"));
  Serial.print(addr, HEX);
  Serial.print(F("] = 0x"));
  Serial.println(val, HEX);

  if (val != 0xFFFFFFFF) {
    Serial.print(F("  Erasing... "));
    if (erase_page(addr)) {
      Serial.println(F("OK"));
    } else {
      Serial.println(F("FAIL"));
    }
  } else {
    Serial.println(F("  (already erased)"));
  }
}

static void run_recovery(void) {
  if (done) return;

  Serial.println(F("=== nRF52 Bootloader Repair ==="));
  Serial.println();

  /* --- SWD connect --- */
  Serial.print(F("SWD connect... "));
  Serial.flush();

  dap.begin(SWDCLK_PIN, SWDIO_PIN, SWDRST_PIN, error_cb);
  if (!dap.dap_connect()) {
    Serial.println(F("FAILED"));
    dap.dap_disconnect();
    return;
  }
  dap.dap_transfer_configure(8, 1000, 128);
  dap.dap_swd_configure(0);
  dap.dap_swj_clock(50);

  if (!swd_reset_link()) {
    Serial.println(F("link FAILED"));
    dap.dap_disconnect();
    return;
  }
  Serial.print(F("linked... "));

  if (!manual_target_prepare()) {
    dap.dap_disconnect();
    return;
  }
  Serial.println(F("OK"));

  /* dap_quiet=true for the ENTIRE session.  Our own Serial.print
   * calls are unaffected; only the library's internal error prints
   * inside dap_read_reg/dap_write_reg are suppressed.  This prevents
   * USB CDC interrupts (from Serial) corrupting bit-banged SWD timing. */
  dap.dap_quiet = true;

  /* --- Halt CPU (must survive crash-loop resets) --- */
  /* Fresh bus reinit to start with a clean operation counter */
  swd_reinit();

  /* 1. Enable vector-catch on reset */
  swd_write_word(DEMCR, 0x01000001);  /* VC_CORERESET | TRCENA */
  delay(10);
  /* 2. Try halting now */
  swd_write_word(DHCSR, 0xA05F0003);  /* DBGKEY | C_HALT | C_DEBUGEN */
  delay(100);
  swd_write_word(DHCSR, 0xA05F0003);
  delay(50);
  uint32_t dhcsr = swd_read_word(DHCSR);
  if (!(dhcsr & (1UL << 17))) {
    /* Still not halted — reinit bus, force reset, let vector catch fire */
    Serial.print(F("forcing reset... "));
    swd_reinit();
    swd_write_word(DEMCR, 0x01000001);  /* re-assert vector catch */
    delay(10);
    swd_write_word(0xE000ED0C, 0x05FA0004); /* AIRCR SYSRESETREQ */
    delay(200);
    swd_reinit();  /* reconnect after reset */
    swd_write_word(DHCSR, 0xA05F0003);
    delay(50);
    dhcsr = swd_read_word(DHCSR);
  }
  Serial.print(F("CPU halt: "));
  Serial.println((dhcsr & (1UL << 17)) ? F("OK") : F("FAILED (continuing)"));

  /* --- Sanity check --- */
  swd_reinit();  /* fresh bus for reads */
  uint32_t mbr = swd_read_word(0x00000000);
  Serial.print(F("MBR [0x00000] = 0x"));
  Serial.println(mbr, HEX);
  if (mbr == 0 || mbr == 0xFFFFFFFF) {
    Serial.println(F("SWD reads broken, aborting."));
    dap.dap_disconnect();
    return;
  }

  /* --- Write bootloader page 0x77000 unconditionally --- */
  /* Don't waste bus lifetime on a pre-verify.  The page has known
   * corruption (0x77038).  Erase + write with a fresh bus, then
   * verify afterwards. */
  Serial.println(F("--- Restoring page 0x77000 ---"));
  if (!write_page(BL_PAGE_ADDR, bl_page_data, BL_PAGE_WORDS)) {
    Serial.println(F("WRITE FAILED - do NOT power cycle!"));
    dap.dap_disconnect();
    return;
  }

  /* --- Verify --- */
  swd_reinit();  /* fresh bus for verify */
  if (!verify_page(BL_PAGE_ADDR, bl_page_data, BL_PAGE_WORDS)) {
    Serial.println(F("VERIFY FAILED - do NOT power cycle!"));
    dap.dap_disconnect();
    return;
  }

  Serial.println(F("--- Page restored! ---"));
  Serial.println();

cleanup_pages:
  /* --- Erase settings/params/app so bootloader enters DFU --- */
  Serial.println(F("--- Cleanup ---"));
  swd_reinit();  /* fresh bus for cleanup */
  erase_if_needed(SETTINGS_PAGE,   F("Settings"));
  erase_if_needed(MBR_PARAMS_PAGE, F("MBR params"));
  erase_if_needed(APP_START_PAGE,  F("App start"));

  /* --- Done --- */
  Serial.println();
  Serial.println(F("============================="));
  Serial.println(F("  SUCCESS - bootloader fixed"));
  Serial.println(F("============================="));
  Serial.println();
  Serial.println(F("Next steps:"));
  Serial.println(F("  1. Disconnect SWD wires"));
  Serial.println(F("  2. Reset Feather (unplug/replug)"));
  Serial.println(F("  3. Red LED should pulse (DFU)"));
  Serial.println(F("  4. Run: make flash"));
  done = true;
  dap.dap_disconnect();
}

void setup() {
  Serial.begin(115200);
  delay(2000);
}

void loop() {
  run_recovery();
  delay(done ? 1000 : 3000);
}
