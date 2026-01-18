# Saturn SDK Toolchain Setup Guide

This guide will help you set up the SH-ELF toolchain for Sega Saturn development.

## Prerequisites

- Windows 10/11 with WSL (Windows Subsystem for Linux) or
- Access to a Linux system or
- Pre-built Windows toolchain binaries

## Setup Options

### Option 1: Download Pre-built Toolchain (Recommended)

1. **Download the toolchain:**
   - Visit: https://segaxtreme.net/resources/gnu-sh-elf-toolchain.467/
   - Click "Download" to get the latest toolchain archive

2. **Extract and install:**
   - Extract the downloaded archive
   - Copy all files from the `bin/` directory to:
     ```
     C:\Users\celso\saturn-sdk\sh-elf-gcc\bin\
     ```

3. **Verify installation:**
   ```cmd
   sh-elf-gcc -v
   ```

### Option 2: Build from Source (WSL/Linux)

1. **Clone the toolchain repository:**
   ```bash
   git clone https://github.com/kentosama/sh2-elf-gcc.git
   cd sh2-elf-gcc
   ```

2. **Install build dependencies:**
   ```bash
   sudo apt update
   sudo apt install build-essential texinfo wget
   ```

3. **Build the toolchain:**
   ```bash
   ./build-toolchain.sh
   ```
   This process takes 15-60 minutes depending on your system.

4. **Copy the binaries:**
   ```bash
   cp -r sh2-toolchain/bin/* /mnt/c/Users/celso/saturn-sdk/sh-elf-gcc/bin/
   ```

5. **Verify installation:**
   ```cmd
   sh-elf-gcc -v
   ```

## Expected Toolchain Files

After setup, you should have these executables in `C:\Users\celso\saturn-sdk\sh-elf-gcc\bin\`:

- `sh-elf-gcc.exe` - C/C++ compiler
- `sh-elf-g++.exe` - C++ compiler  
- `sh-elf-as.exe` - Assembler
- `sh-elf-ld.exe` - Linker
- `sh-elf-ar.exe` - Archiver (required for building libsaturn.a)
- `sh-elf-objcopy.exe` - Object file converter
- `sh-elf-size.exe` - Size analysis tool
- Other supporting utilities

## Build libsaturn

Once the toolchain is set up:

1. **Run the build script:**
   ```cmd
   build.bat
   ```

2. **Expected output:**
   - Library: `lib\libsaturn.a`
   - No errors from sh-elf-gcc or sh-elf-ar

## Troubleshooting

### "sh-elf-gcc is not recognized"
- Ensure the toolchain bin directory is in your PATH, or use full path:
  ```
  C:\Users\celso\saturn-sdk\sh-elf-gcc\bin\sh-elf-gcc.exe -v
  ```

### "sh-elf-ar.exe not found"
- The archiver is essential for creating the static library
- Re-download the complete toolchain if missing

### Build errors during compilation
- Verify toolchain version with `sh-elf-gcc -v`
- Ensure all source files are present in the expected locations
- Check that include paths are correct

## Alternative Toolchain Sources

- **GitHub (SaturnSDK):** https://github.com/SaturnSDK/Saturn-SDK-GCC-SH2
- **GitHub (kentosama):** https://github.com/kentosama/sh2-elf-gcc
- **SegaXtreme Forums:** https://segaxtreme.net/

## Next Steps

After successful toolchain setup:
1. Run `build.bat` to compile libsaturn
2. Check `lib\libsaturn.a` was created
3. Use the library in your Saturn projects
