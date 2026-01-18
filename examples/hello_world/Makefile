ROOT := ../..
TOOLCHAIN_BIN ?= $(ROOT)/toolchains/sh-elf-gcc/bin
TOOLCHAIN_LIB ?= $(ROOT)/toolchains/sh-elf-gcc/lib/gcc/sh-elf/9.3.0

CC := $(TOOLCHAIN_BIN)/sh-elf-gcc
OBJCOPY := $(TOOLCHAIN_BIN)/sh-elf-objcopy

CFLAGS = -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I$(ROOT)/include -B$(TOOLCHAIN_BIN)
LDFLAGS = -T $(ROOT)/saturn.ld -L$(ROOT)/lib -L$(TOOLCHAIN_LIB) -lsaturn

TARGET = hello_world
OBJS = main.o

.PHONY: all clean

all: $(TARGET).bin

$(TARGET).elf: $(OBJS) $(ROOT)/lib/libsaturn.a
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).bin
