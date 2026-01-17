# 🚀 Quick Start Guide

## Getting Started in 60 Seconds

### Windows (One Command)
```cmd
quick_setup.bat
```

That's it! The script will:
1. Download and install SH-ELF toolchain
2. Configure your environment
3. Build libsaturn and examples
4. Install Kronos emulator
5. Launch first example in emulator

## What to Expect

### Interactive Setup Flow
```
1. Welcome Screen (ASCII art)
2. Choose Mode: Express or Custom
3. Environment Check
4. Download Toolchain (with progress bar)
5. Install Python
6. Clone libsaturn repository
7. Build Library (compilation progress)
8. Build Examples
9. Install Emulator
10. Setup VS Code
11. Completion Screen
```

### Time Required
- **Express Mode**: ~5-10 minutes (depending on internet speed)
- **Custom Mode**: ~10-15 minutes (with user input)

## Files You'll Get

```
C:\Users\celso\saturn-sdk\
├── mcx-sdk\              # SH-ELF toolchain
├── libsaturn\             # SDK source code
│   ├── include\           # Headers
│   ├── src\              # Implementation
│   ├── examples\         # Demo programs
│   └── lib\              # Compiled library
├── Kronos\               # Emulator
└── .vscode\              # VS Code config
```

## First Program

After setup, your first example is ready:

```cmd
cd C:\Users\celso\saturn-sdk\libsaturn\examples\01_helloworld
Kronos 0.BIN
```

Or use VS Code to explore examples.

## Troubleshooting

### "sh-elf-gcc not found"
Run setup again with `-Resume` flag:
```cmd
powershell -ExecutionPolicy Bypass -File setup.ps1 -Resume
```

### "PowerShell not found"
PowerShell 5.1+ is included with Windows 7+. If missing, download from Microsoft.

### Download Failed
Check internet connection, then run setup again. Script will resume from last completed step.

### Permission Denied
Run PowerShell as Administrator for PATH modification (optional but recommended).

## System Requirements

- **OS**: Windows 7/8/10/11
- **PowerShell**: 5.1+ (check: `$PSVersionTable.PSVersion`)
- **Disk Space**: 500MB free
- **Internet**: Required for first run
- **RAM**: 4GB+ recommended

## Next Steps

After setup completes:

1. **Explore Examples**
   ```cmd
   code C:\Users\celso\saturn-sdk\libsaturn\examples
   ```

2. **Read Documentation**
   ```
   libsaturn\README.md          # Overview
   libsaturn\STRUCTURE.md        # SDK structure
   libsaturn\docs\              # Detailed guides
   ```

3. **Start Coding**
   ```c
   #include "saturn/shared.h"
   #include "saturn/vdp1.h"
   
   void _main(void) {
       vdp1_init();
       // Your code here
   }
   ```

## Command Reference

```cmd
# Quick setup (recommended)
quick_setup.bat

# Express mode (auto-confirm all)
powershell -ExecutionPolicy Bypass -File setup.ps1 -Express

# Custom path
powershell -ExecutionPolicy Bypass -File setup.ps1 -InstallPath "C:\my-saturn"

# Resume interrupted setup
powershell -ExecutionPolicy Bypass -File setup.ps1 -Resume

# Offline mode (use cached downloads)
powershell -ExecutionPolicy Bypass -File setup.ps1 -Offline
```

## Support

- **Documentation**: libsaturn\docs\
- **Examples**: libsaturn\examples\
- **Setup Preview**: SETUP_PREVIEW.md
- **Manual Setup**: WINDOWS_SETUP.md

## Tips

1. **First Run**: Use Express mode for fastest setup
2. **Customization**: Run again in Custom mode to adjust paths
3. **Updates**: Re-run script to update toolchain
4. **Backup**: Setup folder can be copied to another machine

---

**Questions?** Check `WINDOWS_SETUP.md` for detailed manual setup.
