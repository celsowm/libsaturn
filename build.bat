@echo off
REM libsaturn Windows build script

setlocal enabledelayedexpansion

echo Building libsaturn...

REM Clean
echo Cleaning...
if exist lib\libsaturn.a del lib\libsaturn.a
for /r %%f in (*.o) do del "%%f"
for /r %%f in (*.elf) do del "%%f"

REM Build library
echo Building library...
sh-elf-gcc -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I./include -c src/crt0.s -o src/crt0.o
sh-elf-gcc -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I./include -c src/system.c -o src/system.o
sh-elf-gcc -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I./include -c src/dualcpu/slave.c -o src/dualcpu/slave.o
sh-elf-gcc -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I./include -c src/dualcpu/sync.c -o src/dualcpu/sync.o
sh-elf-gcc -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I./include -c src/math/fixed.s -o src/math/fixed.o
sh-elf-gcc -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I./include -c src/math/matrix.c -o src/math/matrix.o
sh-elf-gcc -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I./include -c src/math/vector.c -o src/math/vector.o
sh-elf-gcc -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I./include -c src/cd/read.c -o src/cd/read.o
sh-elf-gcc -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I./include -c src/dma/scu_dma.c -o src/dma/scu_dma.o
sh-elf-gcc -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I./include -c src/dsp/dsp.c -o src/dsp/dsp.o
sh-elf-gcc -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I./include -c src/vdp1/init.c -o src/vdp1/init.o
sh-elf-gcc -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I./include -c src/vdp2/init.c -o src/vdp2/init.o
sh-elf-gcc -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I./include -c src/peripheral/controller.c -o src/peripheral/controller.o

sh-elf-ar rcs lib\libsaturn.a src/crt0.o src/system.o src/dualcpu/slave.o src/dualcpu/sync.o src/math/fixed.o src/math/matrix.o src/math/vector.o src/cd/read.o src/dma/scu_dma.o src/dsp/dsp.o src/vdp1/init.o src/vdp2/init.o src/peripheral/controller.o

echo.
echo Build complete!
echo Library: lib\libsaturn.a
endlocal
