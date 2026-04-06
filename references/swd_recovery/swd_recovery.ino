/*
 * SWD Bootloader Repair for Adafruit Feather nRF52832
 *
 * The bootloader lives at 0x74000 (per UICR).  Our firmware's
 * bond_save() erased page 0x77000, which is bootloader code.
 * This sketch writes back the correct data from bootloader v0.9.1
 * (feather_nrf52832_bootloader-0.9.1_s132_6.1.1.hex).
 *
 * Strategy: The bit-banged SWD on ATmega32U4 is unreliable past
 * ~20 operations due to USB interrupt jitter.  Instead of trying
 * to keep the bus alive, we do a FULL disconnect/reconnect cycle
 * for every small batch of 8 words.  Each batch starts from a
 * perfectly clean connection.  Slow (~2 min) but bulletproof.
 *
 * Wiring (Pro Micro -> Feather nRF52832):
 *   Pin 10 -> SWD  (SWDIO)
 *   Pin 9  -> SWC  (SWDCLK)
 *   GND    -> GND
 */

#include "Adafruit_DAP.h"
#include "bl_page_77000.h"

#define SWDIO_PIN 10
#define SWDCLK_PIN 9
#define SWDRST_PIN 8 /* not driven, library needs it */

/* nRF52832 NVMC registers */
#define NVMC_READY 0x4001E400
#define NVMC_CONFIG 0x4001E504
#define NVMC_ERASEPAGE 0x4001E508

/* Pages */
#define SETTINGS_PAGE 0x0007F000
#define MBR_PARAMS_PAGE 0x0007E000
#define APP_START_PAGE 0x00026000

/* Cortex-M4 debug */
#define DHCSR 0xE000EDF0
#define DEMCR 0xE000EDFC

/* SWD DP registers */
#define SWD_DP_W_ABORT 0x00
#define SWD_DP_W_CTRL_STAT 0x04
#define SWD_DP_R_CTRL_STAT 0x06
#define SWD_DP_W_SELECT 0x08
#define SWD_AP_CSW 0x01

/* CTRL/STAT bits */
#define CDBGPWRUPREQ (1UL << 28)
#define CDBGPWRUPACK (1UL << 29)
#define CSYSPWRUPREQ (1UL << 30)
#define CSYSPWRUPACK (1UL << 31)
#define POWERUP_REQ (CDBGPWRUPREQ | CSYSPWRUPREQ | 0x00000F00)
#define POWERUP_ACK (CDBGPWRUPACK | CSYSPWRUPACK)

/* How many words per batch.  Each word takes ~4 SWD transactions.
 * 8 words = ~32 ops.  The first ~20 ops after a fresh connect are
 * always reliable, plus overhead (power-up, halt, NVMC) = ~12 ops.
 * Total ~44 ops per batch — well within safe range. */
#define BATCH_SIZE 8

Adafruit_DAP_nRF5x dap;
bool done = false;

void error_cb(const char *text)
{
  (void)text;
}

/* --------------------------------------------------------
 * Full SWD session: connect, power-up, halt, return true.
 * NO noInterrupts() during connection — the old sketch that
 * successfully connected never used it.  noInterrupts() only
 * matters during long read/write sequences later.
 * -------------------------------------------------------- */
static bool swd_open(void)
{
  dap.begin(SWDCLK_PIN, SWDIO_PIN, SWDRST_PIN, error_cb);

  if (!dap.dap_connect())
  {
    Serial.print(F("[conn]"));
    return false;
  }

  dap.dap_transfer_configure(8, 1000, 128);
  dap.dap_swd_configure(0);
  dap.dap_swj_clock(50);
  dap.dap_quiet = true;

  /* Retry power-up with full link resets between attempts */
  for (int attempt = 0; attempt < 5; attempt++)
  {
    dap.dap_reset_link();
    delay(20);

    dap.dap_write_reg(SWD_DP_W_ABORT, 0x1E);
    dap.dap_write_reg(SWD_DP_W_SELECT, 0x00000000);
    dap.dap_write_reg(SWD_DP_W_CTRL_STAT, POWERUP_REQ);

    for (int i = 0; i < 50; i++)
    {
      uint32_t stat = dap.dap_read_reg(SWD_DP_R_CTRL_STAT);
      if ((stat & POWERUP_ACK) == POWERUP_ACK)
      {
        dap.dap_write_reg(SWD_AP_CSW, 0x23000052);
        dap.dap_write_word(DHCSR, 0xA05F0003);
        return true;
      }
      delay(10);
    }
    delay(200);
  }

  Serial.print(F("[pwr]"));
  dap.dap_disconnect();
  return false;
}

