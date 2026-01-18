# libsaturn Hello World Example

This is a simple example demonstrating basic usage of the libsaturn library for Sega Saturn development.

## Directory Structure

```
hello_world/
├── main.c       - Example source code
├── Makefile     - Build with make
├── build.bat    - Build on Windows
└── README.md    - This file
```

## Building

### Windows
```cmd
build.bat
```

### Linux/macOS (with make)
```bash
make
```

### Manual
```bash
# Set your toolchain path
export SATURN_TOOLCHAIN=/path/to/sh-elf-gcc/bin

# Compile
$SATURN_TOOLCHAIN/sh-elf-gcc -m2 -mb -O2 -fomit-frame-pointer -nostartfiles \
    -I../include -o hello_world.elf main.c \
    -T../scripts/linker.ld -L../lib -lsaturn -lgcc

# Convert to binary
$SATURN_TOOLCHAIN/sh-elf-objcopy -O binary hello_world.elf hello_world.bin
```

## Running

The output file `hello_world.bin` can be run with a Saturn emulator:

```cmd
kronos.exe hello_world.bin
```

## What It Does

1. **Initializes Saturn hardware** via `system_init()`
2. **Sets up VDP1** (Video Display Processor 1) for graphics
3. **Sets up VDP2** (Video Display Processor 2) for background
4. **Initializes controller input** via `peripheral_init()`
5. **Waits for START button** before exiting

## Example Code Overview

```c
#include <saturn/system.h>
#include <saturn/peripheral.h>
#include <saturn/vdp1.h>
#include <saturn/vdp2.h>

void wait_for_button(void) {
    PadState state;
    peripheral_init();

    while (1) {
        peripheral_read(&state);
        if (state.buttons & BTN_START) break;
        interrupt_wait_vblank();
    }
}

int main(void) {
    system_init();
    vdp1_init();
    vdp2_init();

    while (1) {
        vdp1_start_frame();
        vdp1_clear_screen(0x0000);
        vdp1_end_frame();
        wait_for_button();
        break;
    }

    system_halt();
    return 0;
}
```

## Next Steps

- Try modifying the code to draw graphics instead of just clearing the screen
- Check out more examples in the `examples/` directory
- Read the API documentation in `include/saturn/`

## Requirements

- SH-ELF toolchain (GCC for Sega Saturn)
- libsaturn library (built from `build.bat`)
- Saturn emulator or hardware for testing
