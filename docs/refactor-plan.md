# libsaturn SOLID Refactor Plan

## Problem Statement

The layer between demo code (`examples/`) and the library (`src/` + `include/`) has several design issues:

1. **Single Responsibility violation**: `saturn.h` is a monolithic god-header mixing VDP1 sprites, VDP2 backgrounds, input, video init, and texture management. `saturn.cpp` (426 lines) implements every subsystem interleaved.
2. **Open/Closed violation**: Adding a new subsystem (audio, DMA, CD) requires modifying `saturn.h` and `saturn.cpp`.
3. **Interface Segregation violation**: `hello_world` only needs sprites+input but must include the entire VDP2 NBG0 API.
4. **Dependency Inversion violation**: Examples duplicate boilerplate (init, frame loop, vblank) and call low-level functions directly.
5. **No C/C++ tests**: Only Python tooling tests exist. Pure logic (validation, coordinate math, state transitions) is untested.

The HAL layer (`src/hal/`) is already well separated. The problem is the **facade** layer (`saturn.cpp`/`saturn.h`).

---

## Guiding Principles

- **🚫 NO SGL (Sega Graphics Library) — EVER.** SGL is slow, bloated, and pure C. libsaturn talks directly to hardware registers via its own C++ HAL. No SGL headers, no SGL functions, no SGL patterns. This is non-negotiable.
- **No ABI break**: Exported C symbols stay identical (no renames).
- **No HAL rewrite**: The `src/hal/` layer is untouched.
- **Backward compatible**: `saturn/saturn.h` remains as an umbrella header.
- **Incremental**: Each commit is safe, reviewable, and easy to revert.
- **Host-testable**: Pure logic extracted into helpers that compile with native `g++`.

---

## Step 1 — Split Public Headers

Split `include/saturn/saturn.h` into per-subsystem headers. Keep the original as an umbrella.

### New file layout

```
include/saturn/
  saturn.h        ← umbrella (includes all below)
  core.h          ← sat_result_t, sat_fx16_t, SAT_FX16_ONE, sat_init(), sat_shutdown()
  video.h         ← sat_video_config_t, sat_begin/end_frame(), sat_wait_vblank(),
                    sat_set_clear_color(), sat_vdp2_back_color_set()
  input.h         ← SAT_PAD_* defines, sat_pad_state_t, sat_pad_poll(), sat_pad_held()
  vdp1.h          ← sat_texture_t, sat_sprite_cmd_t, sat_tex_upload_indexed8(),
                    sat_draw_sprite()
  vdp2.h          ← sat_vdp2_nbg0_* structs/enums, palette/VRAM/map APIs,
                    sat_vdp2_wait_vblank_start/end()
```

### `saturn.h` becomes

```c
#ifndef SATURN_SATURN_H
#define SATURN_SATURN_H

/* Backward-compatible umbrella header.
 * New code should prefer narrow subsystem headers. */
#include "saturn/core.h"
#include "saturn/video.h"
#include "saturn/input.h"
#include "saturn/vdp1.h"
#include "saturn/vdp2.h"

#endif
```

**Existing examples keep working without any changes.**

---

## Step 2 — Extract `runtime_state`

Move shared runtime state out of `saturn.cpp` into its own module.

### New files

```
src/core/
  runtime_state.hpp   ← RuntimeState struct, extern g_state, require_initialized()
  runtime_state.cpp   ← g_state definition
```

### Content

```cpp
// runtime_state.hpp
namespace saturn::core {

struct RuntimeState {
    bool initialized;
    sat_video_config_t config;
    sat_pad_state_t pad;
    uint16_t clear_color;
    uint16_t nbg0_map_plane_index;
    uint16_t nbg0_map_width;
    uint16_t nbg0_map_height;
    hal::vdp1::Command command_buffer[internal::kCmdCapacity];
};

extern RuntimeState g_state;
sat_result_t require_initialized();

}  // namespace saturn::core
```

---

## Step 3 — Split `saturn.cpp` into Per-Subsystem API Files

Replace the monolithic `saturn.cpp` with focused files. Each includes **only** the HAL headers it needs.

### New file layout

