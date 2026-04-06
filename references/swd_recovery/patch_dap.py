#!/usr/bin/env python3
"""Patch Adafruit_DAP.cpp to add dap_quiet support and proper error returns."""

filepath = "/home/sean/Arduino/libraries/Adafruit_DAP_library/Adafruit_DAP.cpp"

with open(filepath, "r") as f:
    content = f.read()

# Patch 1: dap_read_reg - wrap error prints in if(!dap_quiet), add return 0
old_read = """  if (1 != buf[0] || DAP_TRANSFER_OK != buf[1]) {
    // error_exit("invalid response while reading the register 0x%02x (count =
    // %d, value = %d)",
    //    reg, buf[0], buf[1]);
    Serial.print("invalid response reading reg ");
    Serial.print(reg, HEX);

    Serial.print(" (count = ");
    Serial.print(buf[0]);
    Serial.print(", value = ");
    Serial.print(buf[1]);
    perror_exit(")");
  }"""

new_read = """  if (1 != buf[0] || DAP_TRANSFER_OK != buf[1]) {
    if (!dap_quiet) {
      Serial.print("invalid response reading reg ");
      Serial.print(reg, HEX);
      Serial.print(" (count = ");
      Serial.print(buf[0]);
      Serial.print(", value = ");
      Serial.print(buf[1]);
      perror_exit(")");
    }
    return 0;
  }"""

if old_read in content:
    content = content.replace(old_read, new_read)
    print("Patched dap_read_reg")
else:
    print("WARNING: Could not find dap_read_reg error block!")

# Patch 2: dap_write_reg - wrap error prints in if(!dap_quiet), return false
old_write = """  if (1 != buf[0] || DAP_TRANSFER_OK != buf[1]) {
    Serial.print("invalid response writing to reg ");
    Serial.print(reg, HEX);

    Serial.print(" (count = ");
    Serial.print(buf[0]);
    Serial.print(", value = ");
    Serial.print(buf[1]);
    perror_exit(")");
  }
  return true;"""

new_write = """  if (1 != buf[0] || DAP_TRANSFER_OK != buf[1]) {
    if (!dap_quiet) {
      Serial.print("invalid response writing to reg ");
      Serial.print(reg, HEX);
      Serial.print(" (count = ");
      Serial.print(buf[0]);
      Serial.print(", value = ");
      Serial.print(buf[1]);
      perror_exit(")");
    }
    return false;
  }
  return true;"""

if old_write in content:
    content = content.replace(old_write, new_write)
    print("Patched dap_write_reg")
else:
    print("WARNING: Could not find dap_write_reg error block!")

with open(filepath, "w") as f:
    f.write(content)

print("Done!")
