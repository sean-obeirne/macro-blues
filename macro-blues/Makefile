BOARD = adafruit:nrf52:feather52832  # Your board type
PORT = /dev/ttyUSB0                  # Where your board is connected
SKETCH = ../macro-blues                 # Your file name (led.c or led.ino)


all:
	arduino-cli compile -b $(BOARD) $(SKETCH)
	arduino-cli upload -b $(BOARD) -p $(PORT) --input-dir build/$(BOARD)

clean:
	rm -f build
