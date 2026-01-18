@echo off
REM Saturn SDK Toolchain Setup Script
REM This script helps set up the SH-ELF toolchain for Sega Saturn development

echo ====================================
echo Saturn SDK Toolchain Setup
echo ====================================
echo.

setlocal enabledelayedexpansion

set "SATURN_SDK=C:\Users\celso\saturn-sdk"
set "TOOLCHAIN_PATH=%SATURN_SDK%\sh-elf-gcc\bin"

REM Check if toolchain already exists
if exist "%TOOLCHAIN_PATH%\sh-elf-gcc.exe" (
    echo Toolchain appears to be already installed.
    sh-elf-gcc -v
    echo.
    echo You can now run build.bat to build libsaturn.
    goto :end
)

echo Toolchain not found. Setting up...
echo.

REM Option 1: Download pre-built toolchain
echo Option 1: Download pre-built toolchain
echo ====================================
echo The GNU SH-ELF toolchain is available from SegaXtreme:
echo https://segaxtreme.net/resources/gnu-sh-elf-toolchain.467/
echo.
echo Download steps:
echo 1. Visit the above URL
echo 2. Click "Download" to get the toolchain archive
echo 3. Extract the archive
echo 4. Copy all files from bin\ to %TOOLCHAIN_PATH%\
echo.

REM Option 2: Build from source
echo Option 2: Build from source (requires WSL/Linux)
echo ====================================
echo.
echo Build steps:
echo 1. Clone the toolchain repository:
echo    git clone https://github.com/kentosama/sh2-elf-gcc.git
echo.
echo 2. Install dependencies (requires root):
echo    sudo apt update
echo    sudo apt install build-essential texinfo wget
echo.
echo 3. Build the toolchain:
echo    cd sh2-elf-gcc
echo    ./build-toolchain.sh
echo.
echo 4. Copy the binaries:
echo    cp -r sh2-toolchain/bin/* %TOOLCHAIN_PATH%\
echo.

REM Create directory structure
if not exist "%SATURN_SDK%" mkdir "%SATURN_SDK%"
if not exist "%TOOLCHAIN_PATH%" mkdir "%TOOLCHAIN_PATH%"

echo.
echo ====================================
echo Manual Setup Instructions
echo ====================================
echo.
echo After downloading/extracting the toolchain:
echo.
echo 1. Ensure all sh-elf-* executables are in:
echo    %TOOLCHAIN_PATH%
echo.
echo 2. Add to PATH (optional):
echo    set PATH=%PATH%;%TOOLCHAIN_PATH%
echo.
echo 3. Verify installation:
echo    sh-elf-gcc -v
echo.
echo 4. Run build.bat to compile libsaturn
echo.

:end
endlocal
pause
