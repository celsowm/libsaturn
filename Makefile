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
	$(MAKE) -C examples/01_helloworld
	$(MAKE) -C examples/02_input
	$(MAKE) -C examples/03_sprites
	$(MAKE) -C examples/04_fixedpoint_math
	$(MAKE) -C examples/05_dualcpu_sync
	$(MAKE) -C examples/06_matrix_transform
	$(MAKE) -C examples/07_polygon_render
	$(MAKE) -C examples/08_texture_mapping
	$(MAKE) -C examples/09_vdp2_background
	$(MAKE) -C examples/10_3d_cube
	$(MAKE) -C examples/11_sound
	$(MAKE) -C examples/12_physics
	$(MAKE) -C examples/13_cd_load
	$(MAKE) -C examples/14_scu_dma

tools:
	chmod +x tools/obj2saturn/*.py
