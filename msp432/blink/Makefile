# Build and flash an MSP432P401R program with arm-none-eabi-gcc + OpenOCD.
# Usage: source ../../buckler-blink/paths.txt   (for the compiler)
#        make          build
#        make flash    program the LaunchPad over its USB (XDS110)

PROJECT = blink
VENDOR  = ../vendor

CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size

# Cortex-M4, no hardware floating point for now (TI's startup file assumes soft-float).
CPU     = -mcpu=cortex-m4 -mthumb -mfloat-abi=soft

CFLAGS  = $(CPU) -Os -g -Wall -ffunction-sections -fdata-sections \
          -D__MSP432P401R__ -I$(VENDOR)/CMSIS -I$(VENDOR)/TI
LDFLAGS = $(CPU) -T$(VENDOR)/msp432p401r.ld -nostartfiles \
          --specs=nano.specs --specs=nosys.specs \
          -Wl,--gc-sections -Wl,-Map=build/$(PROJECT).map

SRCS = main.c stubs.c $(VENDOR)/system_msp432p401r.c $(VENDOR)/startup_msp432p401r_gcc.S
OBJS = $(patsubst %,build/%.o,$(notdir $(SRCS)))

all: build/$(PROJECT).elf build/$(PROJECT).hex
	$(SIZE) build/$(PROJECT).elf

build:
	mkdir -p build

build/main.c.o: main.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/stubs.c.o: stubs.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/system_msp432p401r.c.o: $(VENDOR)/system_msp432p401r.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/startup_msp432p401r_gcc.S.o: $(VENDOR)/startup_msp432p401r_gcc.S | build
	$(CC) $(CFLAGS) -c $< -o $@

build/$(PROJECT).elf: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

build/$(PROJECT).hex: build/$(PROJECT).elf
	$(OBJCOPY) -O ihex $< $@

flash: build/$(PROJECT).elf
	openocd -f board/ti_msp432_launchpad.cfg \
	        -c "program build/$(PROJECT).elf verify reset exit"

clean:
	rm -rf build

.PHONY: all flash clean
