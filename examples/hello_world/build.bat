@echo off
REM Hello World Example Build Script
REM Builds the hello_world example using libsaturn

echo ====================================
echo libsaturn Hello World Example
echo ====================================
echo.

REM Change to script directory
cd /d "%~dp0"

REM Check if libsaturn is built
if not exist "..\lib\libsaturn.a" (
    echo [INFO] libsaturn not found, building first...
    cd ..
    call build.bat
    cd examples\hello_world
)

REM Check for toolchain
if not exist "..\..\toolchains\sh-elf-gcc\bin\sh-elf-gcc.exe" (
    if not exist "%USERPROFILE%\saturn-sdk\sh-elf-gcc\bin\sh-elf-gcc.exe" (
        echo [ERROR] Could not find sh-elf-gcc toolchain
        echo Please ensure the toolchain is installed
        exit /b 1
    )
)

REM Set paths
set TOOLCHAIN_PATH=..\..\toolchains\sh-elf-gcc\bin
if exist "%USERPROFILE%\saturn-sdk\sh-elf-gcc\bin\sh-elf-gcc.exe" (
    set TOOLCHAIN_PATH=%USERPROFILE%\saturn-sdk\sh-elf-gcc\bin
)

set CC=%TOOLCHAIN_PATH%\sh-elf-gcc.exe
set OBJCOPY=%TOOLCHAIN_PATH%\sh-elf-objcopy.exe
set LIB_DIR=..\lib
set INCLUDE_DIR=..\include

REM Compiler flags
set CFLAGS=-m2 -mb -O2 -fomit-frame-pointer -nostartfiles
set CFLAGS=%CFLAGS% -I%INCLUDE_DIR%
set CFLAGS=%CFLAGS% -Wall

REM Linker flags
set LDFLAGS=-T..\scripts\linker.ld
set LDFLAGS=%LDFLAGS% -Map=hello_world.map

REM Clean old files
echo Cleaning...
del /Q *.o *.elf *.bin *.map 2>nul

REM Build
echo Building hello_world...
%CC% %CFLAGS% %LDFLAGS% -o hello_world.elf main.c -L%LIB_DIR% -lsaturn -lgcc

if errorlevel 1 (
    echo [ERROR] Build failed
    exit /b 1
)

REM Convert to binary
echo Converting to binary...
%OBJCOPY% -O binary hello_world.elf hello_world.bin

REM Verify
if exist "hello_world.bin" (
    for %%i in (hello_world.bin) do set SIZE=%%~zi
    echo.
    echo ====================================
    echo Build Complete!
    echo ====================================
    echo.
    echo Output: hello_world.bin (%SIZE% bytes)
    echo.
    echo Run with Saturn emulator:
    echo   kronos.exe hello_world.bin
    echo.
) else (
    echo [ERROR] Build failed - binary not created
    exit /b 1
)
