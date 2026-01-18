@echo off
REM libsaturn Windows build script using WSL

setlocal enabledelayedexpansion

set "SATURN_SDK=C:\Users\celso\saturn-sdk"
set "LIBSATURN_DIR=C:\Users\celso\libsaturn"

echo Building libsaturn...

REM Run build through WSL
wsl bash -c "cd %LIBSATURN_DIR_DIR:\=/ && ~/saturn-sdk/sh-elf-gcc/bin/sh-elf-ar rcs lib/libsaturn.a src/crt0.o src/system.o src/dualcpu/slave.o src/dualcpu/sync.o src/math/fixed.o src/math/matrix.o src/math/vector.o src/cd/read.o src/dma/scu_dma.o src/dsp/dsp.o src/vdp1/init.o src/vdp2/init.o src/peripheral/controller.o"

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Build complete!
    echo Library: lib\libsaturn.a
) else (
    echo Build failed!
)

endlocal
pause
