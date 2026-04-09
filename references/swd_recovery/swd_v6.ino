/*
 * SWD Recovery Bridge v6 — per-word noInterrupts.
 *
 * Root cause of v4/v5 failures: noInterrupts() for an entire
 * 16-word batch holds interrupts off for ~161ms.  USB full-speed
 * resets the device after 3 missed SOF frames (3ms).  After many
 * batches, USB state corrupts serial data → wrong addresses/data.
 *
 * Fix: noInterrupts() per INDIVIDUAL word write.  Each window is
 * ~1.4ms (one SWD write + 400us NVMC delay).  Under the 3ms USB
 * threshold.  Interrupts re-enabled between words so USB stays healthy.
 *
 * Protocol (same as v5):
 *   'C' -> connect + halt, 'K'
 *   'D' -> disconnect, 'K'
 *   'E' -> chip erase, 'K'
 *   'W' addr(4) cnt(1) data(cnt*4) -> write up to 16 words, 'K'
 *   'V' addr(4) cnt(1) -> read cnt words (1-8), 'K' + data
 *   'N' -> NVMC write enable, 'K'
 *   'O' -> NVMC read-only, 'K'
 *   'X' -> reset target, 'K'
 *   'P' -> ping, 'K'
 *   Error: 'F' + msg + '\n'
 */

#include "Adafruit_DAP.h"

#define SWDIO_PIN  10
#define SWDCLK_PIN 9
#define SWDRST_PIN 8

#define NVMC_READY    0x4001E400
#define NVMC_CONFIG   0x4001E504
#define NVMC_ERASEALL 0x4001E50C
#define DHCSR         0xE000EDF0
#define AIRCR         0xE000ED0C

#define SWD_DP_W_ABORT     0x00
#define SWD_DP_W_CTRL_STAT 0x04
#define SWD_DP_R_CTRL_STAT 0x06
#define SWD_DP_W_SELECT    0x08
#define SWD_AP_CSW         0x01

#define POWERUP_REQ (0x50000F00UL)
#define POWERUP_ACK (0xA0000000UL)

Adafruit_DAP_nRF5x dap;
bool connected = false;

void error_cb(const char *t) { (void)t; }

static void reply_ok(void) { Serial.write('K'); }
static void reply_fail(const char *msg)
{
  Serial.write('F');
  Serial.print(msg);
  Serial.write('\n');
}

