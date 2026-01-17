# Windows Setup Guide

## Prerequisites

### 1. SH-ELF Toolchain

Download and install a Windows SH-ELF GCC toolchain:

**Option A: Pre-built Binaries**
- https://github.com/jetsetilly/mcx-sdk/releases (recommended)
- Extract to: `C:\sh-elf-gcc`

**Option B: Build from Source**
- Clone https://github.com/kmc-jp/sh-elf-gcc-toolchain
- Follow build instructions in README

**Option C: Use MSYS2**
```powershell
pacman -S mingw-w64-i686-sh-elf-gcc
```

### 2. Add to PATH

Add toolchain to system PATH:
```
C:\sh-elf-gcc\bin
```

Verify installation:
```cmd
sh-elf-gcc --version
```

### 3. Build Tools

**Gnu Make for Windows:**
- Download: http://gnuwin32.sourceforge.net/packages/make.htm
- Add to PATH

**Python 3:**
- Download: https://www.python.org/downloads/
- Install and add to PATH

**mkisofs (for ISO creation):**
- Download: https://cdrtools.sourceforge.net/private/win32/
- Extract and add to PATH

## Building

### Quick Start

```cmd
build.bat
```

### Clean Build

```cmd
clean.bat
build.bat
```

### Build Examples

Each example has its own `Makefile` (requires GNU Make):

```cmd
cd examples\01_helloworld
mingw32-make
```

Or modify to use Windows batch files.

## VS Code Setup

### .vscode/c_cpp_properties.json
```json
{
    "configurations": [
        {
            "name": "Saturn",
            "includePath": [
                "${workspaceFolder}/include",
                "${workspaceFolder}/include/saturn"
            ],
            "compilerPath": "C:/sh-elf-gcc/bin/sh-elf-gcc.exe",
            "cStandard": "c99",
            "intelliSenseMode": "gcc-x86"
        }
    ]
}
```

### .vscode/tasks.json
```json
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "Build Library",
            "type": "shell",
            "command": "build.bat",
            "problemMatcher": []
        },
        {
            "label": "Clean",
            "type": "shell",
            "command": "clean.bat",
            "problemMatcher": []
        }
    ]
}
```

## Troubleshooting

### "sh-elf-gcc not found"
- Verify toolchain is installed
- Add `C:\sh-elf-gcc\bin` to PATH
- Restart terminal/VS Code

### "make not recognized"
- Install GNU Make for Windows
- Add to PATH
- Or use Windows batch scripts

### Python script errors
```cmd
python --version
python tools\obj2saturn\obj2saturn.py model.obj model.c
```

### Path issues with forward slashes
Windows batch scripts automatically handle forward/backward slashes.

## Alternative: WSL (Windows Subsystem for Linux)

If you prefer a Unix-like environment:

1. Install WSL:
```powershell
wsl --install -d Ubuntu
```

2. Install toolchain in WSL:
```bash
sudo apt update
sudo apt install gcc-sh-elf binutils-sh-elf make python3
```

3. Build from WSL terminal:
```bash
cd /mnt/c/Users/celso/libsaturn
make lib
```

## Common Windows Issues

### Long Path Names
Enable long path support:
```powershell
# Run as Administrator
New-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem" -Name "LongPathsEnabled" -Value 1 -PropertyType DWORD -Force
```

### Antivirus Interference
Add libsaturn folder to antivirus exclusions.

### Makefile Issues
If Makefiles don't work, convert to batch scripts or use CMake.

## Emulator Testing

Popular Windows Saturn emulators:
- **Kronos**: https://github.com/FCare/Kronos/releases
- **YabaSanshiro**: https://github.com/devmiyax/yabause
- **Mednafen**: https://mednafen.github.io/

Load `examples/01_helloworld/0.BIN` directly.

## IDE Support

### Visual Studio Code
- C/C++ extension
- Makefile Tools extension (if using Makefiles)

### Visual Studio 2019/2022
- Requires CMake support (not yet implemented)
- Use VS Code or CLI for now
