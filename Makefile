# set MCU attiny85 or attiny13. It presets F_CPU and FUSE as well.
MCU   := attiny13

ifeq ($(MCU),attiny85)
F_CPU := 8000000
FUSE  := -U lfuse:w:0xe2:m -U hfuse:w:0xdd:m -U efuse:w:0xfe:m
else
ifeq ($(MCU),attiny13)
F_CPU := 9600000
FUSE  := -U lfuse:w:0x7a:m -U hfuse:w:0xff:m
else
F_CPU := _must_set_F_CPU_
FUSE  := _must_set_FUSE_
endif
endif

TARGET := stand
PROG   := buspirate
PORT   := /dev/ttyUSB0
TOOLS  := /opt/arduino-1.8.9/hardware/tools
OPTIM  := -Os

CFLAGS = -std=gnu99 $(OPTIM) -funsigned-char -funsigned-bitfields \
  -fpack-struct -fshort-enums -Wall -DF_CPU=$(F_CPU) -mmcu=$(MCU) \
  -Wstrict-prototypes

AVRDUDE := $(TOOLS)/avr/bin/avrdude -C$(TOOLS)/avr/etc/avrdude.conf -c $(PROG) -P $(PORT) -p $(MCU) -V

CC      = $(TOOLS)/avr/bin/avr-gcc
CXX     = $(TOOLS)/avr/bin/avr-c++
OBJCOPY = $(TOOLS)/avr/bin/avr-objcopy
OBJDUMP = $(TOOLS)/avr/bin/avr-objdump
SIZE    = $(TOOLS)/avr/bin/avr-size

all: $(TARGET).hex

check:
	$(AVRDUDE)

fuse:
	$(AVRDUDE) $(FUSE)

upload:
	$(AVRDUDE) -s -U flash:w:$(TARGET).hex

%.hex: %.elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@
	$(SIZE) $@

%.elf: %.o
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	-rm -f *.hex *.elf *.o
