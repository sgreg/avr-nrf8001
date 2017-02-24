MCU = atmega328p
F_CPU = 8000000

PROGRAM=avr_nrf8001_example
# Default target.
all: $(PROGRAM).hex

OBJS = main.o nrf.o spi.o uart.o

# Fuses
# low: CLK 8MHz internal oscillator, no clock divider, 6ck/14ck + 65ms
burn-fuse:
	$(AVRDUDE) $(AVRDUDE_FLAGS) -U lfuse:w:0xe2:m -U hfuse:w:0xdf:m -U efuse:w:0xff:m

# this programs the dependant hex file using our default avrdude flags
program:
	$(AVRDUDE) $(AVRDUDE_FLAGS) -U flash:w:$(PROGRAM).hex


CC = avr-gcc
OBJCOPY = avr-objcopy
SIZE = avr-size

CFLAGS = -g -Os -std=gnu99 -I. \
-funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums \
-Wall -Wextra -Wstrict-prototypes \
-DF_CPU=$(F_CPU) -mmcu=$(MCU) 

ASFLAGS = -Wa,-adhlms=$(<:.c=.lst),-gstabs 
ASFLAGS_ASM = -Wa,-gstabs 
LDFLAGS = -Wl,-Map=$(<:.o=.map),--cref


# ---------------------------------------------------------------------------
# Programming support using avrdude.
AVRDUDE = avrdude

# avrdude usbasp
AVRDUDE_FLAGS = -p $(MCU) -c usbasp

# ---------------------------------------------------------------------------

.PRECIOUS : %.elf %.o
$(PROGRAM).hex: $(PROGRAM).elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@
	@$(SIZE) $^

$(PROGRAM).elf: $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

main.o: CFLAGS += -DBUILD_TIMESTAMP="\"$(shell /bin/date +%y%m%d-%H%M%S)\""

.c.o:
	$(CC) -c $(CFLAGS) $(ASFLAGS) $< -o $@

.S.o:
	$(CC) -c $(CFLAGS) -x assembler-with-cpp $(ASFLAGS_ASM) $< -o $@

clean:
	rm -f $(OBJS)

distclean: clean
	rm -f *.elf
	rm -f *.hex
	rm -f *.lst
	rm -f *.map

# Listing of phony targets.
.PHONY : all clean distclean program
