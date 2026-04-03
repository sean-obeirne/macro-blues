#!/bin/bash
# Upload swd_recovery sketch to SparkFun Pro Micro
# Usage: Double-tap reset on the Pro Micro, then immediately run this script

SKETCH_DIR="/home/sean/code/active/macro-blues/references/swd_recovery"
FQBN="SparkFun:avr:promicro:cpu=16MHzatmega32U4"
URLS="https://raw.githubusercontent.com/sparkfun/Arduino_Boards/main/IDE_Board_Manager/package_sparkfun_index.json"

echo ">>> Compiling..."
arduino-cli compile --fqbn "$FQBN" --additional-urls "$URLS" "$SKETCH_DIR" || exit 1

HEX=$(find ~/.cache/arduino/sketches -name "swd_recovery.ino.hex" -newer "$SKETCH_DIR/swd_recovery.ino" 2>/dev/null | head -1)
if [ -z "$HEX" ]; then
  HEX=$(find ~/.cache/arduino/sketches -name "swd_recovery.ino.hex" 2>/dev/null | head -1)
fi
echo ">>> Using hex: $HEX"

echo ">>> Double-tap reset NOW, then wait..."

# Wait for port to disappear (if present)
while [ -e /dev/ttyACM0 ]; do sleep 0.02; done 2>/dev/null
echo ">>> Port gone, waiting for bootloader..."

# Wait for port to reappear (bootloader)
while [ ! -e /dev/ttyACM0 ]; do sleep 0.02; done
echo ">>> Port back! Uploading immediately..."

sleep 0.1
/home/sean/.arduino15/packages/arduino/tools/avrdude/8.0.0-arduino1/bin/avrdude \
  -C/home/sean/.arduino15/packages/arduino/tools/avrdude/8.0.0-arduino1/etc/avrdude.conf \
  -v -V -patmega32u4 -cavr109 -P/dev/ttyACM0 -b57600 -D \
  -Uflash:w:$HEX:i