```
src/core/
  core_api.cpp    ← sat_init(), sat_shutdown()
                    includes: vdp1.hpp, vdp2.hpp, scu.hpp
  video_api.cpp   ← sat_begin_frame(), sat_end_frame(), sat_wait_vblank(),
                    sat_set_clear_color(), sat_vdp2_back_color_set()
                    includes: vdp1.hpp, scu.hpp, vdp2.hpp
  input_api.cpp   ← sat_pad_poll(), sat_pad_held()
                    includes: smpc.hpp only
  vdp1_api.cpp    ← sat_tex_upload_indexed8(), sat_draw_sprite()
                    includes: vdp1.hpp only
  vdp2_api.cpp    ← all sat_vdp2_nbg0_*, palette, VRAM, map functions
                    includes: vdp2.hpp only
```

**Key rule**: Each API `.cpp` includes only the HAL headers it actually uses. `input_api.cpp` must NOT include `vdp1.hpp`.

### Delete

- `src/core/saturn.cpp` (replaced by the five files above + `runtime_state.cpp`)

---

## Step 4 — Extract Pure Logic into `logic.hpp`

Create `src/core/logic.hpp` with pure, side-effect-free helpers that are testable on the host.

### Functions to extract

| Function | Source (current location) |
|---|---|
| `validate_video_config(config)` → `sat_result_t` | from `sat_init()` param checks |
| `compute_pad_state(prev_held, cur_held)` → `sat_pad_state_t` | from `sat_pad_poll()` bitwise logic |
| `resolve_sprite_cmd(cmd)` → `SpriteResolved` or error | from `sat_draw_sprite()` defaulting/validation |
| `validate_nbg0_config(config)` → `sat_result_t` | from `sat_vdp2_nbg0_init()` checks |
| `validate_vdp2_palette_upload(count, offset)` → `sat_result_t` | from `sat_vdp2_palette_upload()` bounds |
| `validate_vdp2_vram_write(offset, words, count)` → `sat_result_t` | from `sat_vdp2_vram_write_words()` bounds |
| `validate_map_region(region, map_w, map_h, stride)` → `sat_result_t` | from `sat_vdp2_nbg0_map_write_region()` |
| `compute_map_base_words(plane_index)` → `uint32_t` | `plane_index << 10` |
| `compute_map_row_offset(plane_index, map_w, x, y)` → `uint32_t` | row address math |

### Pattern: thin exported API

```cpp
extern "C" sat_result_t sat_pad_poll(sat_pad_state_t* out_state) {
    sat_result_t st = require_initialized();
    if (st != SAT_OK) return st;
    if (out_state == nullptr) return SAT_ERR_INVALID_ARG;

    const uint16_t held = hal::smpc::read_digital_pad();
    g_state.pad = compute_pad_state(g_state.pad.held, held);  // pure logic
    *out_state = g_state.pad;
    return SAT_OK;
}
```

---

## Step 5 — Add Host-Run Unit Tests

Tests compile with native `g++` (not SH2 cross-compiler) and test only the pure logic from `logic.hpp`.

### New file layout

```
tests/host/
  test_core_logic.cpp
  test_input_logic.cpp
  test_vdp1_logic.cpp
  test_vdp2_logic.cpp
```

### Test cases to implement

#### Fixed-point math (`internal.hpp`)
- `fx16_to_int(0) == 0`
- `fx16_to_int(1 * SAT_FX16_ONE) == 1`
- `fx16_to_int(-1 * SAT_FX16_ONE) == -1`
- Truncation behavior for fractional values

#### Video config validation
- `nullptr` → `SAT_ERR_INVALID_ARG`
- Unsupported width/height → `SAT_ERR_UNSUPPORTED`
- `ntsc == 0` → `SAT_ERR_UNSUPPORTED`
- Valid `{320, 224, 1, 0}` → `SAT_OK`

#### Pad state transitions
- prev=`0`, held=`A` → `pressed=A, released=0`
- prev=`A`, held=`A` → `pressed=0, released=0`
- prev=`A`, held=`0` → `pressed=0, released=A`
- Multi-button masks

#### Sprite command resolution
- null cmd → `SAT_ERR_INVALID_ARG`
- null/invalid texture → `SAT_ERR_INVALID_ARG`
- width/height default to texture when zero
- palette_override falls back to texture palette when zero

#### VDP2 validation
- invalid `char_size`, `color_mode`, `map_plane_index`
- palette upload within/beyond bounds
- VRAM write null/zero/overflow
- map region zero size, out of bounds, stride < width

#### Map offset math
- `compute_map_base_words(0x0010) == 0x0010 << 10`
- Row offset matches expected addresses

### Test harness

Plain `assert()` macros — no framework. Example:

