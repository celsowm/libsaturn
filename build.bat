@echo off
REM libsaturn Windows build script v2.0
REM Auto-detects toolchain and generates pkg-config file

echo Building libsaturn...

REM ============================================================================
REM AUTO-DETECT TOOLCHAIN
REM ============================================================================

if exist "toolchains\sh-elf-gcc\bin\sh-elf-gcc.exe" (
    set TOOLCHAIN_PATH=toolchains\sh-elf-gcc\bin
    echo [INFO] Using bundled toolchain
    goto :got_toolchain
)

if exist "%USERPROFILE%\saturn-sdk\sh-elf-gcc\bin\sh-elf-gcc.exe" (
    set TOOLCHAIN_PATH=%USERPROFILE%\saturn-sdk\sh-elf-gcc\bin
    echo [INFO] Using user toolchain
    goto :got_toolchain
)

if exist "C:\saturn-sdk\sh-elf-gcc\bin\sh-elf-gcc.exe" (
    set TOOLCHAIN_PATH=C:\saturn-sdk\sh-elf-gcc\bin
    echo [INFO] Using C:\saturn-sdk toolchain
    goto :got_toolchain
)

if exist "C:\msys64\mingw64\bin\sh-elf-gcc.exe" (
    set TOOLCHAIN_PATH=C:\msys64\mingw64\bin
    echo [INFO] Using MSYS2 mingw64 toolchain
    goto :got_toolchain
)

if exist "%ProgramFiles%\SaturnSDK\sh-elf-gcc\bin\sh-elf-gcc.exe" (
    set TOOLCHAIN_PATH=%ProgramFiles%\SaturnSDK\sh-elf-gcc\bin
    echo [INFO] Using SaturnSDK toolchain
    goto :got_toolchain
)

if exist "C:\sh-elf-gcc\bin\sh-elf-gcc.exe" (
    set TOOLCHAIN_PATH=C:\sh-elf-gcc\bin
    echo [INFO] Using C:\sh-elf-gcc toolchain
    goto :got_toolchain
)

echo [ERROR] Could not find sh-elf-gcc toolchain
echo.
echo Searched locations:
echo   - toolchains\sh-elf-gcc\bin\sh-elf-gcc.exe (bundled)
echo   - %%USERPROFILE%%\saturn-sdk\sh-elf-gcc\bin
echo   - C:\saturn-sdk\sh-elf-gcc\bin
echo   - C:\msys64\mingw64\bin
echo   - %%ProgramFiles%%\SaturnSDK\sh-elf-gcc\bin
echo   - C:\sh-elf-gcc\bin
echo.
echo Please install or add to PATH
exit /b 1

:got_toolchain
set CC=%TOOLCHAIN_PATH%\sh-elf-gcc.exe
set AR=%TOOLCHAIN_PATH%\sh-elf-ar.exe

REM Verify toolchain
%CC% --version >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Toolchain verification failed: %CC%
    exit /b 1
)

for /f "tokens=3" %%v in ('%CC% --version ^| findstr "GCC"') do set TOOLCHAIN_VERSION=%%v
echo [INFO] Toolchain: GCC %TOOLCHAIN_VERSION%

REM ============================================================================
REM CLEAN
REM ============================================================================
echo.
echo Cleaning...
if exist lib\libsaturn.a del lib\libsaturn.a
for /r %%f in (*.o) do del "%%f" 2>nul
for /r %%f in (*.elf) do del "%%f" 2>nul
if exist libsaturn.pc del libsaturn.pc 2>nul

REM ============================================================================
REM BUILD LIBRARY
REM ============================================================================
echo.
echo Building library...

%CC% -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I./include -c src/crt0.s -o src/crt0.o
%CC% -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I./include -c src/system.c -o src/system.o
%CC% -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I./include -c src/dualcpu/slave.c -o src/dualcpu/slave.o
%CC% -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I./include -c src/dualcpu/sync.c -o src/dualcpu/sync.o
%CC% -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I./include -c src/math/fixed.c -o src/math/fixed.o
%CC% -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I./include -c src/math/matrix.c -o src/math/matrix.o
%CC% -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I./include -c src/math/vector.c -o src/math/vector.o
%CC% -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I./include -c src/cd/read.c -o src/cd/read.o
%CC% -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I./include -c src/dma/scu_dma.c -o src/dma/scu_dma.o
%CC% -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I./include -c src/dsp/dsp.c -o src/dsp/dsp.o
%CC% -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I./include -c src/vdp1/init.c -o src/vdp1/init.o
%CC% -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I./include -c src/vdp2/init.c -o src/vdp2/init.o
%CC% -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I./include -c src/peripheral/controller.c -o src/peripheral/controller.o

%AR% rcs lib\libsaturn.a src/crt0.o src/system.o src/dualcpu/slave.o src/dualcpu/sync.o src/math/fixed.o src/math/matrix.o src/math/vector.o src/cd/read.o src/dma/scu_dma.o src/dsp/dsp.o src/vdp1/init.o src/vdp2/init.o src/peripheral/controller.o

REM ============================================================================
REM GENERATE PKG-CONFIG FILE
REM ============================================================================
echo.
echo Generating pkg-config file...

set SCRIPT_DIR=%~dp0
set LIB_DIR=%SCRIPT_DIR%lib
set INCLUDE_DIR=%SCRIPT_DIR%include

(
    echo.prefix=%SCRIPT_DIR%
    echo.exec_prefix=^${prefix}
    echo.bindir=^${exec_prefix}/toolchains/sh-elf-gcc/bin
    echo.libdir=^${exec_prefix}/lib
    echo.incdir=^${exec_prefix}/include
    echo.
    echo.Name: libsaturn
    echo.Description: Sega Saturn development library
    echo.Version: 1.0.0
    echo.Cflags: -I^${incdir}
    echo.Libs: -L^${libdir} -lsaturn
) > libsaturn.pc

echo [OK] Created libsaturn.pc

REM ============================================================================
REM VERIFY BUILD
REM ============================================================================
echo.
echo Build complete!
echo.
if exist lib\libsaturn.a (
    for %%i in (lib\libsaturn.a) do set LIB_SIZE=%%~zi
    echo [OK] Library: lib\libsaturn.a (%LIB_SIZE% bytes)
    echo [OK] Toolchain: GCC %TOOLCHAIN_VERSION%
    echo.
    echo Contents:
    %AR% -t lib\libsaturn.a | findstr /r /c:"[a-z]" | for /f %%a in ('more') do echo    - %%a
) else (
    echo [ERROR] Library not found!
    exit /b 1
)

echo.
echo To use in your project:
echo   INCLUDES += -I%%CD%%/include
echo   LIBRARIES += -L%%CD%%/lib -lsaturn