static void swd_close(void)
{
  dap.dap_disconnect();
}

/* Single-word read/write */
static uint32_t swd_read(uint32_t addr)
{
  return dap.dap_read_word(addr);
}

static void swd_write(uint32_t addr, uint32_t val)
{
  dap.dap_write_word(addr, val);
}

/* --------------------------------------------------------
 * NVMC helpers (call within an open session)
 * -------------------------------------------------------- */
static bool nvmc_wait(uint16_t timeout_ms)
{
  for (uint16_t i = 0; i < timeout_ms; i++)
  {
    if (swd_read(NVMC_READY) & 1)
      return true;
    delay(1);
  }
  return false;
}

/* --------------------------------------------------------
 * Erase one 4KB page.  Opens its own session.
 * -------------------------------------------------------- */
static bool erase_page(uint32_t addr)
{
  if (!swd_open())
    return false;

  swd_write(NVMC_CONFIG, 2); /* erase enable */
  delay(10);
  if (!nvmc_wait(500))
  {
    swd_close();
    return false;
  }

  swd_write(NVMC_ERASEPAGE, addr);
  delay(100);
  if (!nvmc_wait(1000))
  {
    swd_close();
    return false;
  }

  swd_write(NVMC_CONFIG, 0); /* read-only */
  delay(10);
  swd_close();
  return true;
}

/* --------------------------------------------------------
 * Write BATCH_SIZE words starting at word index `start`.
 * Opens its own session, writes, closes.
 * -------------------------------------------------------- */
static bool write_batch(uint32_t base_addr, const uint32_t *pgm_data,
                        uint16_t start, uint16_t count)
{
  if (!swd_open())
    return false;

  /* Enable write mode */
  swd_write(NVMC_CONFIG, 1);
  delay(5);
  if (!nvmc_wait(200))
  {
    swd_close();
    return false;
  }

  /* Write words */
  for (uint16_t i = 0; i < count; i++)
  {
    uint32_t word = pgm_read_dword(&pgm_data[start + i]);
    swd_write(base_addr + ((uint32_t)(start + i) * 4), word);
    delayMicroseconds(500);
  }

  /* Wait for last write to complete */
  if (!nvmc_wait(200))
  {
    swd_close();
    return false;
  }

  /* Back to read-only */
  swd_write(NVMC_CONFIG, 0);
  delay(5);

  swd_close();
  return true;
}

/* --------------------------------------------------------
 * Verify BATCH_SIZE words starting at word index `start`.
 * Opens its own session, reads, closes.
 * Returns number of mismatches (0 = good).
 * -------------------------------------------------------- */
static uint16_t verify_batch(uint32_t base_addr, const uint32_t *pgm_data,
                             uint16_t start, uint16_t count)
{
  uint16_t errors = 0;

  if (!swd_open())
    return 0xFFFF; /* open failed = max errors */

  for (uint16_t i = 0; i < count; i++)
  {
    uint32_t expected = pgm_read_dword(&pgm_data[start + i]);
    uint32_t actual = swd_read(base_addr + ((uint32_t)(start + i) * 4));
    if (actual != expected)
    {
      errors++;
      if (errors <= 2)
      {
        Serial.print(F("    @0x"));
        Serial.print(base_addr + ((uint32_t)(start + i) * 4), HEX);
        Serial.print(F(" exp=0x"));
        Serial.print(expected, HEX);
        Serial.print(F(" got=0x"));
        Serial.println(actual, HEX);
      }
    }
  }

  swd_close();
  return errors;
}

