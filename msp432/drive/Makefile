# Build and flash a drive program for the MSP432P401R (arm-none-eabi-gcc + OpenOCD).
#
#   source ../../buckler-blink/paths.txt     (once per terminal: compiler on PATH)
#   make                      build the wheel test      (APP=wheeltest, the default)
#   make APP=square           build the square program
#   make flash                flash the wheel test over the LaunchPad's USB (XDS110)
#   make APP=square flash     flash the square program

APP    ?= wheeltest
VENDOR  = ../vendor
BUILD   = build/$(APP)

CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size

# Cortex-M4, no hardware floating point for now (TI's startup file assumes soft-float).
CPU     = -mcpu=cortex-m4 -mthumb -mfloat-abi=soft

CFLAGS  = $(CPU) -Os -g -Wall -Wextra -ffunction-sections -fdata-sections \
          -D__MSP432P401R__ -I$(VENDOR)/CMSIS -I$(VENDOR)/TI
LDFLAGS = $(CPU) -T$(VENDOR)/msp432p401r.ld -nostartfiles \
          --specs=nano.specs --specs=nosys.specs \
          -Wl,--gc-sections -Wl,-Map=$(BUILD)/$(APP).map -lm

SRCS_C  = $(APP).c board.c motor.c uart.c encoder.c odom.c stubs.c system_msp432p401r.c
SRCS_S  = startup_msp432p401r_gcc.S
OBJS    = $(addprefix $(BUILD)/,$(SRCS_C:.c=.o) $(SRCS_S:.S=.o))

vpath %.c $(VENDOR)
vpath %.S $(VENDOR)

all: $(BUILD)/$(APP).elf $(BUILD)/$(APP).hex
	$(SIZE) $(BUILD)/$(APP).elf

$(BUILD):
	mkdir -p $@

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/%.o: %.S | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/$(APP).elf: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(BUILD)/$(APP).hex: $(BUILD)/$(APP).elf
	$(OBJCOPY) -O ihex $< $@

flash: $(BUILD)/$(APP).elf
	openocd -f board/ti_msp432_launchpad.cfg \
	        -c "program $(BUILD)/$(APP).elf verify reset exit"

clean:
	rm -rf build

-include $(OBJS:.o=.d)

.PHONY: all flash clean
