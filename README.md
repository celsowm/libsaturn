# libsaturn

Bare Metal Sega Saturn 3D Game Engine SDK

## 🔮 One-Command Setup (Recommended)

**Windows:**
```cmd
quick_setup.bat
```

This interactive PowerShell script will:
- ✅ Download and install SH-ELF toolchain
- ✅ Configure your development environment
- ✅ Build libsaturn and all examples
- ✅ Install a Saturn emulator (Kronos)
- ✅ Launch your first Saturn homebrew program

**Features:**
- 🎨 Color-coded progress indicators
- 📊 Real-time progress bars
- 🔧 Interactive setup wizard
- 🔄 Resume capability (interrupt and continue later)
- ⚡ Express mode for quick installation

## Manual Setup

**Windows:**
```cmd
build.bat
```

**Linux/macOS:**
```bash
make lib
```

No SGL/SGL overhead. Cycle-accurate, memory-safe, optimized for dual SH-2 architecture.

## Architecture

### Memory Layout
- **0x06000000** (High WRAM): Slave CPU Code & Stack (Fast 32-bit bus)
- **0x06004000** (Low WRAM): Master CPU Code, Stack, Data (Slower 16-bit bus)
- **0x25C00000** (VRAM): VDP1 Command Lists
- **0x20000000** (Uncached): OR this bit to bypass cache

### Dual CPU Strategy
- **Master**: Logic, Input, Sound, Orchestration
- **Slave**: Geometry transformation, T&L, VDP1 Command List generation
- **Sync**: Shared memory with volatile flags + Cache Purging

## Features

- **VDP1**: Sprite/3D polygon rendering
- **VDP2**: Background/tilemap scrolling
- **CD Block**: Raw sector reading for asset loading
- **SCU DMA**: High-speed memory transfers
- **SCU DSP**: Matrix math coprocessor (stub v1.0)
- **16.16 Fixed Point Math**: No FPU, using dmuls.l + xtrct in assembly
- **Dual CPU Parallelism**: Master/Slave synchronization primitives

## Building

### Windows
```cmd
build.bat        # Build libsaturn.a
clean.bat        # Clean build artifacts
```

See [WINDOWS_SETUP.md](WINDOWS_SETUP.md) for detailed Windows setup instructions.

### Linux/macOS
```bash
make lib        # Build libsaturn.a
make examples   # Build all examples
make clean      # Clean build artifacts
```

## Project Structure

```
libsaturn/
├── include/saturn/    # Public headers
├── src/               # Implementation
│   ├── cd/           # CD block driver
│   ├── dma/          # SCU DMA controller
│   ├── dsp/          # SCU DSP (stub)
│   ├── dualcpu/      # Master/Slave sync
│   ├── math/         # Fixed-point math
│   ├── vdp1/         # 3D rendering
│   └── vdp2/         # Backgrounds
├── examples/         # 14 demo programs
└── tools/           # Asset conversion tools
```

## Usage Example

```c
#include "saturn/shared.h"
#include "saturn/vdp1.h"
#include "saturn/dualcpu.h"

SharedData shared;

void _main(void) {
    system_init();
    vdp1_init();
    dualcpu_init();
    dualcpu_start_slave();
    
    while (1) {
        shared.input.x = ...;
        dualcpu_signal_slave();
        dualcpu_wait_for_slave();
        vdp1_start_frame();
    }
}
```

## Setup Script Features

### Interactive Menu System
```
╔════════════════════════════════════════════════════════════════════════════╗
║   🔮       SATURN DEVELOPMENT SETUP       libsaturn v1.0.0              ║
╚════════════════════════════════════════════════════════════════════════════╝

[?] Installation Mode
    [1] Express (Recommended)
    [2] Custom Configuration
    [3] Resume Previous Setup

[████████████████████████████] 100% - Installing Toolchain
[████████░░░░░░░░░░░░░░░░░░] 45%  - Building Library
[████████████████████████████] 100% - Complete!
```

### Smart Automation
- **Environment Detection**: Automatically checks for existing installations
- **Resume Capability**: Interrupt and continue setup later
- **Rollback System**: Undoes changes if setup fails
- **Verification**: Validates each installation step
- **Progress Tracking**: Real-time progress bars for long operations

## Tools

### obj2saturn
Convert .OBJ 3D models to Saturn vertex arrays:

```bash
python3 tools/obj2saturn/obj2saturn.py model.obj model.c
```

## License

MIT License - Feel free to use in your Saturn homebrew projects!
