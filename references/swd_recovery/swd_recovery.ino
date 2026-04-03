/*
 * SWD Recovery: Erase the nRF52832 bootloader settings page (0x7F000)
 *
 * Our bond_save() accidentally wrote to 0x7F000, corrupting the Adafruit
 * bootloader settings.  This sketch uses a Pro Micro (ATmega32U4) as an
 * SWD programmer to erase that page.  Once erased (all 0xFF), the
 * bootloader will see no valid app and enter serial DFU mode.
 *
 * Wiring (Pro Micro → Feather nRF52832):
 *   Pin 10  →  SWDIO   (via 1K resistor if Pro Micro is 5V)
 *   Pin 9   →  SWDCLK  (via 1K resistor if Pro Micro is 5V)
 *   GND     →  GND
 *
 * The SWD pads are on the BACK of the Feather nRF52832 board,
 * near the RF module.  They're tiny labeled test points.
 *
 * Open Serial Monitor at 115200 after uploading to see progress.
 */

#include "Adafruit_DAP.h"

#define SWDIO_PIN  10   /* Your actual wiring */
#define SWDCLK_PIN 9
#define SWDRST_PIN 8    /* Optional - connect to Feather RST if desired */

Adafruit_DAP_nRF5x dap;
bool done = false;

void error_cb(const char *text) {
  Serial.print("ERROR: ");
  Serial.println(text);
  while (1) delay(1000);
}

/* nRF52832 NVMC (Non-Volatile Memory Controller) registers */
#define NVMC_READY     0x4001E400
#define NVMC_CONFIG    0x4001E504
#define NVMC_ERASEPAGE 0x4001E508

/* The page our bond_save() corrupted */
#define SETTINGS_PAGE  0x0007F000

/* Cortex-M4 debug register to halt the CPU */
#define DHCSR          0xE000EDF0

static void run_recovery(void) {
  if (done) {
    return;
  }

  Serial.println("=== Pin Diagnostics ===");
  Serial.flush();
  
  // Test SWDIO
  pinMode(SWDIO_PIN, INPUT_PULLUP);
  delay(10);
  int swdio_pullup = digitalRead(SWDIO_PIN);
  pinMode(SWDIO_PIN, INPUT);
  delay(10);
  int swdio_float = digitalRead(SWDIO_PIN);
  
  Serial.print("SWDIO (pin ");
  Serial.print(SWDIO_PIN);
  Serial.print("): pullup=");
  Serial.print(swdio_pullup);
  Serial.print(", float=");
  Serial.println(swdio_float);
  
  // Test SWDCLK
  pinMode(SWDCLK_PIN, INPUT_PULLUP);
  delay(10);
  int swdclk_pullup = digitalRead(SWDCLK_PIN);
  pinMode(SWDCLK_PIN, INPUT);
  delay(10);
  int swdclk_float = digitalRead(SWDCLK_PIN);
  
  Serial.print("SWDCLK (pin ");
  Serial.print(SWDCLK_PIN);
  Serial.print("): pullup=");
  Serial.print(swdclk_pullup);
  Serial.print(", float=");
  Serial.println(swdclk_float);
  Serial.flush();
  
  if (swdio_float == 0 && swdio_pullup == 1) {
    Serial.println("WARNING: SWDIO not connected to target");
  }
  if (swdclk_float == 0 && swdclk_pullup == 1) {
    Serial.println("WARNING: SWDCLK not connected to target");
  }
  if (swdio_float == 1) {
    Serial.println("SWDIO driven HIGH - target connected!");
  }
  Serial.println();

  Serial.println("=== nRF52 Bootloader Settings Recovery ===");
  Serial.println();

  Serial.println("DEBUG: Calling dap.begin()...");
  Serial.flush();
  dap.begin(SWDCLK_PIN, SWDIO_PIN, SWDRST_PIN, error_cb);
  Serial.println("DEBUG: dap.begin() done");
  Serial.flush();

  /* --- Connect via SWD --- */
  Serial.print("Connecting via SWD... ");
  Serial.flush();
  dap.dap_disconnect();
  Serial.println("DEBUG: disconnect done, waiting 100ms...");
  Serial.flush();
  delay(100);

  Serial.println("DEBUG: calling dap_connect()...");
  Serial.flush();
  if (!dap.dap_connect()) {
    Serial.println("FAILED!");
    Serial.println("Check wiring: SWDIO=pin10, SWDCLK=pin9, GND=GND");
    Serial.println("Retrying in 2 seconds...");
    Serial.println();
    dap.dap_disconnect();
    delay(2000);
    return;
  }
  dap.dap_transfer_configure(0, 128, 128);
  dap.dap_swd_configure(0);
  Serial.println("OK");

  /* --- Halt the CPU (it may be crash-looping) --- */
  Serial.print("Halting CPU... ");
  dap.dap_write_word(DHCSR, 0xA05F0003);
  delay(10);
  Serial.println("OK");

  /* --- Read current contents of settings page --- */
  uint32_t val = dap.dap_read_word(SETTINGS_PAGE);
  Serial.print("Settings page [0x7F000] = 0x");
  Serial.println(val, HEX);

  /* --- Erase the page --- */
  Serial.print("Erasing page 0x7F000... ");

  /* Enable erase in NVMC */
  dap.dap_write_word(NVMC_CONFIG, 2);
  delay(10);
  while (!(dap.dap_read_word(NVMC_READY) & 1)) delay(1);

  /* Erase the page */
  dap.dap_write_word(NVMC_ERASEPAGE, SETTINGS_PAGE);
  delay(100);
  while (!(dap.dap_read_word(NVMC_READY) & 1)) delay(1);

  /* Back to read-only */
  dap.dap_write_word(NVMC_CONFIG, 0);
  delay(10);
  Serial.println("OK");

  /* --- Verify --- */
  val = dap.dap_read_word(SETTINGS_PAGE);
  Serial.print("After erase: 0x");
  Serial.println(val, HEX);

  if (val == 0xFFFFFFFF) {
    Serial.println();
    Serial.println("SUCCESS! Settings page is clean.");
    Serial.println();
    Serial.println("Next steps:");
    Serial.println("  1. Disconnect SWD wires from the Feather");
    Serial.println("  2. Press RESET on the Feather");
    Serial.println("  3. Bootloader enters serial DFU mode");
    Serial.println("  4. Run: make flash");
    done = true;
  } else {
    Serial.println("WARNING: erase may have failed, retrying...");
  }

  dap.dap_disconnect();
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(2000);
}

void loop() {
  run_recovery();
  delay(done ? 1000 : 2000);
}
