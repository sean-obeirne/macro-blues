/*
 * SWD Diagnostic for Adafruit Feather nRF52832
 *
 * Reads critical memory regions via SWD to diagnose boot failure.
 * Does NOT modify anything — read-only.
 *
 * Wiring (Pro Micro -> Feather nRF52832):
 *   Pin 10 -> SWDIO
 *   Pin 9  -> SWDCLK
 *   GND    -> GND
 *
 * The SWD pads are on the bottom of the Feather PCB.
 */

#include "Adafruit_DAP.h"

#define SWDIO_PIN 10
#define SWDCLK_PIN 9
#define SWDRST_PIN 8

/* Cortex-M4 debug */
#define DHCSR 0xE000EDF0
#define DEMCR 0xE000EDFC

/* SWD DP registers */
#define SWD_DP_W_ABORT 0x00
#define SWD_DP_W_CTRL_STAT 0x04
#define SWD_DP_R_CTRL_STAT 0x06
#define SWD_DP_W_SELECT 0x08
#define SWD_AP_CSW 0x01

#define CDBGPWRUPREQ (1UL << 28)
#define CDBGPWRUPACK (1UL << 29)
#define CSYSPWRUPREQ (1UL << 30)
#define CSYSPWRUPACK (1UL << 31)
#define POWERUP_REQ (CDBGPWRUPREQ | CSYSPWRUPREQ | 0x00000F00)
#define POWERUP_ACK (CDBGPWRUPACK | CSYSPWRUPACK)

Adafruit_DAP_nRF5x dap;
bool done = false;

void error_cb(const char *text) { (void)text; }

static bool swd_open(void)
{
  dap.begin(SWDCLK_PIN, SWDIO_PIN, SWDRST_PIN, error_cb);
  if (!dap.dap_connect())
  {
    Serial.println(F("  dap_connect FAILED"));
    return false;
  }

  dap.dap_transfer_configure(8, 1000, 128);
  dap.dap_swd_configure(0);
  dap.dap_swj_clock(50);
  dap.dap_quiet = true;

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

  Serial.println(F("  Power-up FAILED — check wiring!"));
  dap.dap_disconnect();
  return false;
}

static void swd_close(void)
{
  dap.dap_disconnect();
}

static void read_and_print(const char *label, uint32_t addr)
{
  if (!swd_open())
  {
    Serial.print(label);
    Serial.println(F(": OPEN FAIL"));
    return;
  }
  uint32_t val = dap.dap_read_word(addr);
  swd_close();

  Serial.print(label);
  Serial.print(F(" [0x"));
  Serial.print(addr, HEX);
  Serial.print(F("] = 0x"));
  Serial.println(val, HEX);
}

static void run_diagnostic(void)
{
  if (done)
    return;

  Serial.println();
  Serial.println(F("=== nRF52832 Boot Diagnostic ==="));
  Serial.println();

  /* MBR vector table (address 0x0) */
  Serial.println(F("--- MBR (0x00000) ---"));
  read_and_print("  SP (stack ptr)", 0x00000000);
  read_and_print("  Reset vector ", 0x00000004);

  /* SoftDevice info */
  Serial.println(F("--- SoftDevice Info ---"));
  read_and_print("  SD magic     ", 0x00001000);
  read_and_print("  SD size/info ", 0x0000300C);

  /* MBR forward address (where MBR sends interrupts) */
  Serial.println(F("--- MBR params ---"));
  read_and_print("  MBR fwd addr ", 0x20000000);
  read_and_print("  SD vector fwd", 0x20000004);

  /* App vector table at 0x26000 */
  Serial.println(F("--- App (0x26000) ---"));
  read_and_print("  App SP       ", 0x00026000);
  read_and_print("  App Reset    ", 0x00026004);
  read_and_print("  App word 2   ", 0x00026008);
  read_and_print("  App word 3   ", 0x0002600C);

  /* UICR */
  Serial.println(F("--- UICR ---"));
  read_and_print("  NRFFW[0] (BL)", 0x10001014);
  read_and_print("  NRFFW[1] (MBR param)", 0x10001018);

  /* Bootloader settings page (0x7F000) */
  Serial.println(F("--- BL Settings (0x7F000) ---"));
  read_and_print("  settings[0]  ", 0x0007F000);
  read_and_print("  settings[1]  ", 0x0007F004);
  read_and_print("  settings[2]  ", 0x0007F008);
  read_and_print("  settings[3]  ", 0x0007F00C);
  read_and_print("  settings[4]  ", 0x0007F010);

  /* MBR params page (0x7E000) */
  Serial.println(F("--- MBR Params (0x7E000) ---"));
  read_and_print("  mbr_params[0]", 0x0007E000);
  read_and_print("  mbr_params[1]", 0x0007E004);

  /* Bootloader start */
  Serial.println(F("--- Bootloader (0x74000) ---"));
  read_and_print("  BL SP        ", 0x00074000);
  read_and_print("  BL Reset     ", 0x00074004);

  /* NVMC READYNEXT — is flash accessible? */
  Serial.println(F("--- Flash controller ---"));
  read_and_print("  NVMC READY   ", 0x4001E400);

  Serial.println();
  Serial.println(F("=== Diagnostic complete ==="));
  Serial.println(F("Copy the output above and share it."));
  done = true;
}

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    ; /* wait for USB serial to connect */
  delay(500);
  Serial.println(F("=== SWD Diag Ready ==="));
  Serial.println(F("Attempting SWD connection..."));
  Serial.flush();
}

void loop()
{
  Serial.println(F("--- Starting diagnostic pass ---"));
  Serial.flush();
  run_diagnostic();
  done = false; /* repeat every 10s so user can reconnect */
  Serial.println(F("--- Waiting 10s before next pass ---"));
  Serial.flush();
  delay(10000);
}
