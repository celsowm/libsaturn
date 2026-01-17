# Setup Script Summary

## What Was Created

### 1. Setup Script (`setup.ps1`)
- **Scope**: Entry point + configuration
- **Modules**: `scripts/setup/*.ps1` holds the implementation
- **Features**:
  - ASCII art welcome screen
  - Interactive menu system
  - Real-time progress bars
  - Color-coded output (green/red/yellow/cyan/white)
  - Environment detection
  - Automatic toolchain download/installation
  - Python setup
  - Git repository cloning
  - Library compilation
  - Example building
  - Emulator installation (Kronos/YabaSanshiro)
  - VS Code configuration
  - Resume capability
  - Rollback system
  - Error handling
  - Verification checks

### 2. Quick Start Wrapper (`quick_setup.bat`)
- Calls `setup.ps1` with current directory
- Simple one-command entry point
- Handles PowerShell execution policy

### 3. Documentation Files
- **QUICK_START.md**: 60-second getting started guide
- **SETUP_PREVIEW.md**: Visual preview of setup process
- **WINDOWS_SETUP.md**: Detailed manual setup (fallback)
- Updated **README.md**: Added automated setup section

## Script Architecture

### Modules
- `setup.ps1`: Parameters + configuration + module loader
- `scripts/setup/ui.ps1`: UI/console helpers
- `scripts/setup/logging.ps1`: log file setup + logging helpers
- `scripts/setup/state.ps1`: state save/load/clear
- `scripts/setup/prerequisites.ps1`: environment checks and validation
- `scripts/setup/download.ps1`: download/extract helpers
- `scripts/setup/install.ps1`: installers (MSYS2/toolchain/python/repo/build/emulators/VS Code)
- `scripts/setup/rollback.ps1`: cleanup/rollback
- `scripts/setup/main.ps1`: orchestration + entrypoint functions

## User Experience

### Interactive Mode
```
1. Welcome Screen (ASCII art)
2. Installation Mode Selection
   - [1] Express (Recommended)
   - [2] Custom Configuration
   - [3] Resume Previous Setup
3. Progress Through 8 Steps
   - Each step with progress bar
   - Color-coded success/error messages
   - Option to continue on errors
4. Completion Screen
   - Success summary
   - Next steps menu
   - Launch emulator/VS Code options
```

### Express Mode
```
1. Welcome Screen
2. Automatic execution of all steps
3. Progress bars only (no prompts)
4. Completion Screen
```

## Progress Bars

### Visual Style
```
[████████████████████████████] 100% - Installing Toolchain
[████████░░░░░░░░░░░░░░░░░░] 45%  - Building Library
[████████████████████████████] 100% - Complete!
```

### Progress Tracking
- Download progress (bytes/MB)
- Extraction progress
- Compilation progress (time-based)
- Overall setup progress

## Color Coding

- **Cyan** (╔═╗): Section headers and UI elements
- **Green** (✓): Success messages
- **Red** (✗): Errors
- **Yellow** (!): Warnings
- **White** (i): Information

## Smart Features

### Resume Capability
- Saves progress to JSON file
- Can interrupt and continue later
- Skips completed steps

### Rollback System
- Tracks all changes (PATH, files)
- Can undo if setup fails
- Clean on error

### Verification
- Validates each installation step
- Checks file existence
- Tests executables
- Confirms PATH changes

### Error Handling
- Auto-retry failed downloads
- Prompt user on errors
- Continue option for non-critical failures
- Detailed error messages

## Installation Flow

### Step 1: Environment Check
- Check PowerShell version
- Check administrator rights
- Check for SH-ELF GCC
- Check for Python
- Check for Git
- Display missing dependencies

### Step 2: Install Toolchain
- Download MCX-SDK (~100MB)
- Extract to installation path
- Add to system PATH
- Verify installation

### Step 3: Install Python
- Download Python installer
- Silent installation
- Verify Python command

### Step 4: Clone Repository
- Git clone libsaturn
- Or assume already exists
- Verify repository structure

### Step 5: Build Library
- Run build.bat
- Compile all source files
- Create libsaturn.a
- Verify library file

### Step 6: Build Examples
- Compile each example
- Create 0.BIN files
- Verify example binaries

### Step 7: Install Emulator
- Download Kronos/YabaSanshiro
- Extract to installation path
- Verify emulator executable