static uint32_t read_u32(void)
{
  uint8_t buf[4];
  Serial.readBytes(buf, 4);
  return (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
         ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
}

static void write_u32(uint32_t v)
{
  uint8_t buf[4] = {
    (uint8_t)(v), (uint8_t)(v >> 8),
    (uint8_t)(v >> 16), (uint8_t)(v >> 24)
  };
  Serial.write(buf, 4);
}

static bool swd_connect(void)
{
  dap.begin(SWDCLK_PIN, SWDIO_PIN, SWDRST_PIN, error_cb);
  if (!dap.dap_connect()) return false;

  dap.dap_transfer_configure(8, 1000, 128);
  dap.dap_swd_configure(0);
  dap.dap_swj_clock(50);
  dap.dap_quiet = true;

  for (int a = 0; a < 5; a++) {
    dap.dap_reset_link();
    delay(20);

    dap.dap_write_reg(SWD_DP_W_ABORT, 0x1E);
    dap.dap_write_reg(SWD_DP_W_SELECT, 0x00000000);
    dap.dap_write_reg(SWD_DP_W_CTRL_STAT, POWERUP_REQ);

    for (int i = 0; i < 50; i++) {
      uint32_t s = dap.dap_read_reg(SWD_DP_R_CTRL_STAT);
      if ((s & POWERUP_ACK) == POWERUP_ACK) {
        dap.dap_write_reg(SWD_AP_CSW, 0x23000052);
        dap.dap_write_word(DHCSR, 0xA05F0003);
        return true;
      }
      delay(10);
    }
    delay(200);
  }

  dap.dap_disconnect();
  return false;
}

static void cmd_connect(void)
{
  if (connected) { reply_ok(); return; }
  if (!swd_connect()) { reply_fail("SWD"); return; }
  connected = true;
  reply_ok();
}

static void cmd_disconnect(void)
{
  if (connected) { dap.dap_disconnect(); connected = false; }
  reply_ok();
}

static void cmd_erase(void)
{
  if (!connected) { reply_fail("NC"); return; }

  dap.dap_write_word(NVMC_CONFIG, 2);
  dap.dap_write_word(NVMC_ERASEALL, 1);
  delay(600);

  for (int i = 0; i < 100; i++) {
    if (dap.dap_read_word(NVMC_READY) & 1) {
      dap.dap_write_word(NVMC_CONFIG, 0);
      reply_ok();
      return;
    }
    delay(10);
  }
  dap.dap_write_word(NVMC_CONFIG, 0);
  reply_fail("ET");
}

static void cmd_nvmc_we(void)
{
  if (!connected) { reply_fail("NC"); return; }
  dap.dap_write_word(NVMC_CONFIG, 1);
  reply_ok();
}

static void cmd_nvmc_ro(void)
{
  if (!connected) { reply_fail("NC"); return; }
  dap.dap_write_word(NVMC_CONFIG, 0);
  reply_ok();
}

/*
 * Write up to 16 words to flash.
 *
 * KEY CHANGE from v5: noInterrupts() per INDIVIDUAL word, not
 * per batch.  Each window is ~1.4ms (SWD write + 400us NVMC).
 * USB misses at most 1 SOF frame per word — no bus reset.
 */
static void cmd_write(void)
{
  uint32_t addr = read_u32();
  uint8_t cnt;
  Serial.readBytes(&cnt, 1);
  if (cnt > 16) cnt = 16;

  uint32_t words[16];
  for (uint8_t i = 0; i < cnt; i++) words[i] = read_u32();

  if (!connected) { reply_fail("NC"); return; }

  for (uint8_t i = 0; i < cnt; i++) {
    noInterrupts();
    dap.dap_write_word(addr + (uint32_t)i * 4, words[i]);
    delayMicroseconds(400);
    interrupts();
  }

  reply_ok();
}

/*
 * Read up to 8 words.  Per-word noInterrupts for clean reads.
 */
static void cmd_read(void)
{
  uint32_t addr = read_u32();
  uint8_t cnt;
  Serial.readBytes(&cnt, 1);
  if (cnt > 8) cnt = 8;

  if (!connected) { reply_fail("NC"); return; }

  uint32_t vals[8];
  for (uint8_t i = 0; i < cnt; i++) {
    noInterrupts();
    vals[i] = dap.dap_read_word(addr + (uint32_t)i * 4);
    interrupts();
  }

  reply_ok();
  for (uint8_t i = 0; i < cnt; i++) write_u32(vals[i]);
}

static void cmd_reset(void)
{
  if (connected) {
    dap.dap_write_word(DHCSR, 0xA05F0000);
    delay(10);
    dap.dap_write_word(AIRCR, 0x05FA0004);
    delay(10);
    dap.dap_disconnect();
    connected = false;
  }
  reply_ok();
}

void setup()
{
  Serial.begin(115200);
  Serial.setTimeout(5000);
  while (!Serial) ;
  delay(200);
  Serial.println(F("SWD_BRIDGE_READY"));
}

void loop()
{
  if (!Serial.available()) return;
  char cmd = Serial.read();
  switch (cmd) {
    case 'C': cmd_connect(); break;
    case 'D': cmd_disconnect(); break;
    case 'E': cmd_erase(); break;
    case 'W': cmd_write(); break;
    case 'V': cmd_read(); break;
    case 'N': cmd_nvmc_we(); break;
    case 'O': cmd_nvmc_ro(); break;
    case 'X': cmd_reset(); break;
    case 'P': reply_ok(); break;
    default: break;
  }
}
