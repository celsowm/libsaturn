CC = sh-elf-gcc
LD = sh-elf-ld
OBJCOPY = sh-elf-objcopy
MKISOFS = mkisofs

CFLAGS = -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I../../include
ASFLAGS = -m2 -mb
LDFLAGS = -T ../../saturn.ld

OBJS = ../../src/crt0.o main.o
LIB = ../../lib/libsaturn.a

.PHONY: all clean

all: 0.BIN

0.BIN: game.elf
	$(OBJCOPY) -O binary $< $@

game.elf: $(OBJS) $(LIB)
	$(LD) $(LDFLAGS) -o $@ $(OBJS) $(LIB)

clean:
	rm -f *.o *.elf 0.BIN
