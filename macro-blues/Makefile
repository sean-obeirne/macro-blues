SDK     = $(HOME)/applications/nrf5-sdk
SD_HEX  = $(SDK)/components/softdevice/s132/hex/s132_nrf52_7.0.1_softdevice.hex
PORT    = /dev/ttyUSB0
BAUD    = 115200

CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
LD      = arm-none-eabi-gcc

BUILD   = build
TARGET  = $(BUILD)/macro-blues

SRCS = hw/startup.c main.c drivers/timer.c drivers/gpio.c output/led.c \
       drivers/i2c.c drivers/ssd1306.c \
       input/keyswitch.c input/debounce.c input/encoder.c drivers/gpiote.c \
       ble/ble_stack.c ble/hid_service.c ble/bond.c \
       input/keymap.c ble/battery.c \
       output/display.c output/glyphs.c

INCLUDES = \
	-I. \
	-Ihw \
	-Ible \
	-Iinput \
	-Idrivers \
	-Ioutput \
	-I$(SDK)/components/softdevice/s132/headers \
	-I$(SDK)/components/softdevice/s132/headers/nrf52 \
	-I$(SDK)/components/toolchain/cmsis/include \
	-I$(SDK)/modules/nrfx/mdk

CFLAGS = \
	-mcpu=cortex-m4 \
	-mthumb \
	-mfpu=fpv4-sp-d16 \
	-mfloat-abi=hard \
	-std=c11 \
	-Wall \
	-O2 \
	-ffunction-sections \
	-fdata-sections \
	-DNRF52832_XXAA \
	-DS132 \
	-DSWI_DISABLE0 \
	$(INCLUDES)

LDFLAGS = \
	-mcpu=cortex-m4 \
	-mthumb \
	-mfpu=fpv4-sp-d16 \
	-mfloat-abi=hard \
	-T hw/nrf52832.ld \
	-Wl,--gc-sections \
	-Wl,-Map=$(TARGET).map \
	--specs=nosys.specs \
	--specs=nano.specs

OBJS = $(patsubst %.c, $(BUILD)/%.o, $(SRCS))
DEPS = $(OBJS:.o=.d)

all: $(TARGET).hex

-include $(DEPS)

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(TARGET).elf: $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $@

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

# Flash SoftDevice (only needed once).
# If --touch 1200 times out, enter DFU manually:
#   Hold DFU button, tap RESET, release DFU.  LED pulses yellow.
#   Then run:  make flash-sd-manual
flash-sd:
	adafruit-nrfutil dfu genpkg --dev-type 0x0052 --sd-req 0x00 \
		--softdevice $(SD_HEX) $(BUILD)/sd_package.zip
	adafruit-nrfutil dfu serial --package $(BUILD)/sd_package.zip \
		-p $(PORT) -b $(BAUD) --singlebank --touch 1200

# Manual DFU: board must already be in DFU mode (yellow LED pulsing)
flash-sd-manual:
	adafruit-nrfutil dfu genpkg --dev-type 0x0052 --sd-req 0x00 \
		--softdevice $(SD_HEX) $(BUILD)/sd_package.zip
	adafruit-nrfutil dfu serial --package $(BUILD)/sd_package.zip \
		-p $(PORT) -b $(BAUD) --singlebank

# Flash your application
flash: $(TARGET).hex
	adafruit-nrfutil dfu genpkg --dev-type 0x0052 --sd-req 0x00B7 \
		--application $(TARGET).hex $(BUILD)/app_package.zip
	adafruit-nrfutil dfu serial --package $(BUILD)/app_package.zip \
		-p $(PORT) -b $(BAUD) --singlebank --touch 1200

test-glyphs:
	gcc -Itest/stubs -Ioutput -I. test/test_display.c output/display.c output/glyphs.c -o $(BUILD)/test_display
	./$(BUILD)/test_display

clean:
	rm -rf $(BUILD)
