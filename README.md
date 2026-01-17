# libsaturn

Bare Metal Sega Saturn 3D Game Engine SDK

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

## Tools

### obj2saturn
Convert .OBJ 3D models to Saturn vertex arrays:

```bash
python3 tools/obj2saturn/obj2saturn.py model.obj model.c
```

## License

MIT License - Feel free to use in your Saturn homebrew projects!