### Step 8: Setup VS Code
- Create .vscode directory
- Generate c_cpp_properties.json
- Generate tasks.json
- Configure IntelliSense

## Command-Line Options

```cmd
# Quick setup (express mode)
quick_setup.bat

# Manual setup (interactive)
powershell -ExecutionPolicy Bypass -File setup.ps1

# Express mode (auto-confirm)
powershell -ExecutionPolicy Bypass -File setup.ps1 -Express

# Custom install path
powershell -ExecutionPolicy Bypass -File setup.ps1 -InstallPath "C:\my-saturn"

# Resume previous setup
powershell -ExecutionPolicy Bypass -File setup.ps1 -Resume

# Offline mode (use cached)
powershell -ExecutionPolicy Bypass -File setup.ps1 -Offline
```

## File Locations After Setup

```
%USERPROFILE%\saturn-sdk\
├── mcx-sdk\              # Toolchain
│   └── bin\
│       └── sh-elf-gcc.exe
├── libsaturn\             # SDK (if not in current dir)
│   ├── include\
│   ├── src\
│   ├── examples\
│   └── lib\
├── Kronos\               # Emulator
│   └── Kronos.exe
└── .vscode\              # VS Code config
    ├── c_cpp_properties.json
    └── tasks.json
```

## Dependencies Downloaded

1. **MCX-SDK v2.0** (~100MB)
   - SH-ELF GCC toolchain
   - Binaries (no compilation needed)

2. **Python 3.11** (~25MB)
   - Python interpreter
   - pip package manager

3. **Kronos v2.7.2** (~20MB)
   - Saturn emulator
   - Debugging support

4. **libsaturn** (if not present)
   - Git repository
   - Source code

## Time Required

- **Express Mode**: 5-10 minutes (depending on internet)
- **Custom Mode**: 10-15 minutes (with user input)
- **Resume Mode**: Skips completed steps

## System Requirements

- Windows 7/8/10/11
- PowerShell 5.1+ (check: `$PSVersionTable.PSVersion`)
- 500MB free disk space
- Internet connection (first run)
- Administrator rights (recommended)

## Troubleshooting

### Common Issues

1. **"sh-elf-gcc not found" after setup**
   - Close/reopen terminal (PATH refresh)
   - Run: `refreshenv` (if using Windows Terminal)
   - Or restart computer

2. **Download failed**
   - Check internet connection
   - Run setup again (will resume)
   - Script retries automatically

3. **Permission denied**
   - Run PowerShell as Administrator
   - Or use user-level PATH (limited features)

4. **"PowerShell not found"**
   - PowerShell 5.1+ included with Windows
   - Update Windows to latest version

### Logging

Setup script creates:
- `%TEMP%\saturn-setup-resume.json` (progress state)
- Console output (all steps logged)

### Rollback

If setup fails:
- Script displays what succeeded/failed
- Can manually remove installed components
- Run setup again (will resume/skip)

## Testing

After setup completes:

```cmd
# Test toolchain
sh-elf-gcc --version

# Test Python
python --version

# Test build
cd C:\Users\celso\saturn-sdk\libsaturn
build.bat

# Test emulator
cd C:\Users\celso\saturn-sdk\Kronos
.\Kronos.exe ..\libsaturn\examples\01_helloworld\0.BIN
```

## Customization

### Modify Toolchain URL

Edit `setup.ps1` in the `Toolchain` config block:
```powershell
Url = "https://your-custom-url/toolchain.zip"
```

### Change Default Install Path

Edit the default parameter in `setup.ps1`:
```powershell
$InstallPath = "C:\your\custom\path"
```

### Add Custom Setup Steps

Add a new function to the setup steps array in `scripts/setup/main.ps1` (`Start-ExpressSetup`):
```powershell
@{ Name = "My Custom Step"; Script = { My-CustomFunction } }
```

## Summary

The setup script provides:
- ✅ One-command installation
- ✅ Interactive and express modes
- ✅ Real-time progress bars
- ✅ Color-coded output
- ✅ Error handling
- ✅ Resume capability
- ✅ Rollback system
- ✅ VS Code integration
- ✅ Emulator setup
- ✅ Complete environment ready in 10 minutes

No manual steps required beyond running `quick_setup.bat`!
