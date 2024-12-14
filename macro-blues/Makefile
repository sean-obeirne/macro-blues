BOARD = adafruit:nrf52:feather52832
PORT = /dev/ttyUSB0
SKETCH = ../macro-blues


all:
	arduino-cli compile -b $(BOARD) --output-dir build/$(BOARD) --verbose --build-properties "compiler.c.elf.extra_flags=-u __wrap_malloc -u __wrap_free"
	arduino-cli upload -b $(BOARD) -p $(PORT) --input-dir build/$(BOARD)

clean:
	rm -rf build