/* --------------------------------------------------------
 * High-level: write entire page in batches
 * -------------------------------------------------------- */
static bool write_page(uint32_t addr, const uint32_t *pgm_data,
                       uint16_t word_count)
{
  Serial.print(F("  Erase 0x"));
  Serial.print(addr, HEX);
  Serial.print(F("... "));
  if (!erase_page(addr))
  {
    Serial.println(F("FAIL"));
    return false;
  }
  Serial.println(F("OK"));

  Serial.print(F("  Writing "));
  Serial.print(word_count);
  Serial.print(F(" words"));
  Serial.flush();

  for (uint16_t i = 0; i < word_count; i += BATCH_SIZE)
  {
    uint16_t n = word_count - i;
    if (n > BATCH_SIZE)
      n = BATCH_SIZE;

    if (!write_batch(addr, pgm_data, i, n))
    {
      Serial.print(F(" FAIL@"));
      Serial.println(i);
      return false;
    }

    /* Progress: dot every batch (8 words) */
    Serial.print('.');
    if (((i / BATCH_SIZE) & 0x0F) == 0x0F)
    {
      Serial.print(i + n);
    }
    Serial.flush();
  }
  Serial.println(F(" done"));
  return true;
}

/* --------------------------------------------------------
 * High-level: verify entire page in batches
 * -------------------------------------------------------- */
static bool verify_page(uint32_t addr, const uint32_t *pgm_data,
                        uint16_t word_count)
{
  Serial.print(F("  Verifying"));
  Serial.flush();
  uint16_t total_errors = 0;

  for (uint16_t i = 0; i < word_count; i += BATCH_SIZE)
  {
    uint16_t n = word_count - i;
    if (n > BATCH_SIZE)
      n = BATCH_SIZE;

    uint16_t e = verify_batch(addr, pgm_data, i, n);
    if (e == 0xFFFF)
    {
      Serial.print(F(" OPEN_FAIL@"));
      Serial.println(i);
      return false;
    }
    total_errors += e;

    Serial.print('.');
    if (((i / BATCH_SIZE) & 0x0F) == 0x0F)
    {
      Serial.print(i + n);
    }
    Serial.flush();
  }

  if (total_errors == 0)
  {
    Serial.println(F(" OK (1024 words match)"));
    return true;
  }
  Serial.println();
  Serial.print(F("  FAILED: "));
  Serial.print(total_errors);
  Serial.println(F(" mismatches"));
  return false;
}

/* --------------------------------------------------------
 * Erase a page if not already erased (opens own session)
 * -------------------------------------------------------- */
static void erase_if_needed(uint32_t addr, const __FlashStringHelper *name)
{
  if (!swd_open())
  {
    Serial.print(name);
    Serial.println(F(" open fail"));
    return;
  }
  uint32_t val = swd_read(addr);
  swd_close();

  Serial.print(name);
  Serial.print(F(" [0x"));
  Serial.print(addr, HEX);
  Serial.print(F("] = 0x"));
  Serial.println(val, HEX);

  if (val != 0xFFFFFFFF && val != 0)
  {
    Serial.print(F("  Erasing... "));
    if (erase_page(addr))
    {
      Serial.println(F("OK"));
    }
    else
    {
      Serial.println(F("FAIL"));
    }
  }
  else
  {
    Serial.println(F("  (already erased)"));
  }
}

/* --------------------------------------------------------
 * Initial halt with vector-catch (run once at start)
 * -------------------------------------------------------- */
