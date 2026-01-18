CC = sh-elf-gcc
LD = sh-elf-ld
OBJCOPY = sh-elf-objcopy
AR = sh-elf-ar
MKISOFS = mkisofs

CFLAGS = -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I./include
ASFLAGS = -m2 -mb
LDFLAGS = -T saturn.ld

LIB_OBJS = src/crt0.o src/system.o src/dualcpu/slave.o src/dualcpu/sync.o src/math/fixed.o src/math/matrix.o src/math/vector.o src/cd/read.o src/dma/scu_dma.o src/dsp/dsp.o src/vdp1/init.o src/vdp2/init.o src/peripheral/controller.o
LIB_HDRS = $(wildcard include/saturn/*.h) include/config.h

.PHONY: all clean lib examples tools

all: lib examples tools

lib: lib/libsaturn.a

lib/libsaturn.a: $(LIB_OBJS)
	$(AR) rcs $@ $^

%.o: %.s
	$(CC) $(ASFLAGS) -c $< -o $@

%.o: %.c $(LIB_HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf lib/*.a src/*/*.o examples/*/*.o examples/*/0.BIN examples/*/game.iso

examples:
	$(MAKE) -C examples/hello_world

tools:
	chmod +x tools/obj2saturn/*.py
