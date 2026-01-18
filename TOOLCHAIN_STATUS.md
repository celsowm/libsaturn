# Saturn SDK Toolchain Setup - Status Report

## What Has Been Done

1. **Downloaded and extracted** the GNU SH-ELF toolchain for Dreamcast/Saturn
2. **Installed to:** `C:\Users\celso\saturn-sdk\sh-elf-gcc\`
3. **Toolchain version:** GCC 4.2-GNUSH_v0603

## Current Status: PARTIALLY WORKING

### The Issue
The downloaded toolchain is a **Cygwin-based Windows application**. This means:
- It works when run from a **real Windows Command Prompt**
- It has compatibility issues when run from **WSL (Windows Subsolver for Linux)**

### Testing Results

**Working (Windows CMD):**
```
C:\> C:\Users\celso\saturn-sdk\sh-elf-gcc\bin\sh-elf-gcc --version
sh-elf-gcc.exe (GCC) 4.2-GNUSH_v0603
```

**Not Working (WSL):**
```bash
$ /c/Users/celso/saturn-sdk/sh-elf-gcc/bin/sh-elf-gcc --version
sh-elf-gcc.exe: CreateProcess: No such file or directory
```

## How to Complete the Build

### Option 1: Run build.bat from Windows CMD (Recommended)

1. Open **Windows Command Prompt** (cmd.exe)
2. Navigate to the libsaturn directory:
   ```
   cd C:\Users\celso\libsaturn
   ```
3. Run the build:
   ```
   build.bat
   ```

### Option 2: Add toolchain to Windows PATH (Permanent)

1. Open **System Properties** → **Advanced** → **Environment Variables**
2. Add to PATH:
   ```
   C:\Users\celso\saturn-sdk\sh-elf-gcc\bin
   ```
3. Open a new CMD window and run:
   ```
   cd C:\Users\celso\libsaturn
   build.bat
   ```

### Option 3: Use Native Linux Toolchain (Advanced)

If you need to build from WSL/Linux, you'll need a native Linux toolchain:
- Clone and build: https://github.com/kentosama/sh2-elf-gcc
- Requires: `sudo apt install build-essential texinfo wget`

## Files Location

```
C:\Users\celso\saturn-sdk\sh-elf-gcc\
├── bin\                    # Toolchain executables
│   ├── sh-elf-gcc.exe     # C/C++ compiler
│   ├── sh-elf-ar.exe      # Archiver (for creating .a libraries)
│   ├── sh-elf-as.exe      # Assembler
│   ├── sh-elf-ld.exe      # Linker
│   └── *.dll             # Cygwin runtime libraries
├── include\               # Header files
└── lib\                   # Libraries and runtime files
```

## Build Output

After successful build, you should have:
```
C:\Users\celso\libsaturn\lib\libsaturn.a
```

## Troubleshooting

**"sh-elf-gcc is not recognized":**
- Ensure toolchain is in PATH or use full path
- Run from Windows CMD, not WSL

**"CreateProcess error":**
- This is expected when running from WSL
- Switch to Windows CMD

**Missing DLLs:**
- Ensure all .dll files are in the bin directory
- Don't move individual .exe files out of the bin directory