static bool initial_halt(void)
{
  if (!swd_open())
    return false;

  /* Enable vector-catch so CPU halts immediately after any reset */
  swd_write(DEMCR, 0x01000001); /* VC_CORERESET | TRCENA */
  delay(10);

  /* Halt now */
  swd_write(DHCSR, 0xA05F0003);
  delay(100);

  uint32_t dhcsr = swd_read(DHCSR);
  swd_close();

  if (dhcsr & (1UL << 17))
  {
    Serial.println(F("CPU halt: OK"));
    return true;
  }

  /* Not halted — force reset so vector catch fires */
  Serial.print(F("forcing reset... "));
  if (!swd_open())
    return false;
  swd_write(DEMCR, 0x01000001);
  delay(10);
  swd_write(0xE000ED0C, 0x05FA0004); /* AIRCR SYSRESETREQ */
  swd_close();

  delay(300);

  /* Reconnect — CPU should be caught at reset vector */
  if (!swd_open())
    return false;
  swd_write(DHCSR, 0xA05F0003);
  delay(50);
  dhcsr = swd_read(DHCSR);
  swd_close();

  bool halted = (dhcsr & (1UL << 17));
  Serial.println(halted ? F("CPU halt: OK") : F("CPU halt: FAILED"));
  return halted;
}

/* --------------------------------------------------------
 * Sanity check: read MBR vector to confirm SWD reads work
 * -------------------------------------------------------- */
static bool sanity_check(void)
{
  if (!swd_open())
    return false;
  uint32_t mbr = swd_read(0x00000000);
  swd_close();

  Serial.print(F("MBR [0x00000] = 0x"));
  Serial.println(mbr, HEX);

  /* Valid nRF52832 SP is in RAM: 0x20000000-0x20010000 */
  if (mbr >= 0x20000000 && mbr <= 0x20010000)
    return true;

  Serial.println(F("SWD reads broken (bad MBR SP), retrying..."));
  return false;
}

/* --------------------------------------------------------
 * Main recovery flow
 * -------------------------------------------------------- */
