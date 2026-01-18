#!/bin/bash
set -e

TOOLCHAIN_PATH=~/saturn-sdk/sh-elf-gcc
TOOLCHAIN_BIN="${TOOLCHAIN_PATH}/bin"

echo "Building libsaturn..."

# Clean
echo "Cleaning..."
rm -f lib/libsaturn.a
find . -name "*.o" -delete
find . -name "*.elf" -delete

# Build library
echo "Building library..."
cd "${TOOLCHAIN_BIN}"
"${TOOLCHAIN_BIN}/sh-elf-gcc" -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I/c/Users/celso/libsaturn/include -c /c/Users/celso/libsaturn/src/crt0.s -o /c/Users/celso/libsaturn/src/crt0.o
"${TOOLCHAIN_BIN}/sh-elf-gcc" -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I/c/Users/celso/libsaturn/include -c /c/Users/celso/libsaturn/src/system.c -o /c/Users/celso/libsaturn/src/system.o
"${TOOLCHAIN_BIN}/sh-elf-gcc" -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I/c/Users/celso/libsaturn/include -c /c/Users/celso/libsaturn/src/dualcpu/slave.c -o /c/Users/celso/libsaturn/src/dualcpu/slave.o
"${TOOLCHAIN_BIN}/sh-elf-gcc" -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I/c/Users/celso/libsaturn/include -c /c/Users/celso/libsaturn/src/dualcpu/sync.c -o /c/Users/celso/libsaturn/src/dualcpu/sync.o
"${TOOLCHAIN_BIN}/sh-elf-gcc" -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I/c/Users/celso/libsaturn/include -c /c/Users/celso/libsaturn/src/math/fixed.c -o /c/Users/celso/libsaturn/src/math/fixed.o
"${TOOLCHAIN_BIN}/sh-elf-gcc" -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I/c/Users/celso/libsaturn/include -c /c/Users/celso/libsaturn/src/math/matrix.c -o /c/Users/celso/libsaturn/src/math/matrix.o
"${TOOLCHAIN_BIN}/sh-elf-gcc" -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I/c/Users/celso/libsaturn/include -c /c/Users/celso/libsaturn/src/math/vector.c -o /c/Users/celso/libsaturn/src/math/vector.o
"${TOOLCHAIN_BIN}/sh-elf-gcc" -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I/c/Users/celso/libsaturn/include -c /c/Users/celso/libsaturn/src/cd/read.c -o /c/Users/celso/libsaturn/src/cd/read.o
"${TOOLCHAIN_BIN}/sh-elf-gcc" -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I/c/Users/celso/libsaturn/include -c /c/Users/celso/libsaturn/src/dma/scu_dma.c -o /c/Users/celso/libsaturn/src/dma/scu_dma.o
"${TOOLCHAIN_BIN}/sh-elf-gcc" -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I/c/Users/celso/libsaturn/include -c /c/Users/celso/libsaturn/src/dsp/dsp.c -o /c/Users/celso/libsaturn/src/dsp/dsp.o
"${TOOLCHAIN_BIN}/sh-elf-gcc" -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I/c/Users/celso/libsaturn/include -c /c/Users/celso/libsaturn/src/vdp1/init.c -o /c/Users/celso/libsaturn/src/vdp1/init.o
"${TOOLCHAIN_BIN}/sh-elf-gcc" -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I/c/Users/celso/libsaturn/include -c /c/Users/celso/libsaturn/src/vdp2/init.c -o /c/Users/celso/libsaturn/src/vdp2/init.o
"${TOOLCHAIN_BIN}/sh-elf-gcc" -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I/c/Users/celso/libsaturn/include -c /c/Users/celso/libsaturn/src/peripheral/controller.c -o /c/Users/celso/libsaturn/src/peripheral/controller.o

"${TOOLCHAIN_BIN}/sh-elf-ar" rcs /c/Users/celso/libsaturn/lib/libsaturn.a /c/Users/celso/libsaturn/src/crt0.o /c/Users/celso/libsaturn/src/system.o /c/Users/celso/libsaturn/src/dualcpu/slave.o /c/Users/celso/libsaturn/src/dualcpu/sync.o /c/Users/celso/libsaturn/src/math/fixed.o /c/Users/celso/libsaturn/src/math/matrix.o /c/Users/celso/libsaturn/src/math/vector.o /c/Users/celso/libsaturn/src/cd/read.o /c/Users/celso/libsaturn/src/dma/scu_dma.o /c/Users/celso/libsaturn/src/dsp/dsp.o /c/Users/celso/libsaturn/src/vdp1/init.o /c/Users/celso/libsaturn/src/vdp2/init.o /c/Users/celso/libsaturn/src/peripheral/controller.o

echo ""
echo "Build complete!"
echo "Library: lib/libsaturn.a"
