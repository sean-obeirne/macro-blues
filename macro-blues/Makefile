SDK     = $(HOME)/applications/nrf5-sdk
SD_HEX  = $(SDK)/components/softdevice/s132/hex/s132_nrf52_7.0.1_softdevice.hex
PORT    = /dev/ttyUSB0
BAUD    = 115200

CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
LD      = arm-none-eabi-gcc

BUILD   = build
TARGET  = $(BUILD)/macro-blues

SRCS = startup.c main.c timer.c gpio.c led.c keyswitch.c

INCLUDES = \
	-I. \
	-I$(SDK)/components/softdevice/s132/headers \
	-I$(SDK)/components/softdevice/s132/headers/nrf52 \
	-I$(SDK)/components/toolchain/cmsis/include

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
	-T nrf52832.ld \
	-Wl,--gc-sections \
	-Wl,-Map=$(TARGET).map \
	--specs=nosys.specs \
	--specs=nano.specs

OBJS = $(patsubst %.c, $(BUILD)/%.o, $(SRCS))

all: $(TARGET).hex

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $@

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

# Flash SoftDevice first (only needed once)
# NOTE: double-tap reset first, wait for blue LED to breathe, then run this
flash-sd:
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

clean:
	rm -rf $(BUILD)
