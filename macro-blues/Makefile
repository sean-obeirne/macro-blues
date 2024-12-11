BOARD = adafruit:nrf52:feather52832
PORT = /dev/ttyUSB0
SKETCH = ../macro-blues


all:
	arduino-cli compile -b $(BOARD) $(SKETCH) --output-dir build/$(BOARD)
	arduino-cli upload -b $(BOARD) -p $(PORT) --input-dir build/$(BOARD)

clean:
	rm -f build
