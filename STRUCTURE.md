# libsaturn Structure Summary

```
libsaturn/
├── include/
│   ├── config.h                    # Build configuration
│   └── saturn/
│       ├── types.h                 # Core types (u8, u16, u32, fix16_t)
│       ├── shared.h                # Shared data structures (SharedData, Vdp1Cmd)
│       ├── fixed.h                 # 16.16 fixed-point math
│       ├── hardware.h              # Hardware register definitions
│       ├── cd.h                    # CD block API
│       ├── dma.h                   # SCU DMA API
│       ├── dsp.h                   # SCU DSP API (stub)
│       ├── dualcpu.h               # Master/Slave sync
│       ├── vdp1.h                  # VDP1 sprite/3D API
│       ├── vdp2.h                  # VDP2 background API
│       ├── peripheral.h             # Input controller API
│       ├── system.h                # System initialization
│       ├── matrix.h                # Matrix operations
│       └── vector.h                # Vector operations
│
├── src/
│   ├── crt0.s                     # Startup code (both CPUs)
│   ├── system.c                    # System initialization
│   ├── cd/
│   │   └── read.c                 # CD sector reading
│   ├── dma/
│   │   └── scu_dma.c              # SCU DMA transfers
│   ├── dsp/
│   │   └── dsp.c                  # SCU DSP (stub with C fallback)
│   ├── dualcpu/
│   │   ├── slave.c                # Slave CPU geometry/T&L
│   │   └── sync.c                 # Master/Slave sync primitives
│   ├── math/
│   │   ├── fixed.s                 # Assembly fixed-point multiply
│   │   ├── matrix.c               # Matrix operations
│   │   └── vector.c               # Vector operations
│   ├── vdp1/
│   │   └── init.c                 # VDP1 initialization
│   ├── vdp2/
│   │   └── init.c                 # VDP2 initialization
│   └── peripheral/
│       └── controller.c            # Controller reading
│
├── lib/
│   └── libsaturn.a                # Compiled static library
│
├── examples/
│   ├── 01_helloworld/             # Clear screen
│   ├── 02_input/                 # Controller reading
│   ├── 03_sprites/               # VDP1 sprites
│   ├── 04_fixedpoint_math/       # Fixed-point demo
│   ├── 05_dualcpu_sync/         # Master/Slave communication
│   ├── 06_matrix_transform/      # 3D matrix operations
│   ├── 07_polygon_render/       # Triangle/quad rendering
│   ├── 08_texture_mapping/       # Textured polygons
│   ├── 09_vdp2_background/      # Scrolling backgrounds
│   ├── 10_3d_cube/             # Rotating 3D cube
│   ├── 11_sound/                # SCSP sound
│   ├── 12_physics/              # Fixed-point physics
│   ├── 13_cd_load/              # Load texture from CD
│   └── 14_scu_dma/             # DMA transfers
│
├── tools/
│   ├── bin2c/
│   │   └── bin2c.py             # Binary to C array converter
│   ├── obj2saturn/
│   │   └── obj2saturn.py        # OBJ to Saturn vertex data
│   └── mkisofs/                  # ISO generation tools
│
├── scripts/
│   └── build.sh                  # Build automation
│
├── docs/
│   ├── MEMORY_MAP.md              # Detailed memory layout
│   ├── DUAL_CPU_GUIDE.md         # Master/Slave programming
│   └── FIXED_POINT_MATH.md       # Fixed-point math guide
│
├── saturn.ld                     # Linker script (memory layout)
├── Makefile                      # Root build file
└── README.md                     # Documentation

```

## Key Files

### Core Headers
- **types.h**: Type definitions (u8, u16, u32, fix16_t, UNCACHED macro)
- **shared.h**: Shared data structures (SharedData, Vdp1Cmd, Vec3, Mat4)
- **hardware.h**: Hardware register definitions (VDP1, VDP2, SCU, CD block)

### Math
- **fixed.s**: Assembly-optimized fixed-point multiply (dmuls.l + xtrct)
- **matrix.c**: 4x4 matrix operations (identity, multiply, rotate, translate, scale)
- **vector.c**: 3D/2D vector operations (add, sub, mul, dot, cross, transform)

### Hardware
- **crt0.s**: Startup code for both CPUs (BSS clear, stack setup)
- **slave.c**: Slave CPU workhorse (geometry transformation, VDP1 command generation)
- **read.c**: CD block driver (sector reading, TOC parsing)
- **scu_dma.c**: DMA transfers (CD→VRAM, CD→WRAM, WRAM↔VRAM)
- **init.c**: VDP1/VDP2 initialization

### Examples
- **01_helloworld**: Basic startup + clear screen
- **10_3d_cube**: Full dual-CPU 3D rendering example

### Tools
- **obj2saturn.py**: Convert .OBJ models to Saturn vertex arrays
- **bin2c.py**: Convert binary files to C arrays

## Build Commands

```bash
make lib        # Build libsaturn.a
make examples   # Build all examples
make clean      # Clean build artifacts
```

## Architecture Highlights

### Memory Layout
- **0x06000000**: High WRAM (Slave CPU code, 32-bit bus)
- **0x06004000**: Low WRAM (Master CPU code/data, 16-bit bus)
- **0x25C00000**: VDP1 VRAM (command lists)
- **0x25E00000**: VDP2 VRAM (backgrounds)

### Dual CPU Strategy
- **Master**: Logic, input, sound, orchestration
- **Slave**: Geometry transformation, T&L, VDP1 command generation
- **Sync**: Shared memory with volatile flags + cache purging

### Fixed-Point Math
- 16.16 format (no FPU on Saturn)
- Assembly-optimized multiply (dmuls.l + xtrct)
- Range: -32768 to 32767, precision: ~0.000015