static void run_recovery(void)
{
  if (done)
    return;

  Serial.println(F("=== nRF52 Bootloader Repair ==="));
  Serial.println();

  /* --- Step 0: raw SWD diagnostic --- */
  Serial.println(F("Step 0: SWD diagnostic"));
  dap.begin(SWDCLK_PIN, SWDIO_PIN, SWDRST_PIN, error_cb);
  if (!dap.dap_connect())
  {
    Serial.println(F("  dap_connect FAILED"));
    return;
  }
  Serial.println(F("  dap_connect OK"));

  dap.dap_transfer_configure(8, 1000, 128);
  dap.dap_swd_configure(0);
  dap.dap_swj_clock(50);
  dap.dap_quiet = true;

  bool linked = dap.dap_reset_link();
  Serial.print(F("  reset_link: "));
  Serial.println(linked ? F("OK") : F("FAIL"));
  delay(20);

  /* Read IDCODE (DP reg 0x02 = DPIDR on read) */
  uint32_t idcode = dap.dap_read_reg(0x02);
  Serial.print(F("  IDCODE: 0x"));
  Serial.println(idcode, HEX);

  /* Try ABORT + power-up */
  dap.dap_write_reg(SWD_DP_W_ABORT, 0x1E);
  dap.dap_write_reg(SWD_DP_W_SELECT, 0x00000000);
  dap.dap_write_reg(SWD_DP_W_CTRL_STAT, POWERUP_REQ);
  delay(50);

  uint32_t stat = dap.dap_read_reg(SWD_DP_R_CTRL_STAT);
  Serial.print(F("  CTRL_STAT: 0x"));
  Serial.println(stat, HEX);
  Serial.print(F("  PWRUP ACK: "));
  Serial.println((stat & POWERUP_ACK) == POWERUP_ACK ? F("YES") : F("NO"));

  if ((stat & POWERUP_ACK) != POWERUP_ACK)
  {
    /* Wait and retry */
    for (int i = 0; i < 20; i++)
    {
      delay(50);
      stat = dap.dap_read_reg(SWD_DP_R_CTRL_STAT);
      if ((stat & POWERUP_ACK) == POWERUP_ACK)
      {
        Serial.print(F("  PWRUP ACK after "));
        Serial.print((i + 1) * 50);
        Serial.println(F("ms"));
        break;
      }
    }
    if ((stat & POWERUP_ACK) != POWERUP_ACK)
    {
      Serial.println(F("  Power-up FAILED — check wiring!"));
      Serial.println(F("  SWDIO=pin10, SWDCLK=pin9, GND=GND"));
      dap.dap_disconnect();
      return;
    }
  }

  /* If we got here, power-up worked! Continue with AP access */
  dap.dap_write_reg(SWD_AP_CSW, 0x23000052);
  delay(10);

  /* Set vector catch + halt */
  dap.dap_write_word(DEMCR, 0x01000001);
  delay(10);
  dap.dap_write_word(DHCSR, 0xA05F0003);
  delay(100);

  uint32_t dhcsr = dap.dap_read_word(DHCSR);
  Serial.print(F("  DHCSR: 0x"));
  Serial.println(dhcsr, HEX);

  bool halted = (dhcsr & (1UL << 17));
  if (!halted)
  {
    Serial.println(F("  Not halted, forcing reset..."));
    dap.dap_write_word(0xE000ED0C, 0x05FA0004);
    delay(300);
    dap.dap_reset_link();
    delay(20);
    dap.dap_write_reg(SWD_DP_W_ABORT, 0x1E);
    dap.dap_write_reg(SWD_DP_W_SELECT, 0x00000000);
    dap.dap_write_reg(SWD_DP_W_CTRL_STAT, POWERUP_REQ);
    delay(100);
    dap.dap_write_reg(SWD_AP_CSW, 0x23000052);
    dap.dap_write_word(DHCSR, 0xA05F0003);
    delay(100);
    dhcsr = dap.dap_read_word(DHCSR);
    halted = (dhcsr & (1UL << 17));
    Serial.print(F("  DHCSR after reset: 0x"));
    Serial.println(dhcsr, HEX);
  }

  Serial.print(F("  CPU halted: "));
  Serial.println(halted ? F("YES") : F("NO"));

  if (!halted)
  {
    dap.dap_disconnect();
    return;
  }

  /* Read MBR */
  uint32_t mbr = dap.dap_read_word(0x00000000);
  Serial.print(F("  MBR SP: 0x"));
  Serial.println(mbr, HEX);

  /* Read page 0x77000 first word */
  uint32_t pg = dap.dap_read_word(0x77000);
  Serial.print(F("  Page 0x77000[0]: 0x"));
  Serial.println(pg, HEX);

  dap.dap_disconnect();
  Serial.println(F("  --- Diagnostic complete ---"));
  Serial.println();

  /* If diagnostic passed, proceed with recovery */
  if (mbr < 0x20000000 || mbr > 0x20010000)
  {
    Serial.println(F("MBR read looks wrong, retrying..."));
    return;
  }

  /* --- Erase + Write page 0x77000 --- */
  Serial.println(F("--- Restoring page 0x77000 ---"));
  if (!write_page(BL_PAGE_ADDR, bl_page_data, BL_PAGE_WORDS))
  {
    Serial.println(F("WRITE FAILED"));
    return;
  }

  /* --- Verify --- */
  Serial.println(F("--- Verifying ---"));
  if (!verify_page(BL_PAGE_ADDR, bl_page_data, BL_PAGE_WORDS))
  {
    Serial.println(F("VERIFY FAILED"));
    return;
  }

  /* --- Cleanup: erase pages so bootloader enters DFU --- */
  Serial.println(F("--- Cleanup ---"));
  erase_if_needed(SETTINGS_PAGE, F("Settings"));
  erase_if_needed(MBR_PARAMS_PAGE, F("MBR params"));
  erase_if_needed(APP_START_PAGE, F("App start"));

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
}

void setup()
{
  Serial.begin(115200);
  delay(2000);
}

void loop()
{
  run_recovery();
  delay(done ? 1000 : 5000);
}