```cpp
#define TEST(name) static void name()
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { \
    fprintf(stderr, "FAIL %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b); \
    exit(1); } } while(0)
```

---

## Step 6 — Update Makefile

### Parameterize example selection

```make
EXAMPLE ?= mvp_2d_scene

APP_C_SRCS := $(wildcard examples/$(EXAMPLE)/*.c) $(wildcard examples/common/*.c)
APP_OBJS   := $(patsubst %.c,$(BUILD_DIR)/%.o,$(APP_C_SRCS))

ELF := $(BUILD_DIR)/$(EXAMPLE).elf
BIN := $(BUILD_DIR)/$(EXAMPLE).bin
```

### Update LIB_CPP_SRCS

```make
LIB_CPP_SRCS := \
    src/core/runtime_state.cpp \
    src/core/core_api.cpp \
    src/core/video_api.cpp \
    src/core/input_api.cpp \
    src/core/vdp1_api.cpp \
    src/core/vdp2_api.cpp \
    src/hal/vdp1.cpp \
    src/hal/vdp2.cpp \
    src/hal/scu.cpp \
    src/hal/smpc.cpp
```

### Add host test target

```make
HOST_CXX ?= g++
HOST_BUILD_DIR := build-host

test-host:
    @mkdir -p $(HOST_BUILD_DIR)
    $(HOST_CXX) -std=c++20 -Wall -Wextra -Iinclude -I. \
        tests/host/test_core_logic.cpp \
        tests/host/test_input_logic.cpp \
        tests/host/test_vdp1_logic.cpp \
        tests/host/test_vdp2_logic.cpp \
        -o $(HOST_BUILD_DIR)/host_tests
    $(HOST_BUILD_DIR)/host_tests

test: test-host
    $(PYTHON) -m unittest tests/test_asset_converter.py tests/test_gen_ip_bin.py
```

### Add build-all-examples target

```make
EXAMPLES := $(notdir $(wildcard examples/*))

examples-all:
    @for e in $(EXAMPLES); do $(MAKE) EXAMPLE=$$e all; done
```

---

## Step 7 — Add `examples/common/` Helper

Remove init/frame-loop boilerplate from examples.

### New files

```
examples/common/
  demo.h
  demo.c
```

### API

```c
/* demo.h - Shared boilerplate for libsaturn examples */
#include "saturn/saturn.h"

sat_result_t demo_init_default(void);
sat_result_t demo_frame_begin(uint16_t backdrop_rgb555, uint16_t clear_rgb555,
                              sat_pad_state_t* out_pad);
sat_result_t demo_frame_end(void);
```

Implementation uses only the public C API (`sat_init`, `sat_wait_vblank`, `sat_pad_poll`, `sat_vdp2_back_color_set`, `sat_set_clear_color`, `sat_begin_frame`, `sat_end_frame`).

### Migrate `hello_world/main.c` first

Before:
```c
sat_video_config_t cfg = {320, 224, 1, 0};
sat_result_t st = sat_init(&cfg);
if (st != SAT_OK) { while (1) { } }
// ...
while (1) {
    sat_wait_vblank();
    sat_vdp2_back_color_set(0x0010);
    sat_set_clear_color(0x0010);
    sat_begin_frame();
    // draw ...
    sat_end_frame();
}
```

After:
```c
sat_result_t st = demo_init_default();
if (st != SAT_OK) { while (1) { } }
// ...
while (1) {
    sat_pad_state_t pad;
    demo_frame_begin(0x0010, 0x0010, &pad);
    // draw ...
    demo_frame_end();
}
```

---

## Commit Order

| # | Commit | Risk | Breaks anything? |
|---|--------|------|-------------------|
| 1 | Header split + umbrella `saturn.h` | Low | No |
| 2 | Extract `runtime_state.hpp/cpp` | Low | No |
| 3 | Split `saturn.cpp` → `*_api.cpp` files | Medium | No (same symbols) |
| 4 | Extract `logic.hpp` + host tests | Low | No |
| 5 | Parameterize `EXAMPLE` in Makefile + `test-host` | Low | No |
| 6 | Add `examples/common/demo.*` + migrate `hello_world` | Low | No |

---

## When to Go Further

Only revisit with a more complex design if:

- 3+ new subsystems are added (audio, DMA, CD block, save RAM)
- Host-side integration tests need to simulate HAL behavior
- Multiple runtime contexts replace the single global state
- PAL / multi-resolution support is needed
- Example boilerplate evolves into a real reusable app framework
