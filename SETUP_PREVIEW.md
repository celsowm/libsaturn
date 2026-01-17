# Setup Script Preview

## What You'll See When Running quick_setup.bat

### Step 1: Welcome Screen
```
╔════════════════════════════════════════════════════════════════════════════╗
║   🔮       SATURN DEVELOPMENT SETUP       libsaturn v1.0.0              ║
╚════════════════════════════════════════════════════════════════════════════╝
```

### Step 2: Installation Mode Selection
```
INSTALLATION MODE
════════════════════
    [1] Express (Recommended - All Defaults)
    [2] Custom Configuration
    [3] Resume Previous Setup

Select option: 1
```

### Step 3: Environment Check
```
[?] ENVIRONMENT CHECK
═════════════════════
[✗] SH-ELF GCC not found - will install
[✗] Python not found - will install
[i] Missing dependencies: Toolchain, Python
```

### Step 4: Download Progress
```
[?] INSTALLING SH-ELF TOOLCHAIN
═════════════════════════════════════════════════════════════
[████████████████████████████] 100% - Installing Toolchain
[✓] Downloaded SH-ELF Toolchain (98.45 MB)
[✓] Extracted Toolchain
[✓] Added to PATH: C:\Users\celso\saturn-sdk\mcx-sdk\bin
[✓] Toolchain installed to: C:\Users\celso\saturn-sdk\mcx-sdk
```

### Step 5: Building Library
```
[?] BUILDING LIBSATURN LIBRARY
═════════════════════════════════════════════════════════════
[████████░░░░░░░░░░░░░░░░░░] 45% - Building Library
[████████████████████████████] 100% - Building Library
[✓] Library built successfully
```

### Step 6: Completion Screen
```
╔════════════════════════════════════════════════════════════════════════════╗
║   🔮       SATURN DEVELOPMENT SETUP       libsaturn v1.0.0              ║
╚════════════════════════════════════════════════════════════════════════════╝

[✓] Setup completed successfully!

🚀 Saturn development environment is ready!

Next steps:
  1. Open VS Code: code C:\Users\celso\saturn-sdk
  2. Review README.md for documentation
  3. Check examples folder for demos

[?] WHAT'S NEXT?
════════════════════
    [1] Launch Emulator with First Example
    [2] Open VS Code
    [3] Open README
    [4] Exit

Select option: 1
```

## Color Legend

- **Cyan** (╔═╗): Section headers and borders
- **Green** (✓): Success messages
- **Red** (✗): Errors
- **Yellow** (!): Warnings
- **White** (i): Information

## Progress Bars

The script displays real-time progress bars for:

- Downloading toolchain (100MB)
- Extracting archives
- Building library (compilation time)
- Building examples
- Installing emulator

## Features

### Interactive Mode
- Choose installation options
- Customize install paths
- Select preferred emulator
- Resume interrupted setup

### Express Mode
- All defaults pre-selected
- One-click installation
- Fastest way to get started

### Smart Features
- **Auto-Detection**: Checks for existing tools
- **Resume**: Pick up where you left off
- **Rollback**: Undo changes on failure
- **Verification**: Validates each step
- **Offline Mode**: Uses cached downloads

## Command Line Options

```cmd
# Quick setup (express mode)
quick_setup.bat

# Manual setup (interactive)
powershell -ExecutionPolicy Bypass -File setup.ps1

# Custom install path
powershell -ExecutionPolicy Bypass -File setup.ps1 -InstallPath "C:\saturn-dev"

# Resume previous setup
powershell -ExecutionPolicy Bypass -File setup.ps1 -Resume
```

## What Gets Installed

1. **SH-ELF GCC Toolchain** (~100MB)
   - sh-elf-gcc compiler
   - sh-elf-ld linker
   - sh-elf-objcopy
   - sh-elf-ar archiver

2. **Python 3** (~25MB)
   - Python interpreter
   - pip package manager

3. **Kronos Emulator** (~20MB)
   - Accurate Saturn emulation
   - Debugging support

4. **libsaturn SDK**
   - Source code
   - Headers
   - Build system
   - Examples

5. **VS Code Configuration**
   - IntelliSense
   - Build tasks
   - Debugging setup

## Troubleshooting

If setup fails, the script provides:
- Error messages with color coding
- Option to continue despite errors
- Rollback capability
- Detailed logs

## System Requirements

- Windows 7/8/10/11
- PowerShell 5.1+ (included with Windows)
- 500MB free disk space
- Internet connection (first run)
- Administrator rights (recommended for PATH modification)
