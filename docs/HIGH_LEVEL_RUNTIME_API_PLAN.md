# LibSaturn High-Level Runtime API — Execution Plan

## Purpose

Evolve LibSaturn's native public API before implementing compatibility layers, so SDL2, raylib, Allegro, and future source-port adapters can remain thin semantic translations instead of becoming repositories of Saturn-specific workarounds.

The target is **not** to reshape LibSaturn into SDL or raylib. The target is to provide reusable, Saturn-native runtime primitives that are useful to ordinary LibSaturn games and also cover the common needs of portable C game APIs.

The guiding rule is:

> **LibSaturn owns Saturn complexity. Compatibility layers translate API semantics; they must not own VDP1/VDP2/SCSP/SMPC/CD workarounds.**

The intended end state is:

```text
                         Game / engine
                              |
             +----------------+----------------+
             |                |                |
      Native LibSaturn      SDL2 shim      raylib shim
             |                |                |
             |          thin translation  thin translation
             |                |                |
             +----------------+----------------+
                              |
                    LibSaturn runtime
            memory / assets / filesystem / time
           surfaces / textures / render state / text
                events / audio / 2D / 3D facade
                              |
            +-----------------+------------------+
            |                 |                  |
           VDP1              VDP2          SCSP / SMPC / CD
            |                 |                  |
            +-----------------+------------------+
                              |
                         Sega Saturn
```

SDL2 and raylib are **acceptance consumers** of this work, not architectural templates for it.

---

## Harness execution contract

This document is intended to be executed by a coding harness from top to bottom.

Rules:

1. Work directly in `celsowm/libsaturn` on `main`.
2. Inspect the current implementation before changing public contracts.
3. Run relevant host tests after every coherent phase.
4. Build affected examples after every public-API change.
5. Preserve the no-heap runtime model. New high-level facilities must use caller-owned memory, fixed-capacity pools, compile-time capacities, or explicit arenas supplied by the caller.
6. Do not hide `malloc`, C++ dynamic allocation, static constructors, or unbounded containers behind a high-level API.
7. Keep low-level Saturn APIs available. High-level APIs must not remove explicit VDP1/VDP2/SCSP/SMPC access.
8. Do not put SDL or raylib names, types, constants, or compatibility behavior in the LibSaturn core.
9. Hardware limitations belong behind LibSaturn abstractions whenever a generic Saturn-native solution is possible.
10. Hardware-specific APIs must remain visibly hardware-specific (`sat_vdp1_*`, `sat_vdp2_*`, `sat_scsp_*`, etc.).
11. Prefer deterministic capacity failures (`SAT_ERR_CAPACITY`) over implicit allocation or undefined behavior.
12. Preserve source compatibility where practical, but do not retain a leaky abstraction solely to avoid a justified migration.
13. When a breaking migration is necessary, provide a deliberate migration path and update repository-owned consumers in the same coherent phase.
14. Commit coherent, tested phases directly to `main`.
15. Use repository build/run scripts and emulator harnesses according to `AGENTS.md`.
16. Do not begin SDL2 or raylib compatibility-layer implementation until the native acceptance gates in this document pass.
17. Do not implement fake programmable-shader semantics or desktop rendering features merely to make a compatibility table look complete.

---

## Design principles

### 1. High-level and low-level APIs are both first-class

The public surface should conceptually contain:

```text
High-level game-runtime concepts
--------------------------------
memory.h
geometry2d.h
color.h
time.h
input.h
event.h
surface.h
texture.h
render2d.h
font.h / text.h
audio.h
file.h
asset.h
camera3d / model facade where justified

Low-level Saturn concepts
-------------------------
vdp1.h
vdp2.h
scsp.h
smpc.h
cd.h
```

A normal 2D source port should not need to understand CMDSRCA, VDP1 center-origin coordinates, CRAM ownership, texture-width alignment, command selection, SCSP slots, or CD sectors.

A Saturn-specific engine must still be able to use those facilities directly.

### 2. No heap is a feature

Managed resources must not imply unrestricted allocation.

Use one or more of:

- fixed runtime pools with generation-checked handles;
- caller-owned backing memory;
- explicit caller-provided arenas;
- compile-time capacities with documented defaults;
- startup-time preparation of static resources.

Every bounded resource should have a queryable capacity or a deterministic failure path.

### 3. Public logical objects must not expose hardware addresses

A logical texture is not a VDP1 character pattern. A sound is not an SCSP slot. An asset path is not a CD sector.

Hardware-native structures may expose hardware fields in low-level headers. Generic runtime objects must describe game intent.

### 4. Sprite-sheet regions are a native runtime concern

Source rectangles are useful to native LibSaturn games, SDL ports, raylib ports, and custom engines.

The runtime must support drawing a region of a logical texture without requiring callers to manually crop, repack, upload, cache, and track VDP1-compatible subtextures.

### 5. Asset source format and runtime format are different things

A game may logically request:

```text
assets/player.png
fonts/main.ttf
audio/jump.wav
audio/music.ogg
```

but the Saturn build may contain preconverted representations:

```text
indexed/RGB555 texture data
baked glyph atlas + metrics
PCM/ADPCM/streamable audio payload
asset metadata
```

The game-facing logical asset identity should be stable even when the physical CD payload is transformed offline.

### 6. Compatibility layers should be boring

A good SDL2 implementation should mostly map:

```text
SDL_Rect            -> sat_rect_t
SDL_Surface         -> sat_surface_t
SDL_Texture         -> sat_texture_t
SDL_RenderCopy      -> sat_draw_texture
SDL_RenderCopyEx    -> sat_draw_texture + params
SDL_PollEvent       -> sat_event_poll
SDL_GetTicks        -> sat_time_ms
SDL_QueueAudio      -> sat_audio_stream_write
```

A good raylib implementation should mostly map:

```text
Texture2D           -> sat_texture_t
Image               -> sat_surface_t / asset decode result
DrawTexturePro      -> sat_draw_texture
DrawRectangle       -> sat_fill_rect
BeginMode2D         -> sat_render2d state/camera push
EndMode2D           -> sat_render2d state pop
GetFrameTime        -> sat_time delta
IsKeyDown           -> high-level input state
IsGamepadButtonDown -> high-level pad state
DrawTextEx          -> sat_text_draw
LoadSound           -> sat_sound_load / asset load
PlaySound           -> sat_sound_play
LoadMusicStream     -> sat_music_open
UpdateMusicStream   -> sat_music_update / streaming API
LoadTexture(path)   -> logical asset/VFS + texture creation
```

If either compatibility layer must understand VDP1 texture stride, CRAM, VRAM allocation, SCSP slots, SMPC register formats, or CD block details, this plan is incomplete.

---

## Definition of done

The runtime foundation is ready for compatibility layers only when native LibSaturn acceptance examples can perform ordinary game-runtime work without direct hardware APIs in gameplay code.

Required capabilities:

- initialize and shut down cleanly;
- deterministic caller-supplied arenas and fixed pools;
- millisecond/sub-frame timing;
- at least two controller ports;
- polling and event-style input;
- CPU-side pixel surfaces with pitch and pixel-format conversion;
- logical texture creation/update/destruction;
- full-texture and sprite-sheet-region drawing;
- scaling, flip, configurable rotation center;
- tint/color modulation where hardware permits;
- documented blend modes and capability failures;
- 2D camera/transform state;
- clipping/scissor where a correct implementation is practical;
- filled rectangles and lines;
- dynamic texture-region updates;
- explicit sprite-region prewarming;
- deterministic region-cache exhaustion;
- high-level baked-font/text drawing;
- PCM sound playback and streaming music/audio;
- logical asset loading by path;
- streamable file/data reads;
- a small high-level 3D camera/model facade sufficient for common raylib-style model code where current LibSaturn 3D facilities already support the semantics;
- no runtime heap allocation;
- all existing examples building after migrations.

Shaders, arbitrary render-to-texture, desktop windowing, and unrestricted desktop filesystem semantics are **not** required for readiness.

---

# Phase 0 — Baseline and repository audit

Inspect at minimum:

```text
include/saturn/core.h
include/saturn/color.h
include/saturn/video.h
include/saturn/input.h
include/saturn/vdp1.h
include/saturn/app.h
include/saturn/font.h
include/saturn/math3d.h
include/saturn/mesh3d.h
include/saturn/model3d.h
include/saturn/render3d.h
include/saturn/saturn.h
src/core/
src/hal/
tests/host/
examples/
tools/
Makefile
build-example.ps1
run-example.ps1
harness/
```

Also inspect the vendored Saturn hardware documentation before encoding VDP1, VDP2, SCSP, SMPC, CRAM, VRAM, timing, controller, or CD limits.

Record:

- current VDP1 texture-VRAM allocation strategy;
- whether texture storage is monotonic;
- legal indexed/direct-color texture formats and dimensions;
- command-list limits;
- CRAM ownership policy;
- frame/VBlank/timer behavior;
- controller-port capabilities;
- current font implementation and atlas ownership;
- current 3D camera/model abstractions;
- existing audio code, if any;
- existing CD/filesystem helpers, if any;
- asset converter architecture;
- host-test coverage for drawing/upload/conversion.

Baseline gate:

```text
make test
```

Build the standard examples target and run the normal harness suite where available. Do not proceed from a red baseline without identifying whether the failure predates this plan.

---

# Phase 1 — Runtime foundations: geometry, color, memory, time

## 1.1 Geometry

Introduce game-facing screen-space value types independent of VDP1 center-origin coordinates:

```c
typedef struct sat_point {
    int16_t x;
    int16_t y;
} sat_point_t;

typedef struct sat_rect {
    int16_t x;
    int16_t y;
    uint16_t width;
    uint16_t height;
} sat_rect_t;
```

Add host-testable intersection, clipping, bounds, and safe arithmetic helpers where useful.

## 1.2 Color and pixel formats

Provide a generic color value and source-pixel-format vocabulary distinct from VDP1 command encodings.

Conceptually:

```c
typedef struct sat_color {
    uint8_t r, g, b, a;
} sat_color_t;

typedef enum sat_pixel_format {
    SAT_PIXEL_INDEX8,
    SAT_PIXEL_RGB555,
    SAT_PIXEL_ARGB1555,
    SAT_PIXEL_RGB565,
    SAT_PIXEL_RGBA8888
} sat_pixel_format_t;
```

The exact supported subset must follow verified hardware/conversion capability. Unsupported conversion must return `SAT_ERR_UNSUPPORTED`, never reinterpret bytes silently.

## 1.3 Arena and pool primitives

Add small deterministic memory utilities useful both to native games and compatibility layers:

```c
typedef struct sat_arena sat_arena_t;
typedef struct sat_pool sat_pool_t;

sat_result_t sat_arena_init(...);
void* sat_arena_alloc(...);
void sat_arena_reset(...);

sat_result_t sat_pool_init(...);
sat_result_t sat_pool_acquire(...);
sat_result_t sat_pool_release(...);
```

Requirements:

- caller supplies backing memory;
- explicit alignment;
- no hidden heap fallback;
- overflow/exhaustion is deterministic;
- optional high-water diagnostics;
- host tests for alignment, exhaustion, reset, generation safety where handles are used.

Compatibility layers may use their own arenas for objects that desktop APIs traditionally allocate dynamically.

## 1.4 Time API

Promote the existing free-running-timer knowledge behind frame counting into a reusable time facility:

```c
uint32_t sat_time_ms(void);
uint64_t sat_time_us(void);   /* only if honest about actual resolution */
sat_result_t sat_delay_ms(uint32_t ms);
```

Requirements:

- time advances while game code is busy;
- no duplicate calibration subsystem;
- wrap behavior documented;
- `sat_frame_count()` remains supported;
- host tests cover wrap-safe deltas.

Phase gate: host tests green, existing examples build, no heap introduced.

---

# Phase 2 — CPU-side surfaces

Add `include/saturn/surface.h` and host-testable core logic.

A surface is a non-owning or explicitly caller-backed pixel view:

```c
typedef struct sat_surface {
    void* pixels;
    uint16_t width;
    uint16_t height;
    uint16_t pitch;
    sat_pixel_format_t format;
    const uint16_t* palette_rgb555;
    uint16_t palette_count;
} sat_surface_t;
```

Target operations:

```text
sat_surface_init
sat_surface_subview
sat_surface_fill
sat_surface_blit
sat_surface_blit_scaled       if a bounded implementation is justified
sat_surface_convert
sat_surface_get_pixel
sat_surface_set_pixel
```

Rules:

- caller owns backing bytes;
- pitch is always honored;
- palette ownership is explicit;
- overlapping blits have defined behavior;
- integer overflow is rejected;
- conversion stays host-testable;
- no dependency on SDL/raylib structures.

Phase gate: conversions, pitch, subviews, clipping, overlap behavior, and invalid arguments covered by tests.

---

# Phase 3 — Split logical textures from native VDP1 textures

This is the central graphics migration.

## 3.1 Native texture representation

Move hardware-facing fields such as source address and palette into an explicitly VDP1 type:

```c
typedef struct sat_vdp1_texture {
    uint16_t srca;
    uint16_t width;
    uint16_t height;
    uint16_t palette;
    uint16_t valid;
    uint16_t reserved;
} sat_vdp1_texture_t;
```

Low-level operations remain available to Saturn-native engines.

## 3.2 Logical texture handle

Introduce a high-level fixed-table handle, conceptually:

```c
typedef struct sat_texture {
    uint16_t slot;
    uint16_t generation;
} sat_texture_t;
```

Internal metadata may include dimensions, source format, usage policy, backing reference, native representation, region records, dirty state, and generation.

The public handle must never expose VRAM addresses.

## 3.3 Explicit backing policies

Support policies equivalent to:

```text
UPLOAD_ONLY
PERSISTENT_SOURCE
DYNAMIC
```

`PERSISTENT_SOURCE` is a non-owning caller-memory reference; do not secretly duplicate full source textures in work RAM.

## 3.4 High-level texture lifecycle

Target operations:

```text
sat_texture_create_from_surface
sat_texture_update
sat_texture_update_rect
sat_texture_info
sat_texture_destroy
sat_texture_prepare_region
sat_texture_forget_region      optional
sat_texture_region_stats       optional diagnostics
```

Return deterministic errors for invalid dimensions, unsupported source format, logical-slot exhaustion, CRAM exhaustion, and VRAM exhaustion.

## 3.5 Migration

Do not retain two unrelated meanings of `sat_texture_t`.

Preferred order:

1. introduce `sat_vdp1_texture_t`;
2. migrate low-level repository call sites;
3. introduce logical `sat_texture_t`;
4. migrate high-level mesh/model/render code according to actual ownership needs;
5. document external migration clearly.

Phase gate: stale-handle tests, pool exhaustion, migrated examples, exact low-level VDP1 command tests, no hardware addresses in high-level headers.

---

# Phase 4 — High-level 2D rendering, render state, and Camera2D

Add or expand `include/saturn/render2d.h`.

### Implementation status — 2026-09-17

Implemented on `main`:

- logical `sat_draw_texture` with full-texture and cached/prepared source-region resolution;
- destination scaling, X/Y flip, explicit rotation center, and rotation lowered to VDP1 distorted sprites;
- fixed-capacity render-state push/pop with deterministic overflow/underflow;
- `sat_camera2d_t` target/offset/rotation/zoom applied consistently to high-level texture and rectangle draws;
- `sat_fill_rect` and `sat_draw_rect` using `sat_color_t` and the same Camera2D state;
- inside rectangular scissor through VDP1 user clipping, persisted in render state and re-emitted for each command list;
- host coverage for texture draw geometry, Camera2D/state stack, color conversion, clipping resolution, VDP1 user-clip command encoding, and clip PMOD bits.

Still open before the Phase 4 gate can be declared complete:

- high-level `sat_draw_line`; the current low-level VDP1 API already owns that C symbol and requires a deliberate hardware-API naming migration rather than a parallel ad-hoc name;
- a documented hardware-backed tint subset beyond neutral white;
- a documented hardware-backed blend subset beyond `SAT_BLEND_NONE`;
- decide whether a separate generic transform setter adds value beyond Camera2D before adding public surface area;
- final cross-build/examples gate for the complete Phase 4 surface.


## 4.1 Unified texture drawing

The central API should support source rectangle, destination rectangle, rotation/origin, flip, tint, and blend semantics:

```c
typedef enum sat_flip {
    SAT_FLIP_NONE = 0,
    SAT_FLIP_X = 1,
    SAT_FLIP_Y = 2
} sat_flip_t;

typedef struct sat_draw_params {
    sat_fx16_t rotation;
    sat_point_t center;
    sat_color_t tint;
    uint16_t blend_mode;
    uint16_t flags;
    uint8_t flip;
    uint8_t reserved;
} sat_draw_params_t;

sat_result_t sat_draw_texture(
    sat_texture_t texture,
    const sat_rect_t* src,
    const sat_rect_t* dst,
    const sat_draw_params_t* params
);
```

Semantics:

- `src == NULL` means full texture;
- source coordinates are logical pixels;
- destination coordinates are high-level screen space;
- scaling and flip are supported;
- rotation lowers to distorted sprites where representable;
- rotation center is explicit;
- tint/blend modes have documented capability limits;
- unsupported combinations return `SAT_ERR_UNSUPPORTED` rather than silently rendering differently.

Tint does not have to imply unrestricted RGBA multiplication. Define a useful hardware-backed subset and expose capabilities honestly.

## 4.2 Shape drawing

Provide screen-space primitives:

```text
sat_draw_rect
sat_fill_rect
sat_draw_line
```

## 4.3 Render-state stack

Add bounded state push/pop for portable engines that naturally use begin/end scopes:

```text
sat_render2d_push
sat_render2d_pop
sat_render2d_set_transform
sat_render2d_set_camera
sat_render2d_set_clip
sat_render2d_set_blend
sat_render2d_set_tint      if global tint is useful
```

The stack uses fixed capacity and returns deterministic overflow.

## 4.4 Camera2D

Provide a Saturn-native camera/transform abstraction conceptually similar to:

```c
typedef struct sat_camera2d {
    sat_fx16_t offset_x;
    sat_fx16_t offset_y;
    sat_fx16_t target_x;
    sat_fx16_t target_y;
    sat_fx16_t rotation;
    sat_fx16_t zoom;
} sat_camera2d_t;
```

Do not copy raylib ABI; preserve only the useful game concept.

This lets a raylib adapter map `BeginMode2D`/`EndMode2D` to bounded state push/pop rather than manually transforming every draw call.

## 4.5 Clipping

Investigate high-level rectangular clipping/scissor behavior. Use hardware clipping where correct and practical; otherwise define a bounded high-level clipping implementation. Do not claim arbitrary scissor support if the hardware path cannot preserve semantics.

Phase gate: full texture, source rectangle, scale, flip, rotation, tint subset, blend subset, camera transform, state push/pop, clip behavior, and shape tests.

---

# Phase 5 — Native texture-region resolver and cache

Source-region handling belongs below `sat_draw_texture`.

Resolver architecture:

```text
sat_draw_texture(texture, src, dst)
                |
                v
        texture-region resolver
          /          |           \
 direct/native   prepared hit   materialize
      path            |          packed copy
         \            |            /
          +-----------+-----------+
                      |
                      v
                 VDP1 draw
```

Requirements:

- prove hardware-safe direct paths from VDP1 documentation;
- fixed metadata capacity;
- explicit VRAM budget;
- key includes texture identity/generation and exact source rectangle;
- dynamic updates invalidate overlapping regions;
- destruction invalidates owned regions;
- stale entries cannot resolve to reused texture slots;
- deterministic eviction/capacity policy;
- explicit prewarming via `sat_texture_prepare_region`;
- lazy materialization only when backing policy allows it;
- clear error for unprepared regions without source backing.

Phase gate: cache hits, cross-texture isolation, generation reuse, dynamic invalidation, exhaustion behavior, and prewarming all tested.

---

# Phase 6 — Input ports and event abstraction

### Implementation status — 2026-09-17

Implemented on `main`:

- two physical controller ports with shared polling snapshots;
- bounded 32-event ring buffer with deterministic drop-oldest overflow;
- connection/disconnection, button-down/up, and reserved axis event vocabulary;
- stable event ordering generated from the same samples returned by polling;
- capacity, overflow, invalid-port, I/O, and polling/event consistency tests.

The current Saturn digital-pad HAL does not expose analog axes, so `SAT_EVENT_AXIS`
is a vocabulary value only until an axis-capable device is supported.

The Phase 6 gate is complete.

Extend current `held / pressed / released` polling rather than replacing it.

## 6.1 Multi-port input

Conceptually:

```c
sat_result_t sat_pad_poll_port(uint8_t port, sat_pad_state_t* out_state);
```

Keep convenient port-zero helpers where useful.

## 6.2 Bounded event queue

Support event types such as:

```text
SAT_EVENT_PAD_CONNECTED
SAT_EVENT_PAD_DISCONNECTED
SAT_EVENT_BUTTON_DOWN
SAT_EVENT_BUTTON_UP
SAT_EVENT_AXIS
```

Requirements:

- fixed ring buffer;
- deterministic overflow policy and diagnostics;
- stable event ordering;
- raw polling remains available;
- polling and event views agree.

## 6.3 Portable input vocabulary

Do not add `KEY_RIGHT` or SDL keycodes to core. Instead, define Saturn/game-runtime input identities that adapters can map to keyboard-like APIs.

A raylib shim may map Saturn D-pad/button state onto `IsKeyDown()` and gamepad calls without those desktop key constants leaking into LibSaturn.

Phase gate: two-pad tests, press/release ordering, overflow, connection state, and polling/event consistency.

---

# Phase 7 — High-level fonts and text

### Implementation status — 2026-09-17

Implemented on `main`:

- caller-owned `sat_font_t` views over a logical atlas texture and baked glyph metrics;
- explicit fallback-glyph or `SAT_ERR_NOT_FOUND` missing-glyph policy;
- validated UTF-8 scalar decoding, newline handling, and carriage-return handling;
- fixed-point text scaling, tint/blend forwarding, measurement, and draw APIs;
- explicit `sat_font_prepare` region prewarming through the native texture cache;
- host coverage for glyph lookup, fallback, UTF-8 validation, multiline metrics, and scaling.

Runtime font data remains offline-baked. The glyph table and atlas lifetime remain
the caller's responsibility; no font API allocates or parses TTF/OTF data at runtime.

`examples/hello_world` now exercises the native baked-atlas path end to end:
caller-owned atlas pixels and metrics, logical texture creation, explicit
region prewarming, measurement, and `sat_text_draw`.

The Phase 7 gate is complete. The next planned phase is the high-level audio
streaming layer in Phase 8.

Raylib-style ports commonly expect text to be a basic runtime primitive. LibSaturn already has font support; evolve it rather than building a parallel subsystem.

Target concepts:

```text
sat_font_t
sat_font_info
sat_text_measure
sat_text_draw
sat_text_draw_ex
```

Requirements:

- runtime font data is bounded and Saturn-ready;
- TTF/OTF parsing/rasterization is expected to happen offline for the default pipeline;
- generated asset contains glyph atlas + metrics + mapping;
- no runtime dependency on FreeType-like desktop libraries;
- variable scale/tint behavior is documented according to renderer capability;
- default/built-in font remains possible;
- UTF support level is explicit rather than accidental.

Asset pipeline concept:

```text
TTF / OTF
   |
 host-side bake
   v
atlas + glyph metrics + character map
   |
   v
sat_font_t runtime asset
```

Phase gate: measure/draw consistency, multiple glyph widths, clipping, scale, missing-glyph policy, and baked-font example.

---

# Phase 8 — High-level audio: sounds and streams

### Implementation status — 2026-09-17

Implemented on `main` as the first streaming subphase:

- caller-owned fixed PCM ring buffers with generation-checked stream handles;
- deterministic four-stream capacity and all-or-nothing overrun rejection;
- mono PCM S8/S16 validation, buffered/available queries, pause/resume, flush,
  close, and fill/underrun/overrun diagnostics;
- no runtime heap allocation or hidden backing-buffer fallback;
- host coverage for wraparound, ordering, underrun accounting, slot exhaustion,
  stale handles, rejected writes, and lifecycle operations;
- SH-2 cross-build and Ymir execution of the existing `audio_showcase` example.

This subphase intentionally stops at the CPU-side producer/consumer ring. The
SCSP refill/consumption scheduler and audible streamed-music backend remain open;
the API does not claim playback semantics until that hardware path is measured.

Build a native layer above raw SCSP concepts.

## 8.1 Streaming primitive

Conceptually:

```c
typedef struct sat_audio_spec {
    uint32_t frequency;
    uint16_t buffer_frames;
    uint8_t channels;
    uint8_t format;
} sat_audio_spec_t;
```

Target operations:

```text
sat_audio_stream_open
sat_audio_stream_write
sat_audio_stream_available
sat_audio_stream_pause
sat_audio_stream_flush
sat_audio_stream_close
```

Use caller-owned/fixed ring buffers with tested underrun/overrun semantics.

## 8.2 Short sound abstraction

Add a convenient bounded sound resource/player layer for common game effects:

```text
sat_sound_load / create
sat_sound_play
sat_sound_stop
sat_sound_is_playing
sat_sound_destroy
```

Internally this may map to prepared SCSP playback resources; public game code must not manage SCSP slots.

## 8.3 Music/stream abstraction

Provide a small streaming abstraction useful for raylib-like `LoadMusicStream`/`UpdateMusicStream` patterns without copying raylib lifecycle exactly.

The runtime should support preconverted streamable audio from the asset pipeline. Runtime OGG/MP3 decoding is not a requirement.

Phase gate: PCM stream, short sound playback, stream state, ring-buffer tests, deterministic voice/stream exhaustion, and no heap.

---

# Phase 9 — Filesystem, logical assets, and VFS

This phase is critical for real source ports.

## 9.1 Generic file I/O

Target handle-based API:

```text
sat_file_open
sat_file_read
sat_file_seek
sat_file_tell
sat_file_size
sat_file_close
```

Requirements:

- fixed handle table or caller-owned object;
- read-only initially is acceptable;
- no implicit whole-file allocation;
- streaming reads;
- documented path normalization/case behavior;
- backend-specific CD APIs stay separate.

## 9.2 Logical asset paths

Add an asset lookup layer so source-port code can continue referring to logical source names even when build tools have transcoded the payload.

Example source code:

```c
sat_texture_load("assets/player.png", &texture);
sat_sound_load("audio/jump.wav", &sound);
sat_font_load("fonts/main.ttf", &font);
```

Possible Saturn build representation:

```text
assets/player.png  -> baked indexed texture entry
audio/jump.wav     -> prepared PCM entry
fonts/main.ttf     -> baked font atlas entry
```

The logical key remains stable; the physical representation does not need to preserve the original desktop file format.

## 9.3 Generated asset registry / manifest

Extend the host asset pipeline with a deterministic generated registry containing at least:

```text
logical path
asset kind
physical CD path or embedded symbol
format metadata
size / dimensions / stream metadata
optional hash/version
```

Lookup must be bounded and deterministic. Do not require a runtime dynamic hash table unless a fixed implementation is explicitly provisioned.

## 9.4 Explicit raw-file escape hatch

Native games and ports must still be able to open raw data files that are not typed assets. `sat_asset_*` complements `sat_file_*`; it does not replace it.

Phase gate:

- load at least texture, font, sound, and generic data by logical path;
- verify asset lookup after offline conversion;
- partial/streaming reads work;
- handle/registry exhaustion is deterministic;
- missing asset and I/O failures are distinguishable.

---

# Phase 10 — High-level 3D facade for portable game code

LibSaturn already has `math3d`, `mesh3d`, `model3d`, `render3d`, and animation facilities. Do not rewrite them to imitate raylib.

Instead, identify the smallest additional high-level facade needed so ordinary model-rendering code does not need backend details.

Candidate concepts:

```text
sat_camera3d_t
sat_render3d_begin(camera)
sat_model_draw(model, transform, params)
sat_render3d_end()
sat_draw_billboard(...)
```

Goals:

- convenient camera setup;
- reusable model transform structure;
- simple model/material draw parameters;
- billboard support where it maps naturally to the current renderer;
- current detailed mesh/model APIs remain first-class;
- skeletal animation support reuses existing LibSaturn facilities rather than adding raylib-specific state.

Do **not** promise raylib `rlgl`, programmable shaders, arbitrary render targets, or OpenGL-like state.

Phase gate: a native example can draw a model using camera + model facade without direct VDP1 command knowledge; existing 3D examples/regressions remain green.

---

# Phase 11 — Native runtime acceptance examples

## 11.1 `runtime_2d`

Create or evolve a native example demonstrating:

1. sprite sheet with at least four frames;
2. source rectangles;
3. startup region prewarming;
4. scaling and flipping;
5. rotation;
6. tint/blend supported subset;
7. Camera2D/state push-pop;
8. clipping;
9. shapes;
10. controller input and events;
11. millisecond-based animation;
12. dynamic texture update;
13. baked-font text;
14. short sound effect;
15. streamed audio/music;
16. texture/font/sound/data loading by logical asset path;
17. capacity/error diagnostics.

The gameplay code must not perform VDP1 source-address math, CRAM management, sprite-sheet repacking, SCSP slot handling, or CD-sector reads.

## 11.2 `runtime_3d`

Add or adapt a small acceptance example proving:

- high-level camera setup;
- model load/use through the asset/runtime path;
- model transform and drawing;
- billboard if implemented;
- controller-driven camera;
- no direct hardware command construction in gameplay code.

Run/build gate:

- host tests green;
- all examples compile;
- acceptance examples build to ISO/CUE;
- run through the normal emulator path;
- verify relevant functionality in harness/emulator.

---

# Phase 12 — Public API cleanup and documentation

Review naming, ownership, and layering across public headers.

Target conceptual organization:

```text
include/saturn/
    core.h
    memory.h
    color.h
    geometry2d.h
    time.h
    input.h
    event.h
    surface.h
    texture.h
    render2d.h
    font.h
    audio.h
    file.h
    asset.h

    vdp1.h
    vdp2.h
    scsp.h
    smpc.h

    math3d.h
    mesh3d.h
    render3d.h
    model3d.h
    anim3d.h
    ...
```

Do not create a header merely to match this tree if an existing responsibility boundary is cleaner.

Document for every resource type:

- ownership;
- lifetime;
- caller-memory references;
- capacity;
- update/invalidation rules;
- CPU/thread assumptions;
- hardware limitations visible at the abstraction boundary.

README should explain:

```text
High-level runtime: portable game concepts with Saturn-aware implementation.
Low-level hardware: explicit Saturn control for hardware-specific work.
```

Do not advertise SDL/raylib compatibility until those adapters actually exist.

---

# Phase 13 — SDL2-readiness review

Perform a paper mapping after native phases are green:

| SDL2 concept | Expected LibSaturn mapping |
|---|---|
| `SDL_GetTicks` | `sat_time_ms` |
| `SDL_Surface` | caller-backed `sat_surface_t` |
| `SDL_CreateTextureFromSurface` | logical texture creation |
| `SDL_UpdateTexture` | texture update rect |
| `SDL_RenderCopy` | `sat_draw_texture` |
| `SDL_RenderCopyEx` | `sat_draw_texture` + params |
| `SDL_RenderFillRect` | `sat_fill_rect` |
| `SDL_RenderDrawLine` | `sat_draw_line` |
| `SDL_PollEvent` | `sat_event_poll` mapping |
| controller polling | `sat_pad_poll_port` |
| queued audio | `sat_audio_stream_write` |
| stream/file reads | `sat_file_*` |

Create `docs/SDL2_COMPATIBILITY_LAYER_PLAN.md` only after this review.

If SDL code would need VDP1 coordinate conversion, source repacking, region caching, palette/VRAM allocation, format conversion, raw SMPC state tracking, raw SCSP mechanics, or CD-sector logic, return to the corresponding native phase.

---

# Phase 14 — raylib-readiness review

Perform a separate paper mapping of a realistic raylib subset.

## Tier 1 — Core / 2D

Expected mappings:

| raylib concept | Expected LibSaturn mapping |
|---|---|
| `InitWindow` | LibSaturn init + fixed display config |
| `CloseWindow` | shutdown |
| `WindowShouldClose` | platform-specific exit convention / adapter policy |
| `BeginDrawing` / `EndDrawing` | frame lifecycle |
| `ClearBackground` | high-level clear/backdrop path |
| `GetTime` / `GetFrameTime` | `sat_time_*` + delta |
| `IsKeyDown` / `IsKeyPressed` | adapter mapping over pad state/events |
| gamepad APIs | `sat_pad_poll_port` / events |
| `Image` | `sat_surface_t` or typed asset representation |
| `Texture2D` | `sat_texture_t` |
| `LoadTexture(path)` | `sat_asset`/logical path + texture creation |
| `UpdateTexture` | texture update |
| `DrawTexture*` / `DrawTexturePro` | `sat_draw_texture` |
| shape drawing | high-level shape API |
| `BeginMode2D` / `EndMode2D` | bounded render-state stack + Camera2D |
| scissor mode | high-level clip capability |
| colors/tint | `sat_color_t` + supported modulation |

## Tier 2 — Text / audio / files

| raylib concept | Expected LibSaturn mapping |
|---|---|
| `Font` | `sat_font_t` |
| `LoadFont(path)` | baked font via logical asset path |
| `MeasureText*` | `sat_text_measure` |
| `DrawText*` | high-level text drawing |
| `Sound` | short sound resource |
| `LoadSound(path)` | preconverted sound asset |
| `PlaySound` | sound player |
| `Music` | high-level stream/music resource |
| `LoadMusicStream(path)` | preconverted streaming asset |
| `UpdateMusicStream` | stream maintenance API |
| file data APIs | `sat_file_*` / `sat_asset_*` |

## Tier 3 — 3D

| raylib concept | Expected LibSaturn mapping |
|---|---|
| `Camera3D` | `sat_camera3d_t` or existing camera representation |
| `BeginMode3D` / `EndMode3D` | high-level 3D frame/state facade |
| `Mesh` / `Model` | existing LibSaturn mesh/model runtime |
| `DrawModel*` | `sat_model_draw` facade |
| billboards | high-level billboard draw if implemented |
| model animation subset | existing LibSaturn animation facilities |

## Tier 4 — Explicitly hardware-dependent / limited

These are **not** readiness blockers for the initial raylib adapter:

```text
BeginShaderMode / Shader / GLSL semantics
rlgl compatibility
arbitrary RenderTexture2D semantics
OpenGL-style custom batching
multiple desktop windows
clipboard/cursor/desktop monitor APIs
unrestricted mouse semantics
```

For each unsupported or limited feature, the eventual adapter must:

- document the limitation;
- fail clearly where a meaningful fallback does not exist;
- use a native VDP1/VDP2 capability only when semantics are sufficiently close;
- never pretend programmable shaders exist.

Create `docs/RAYLIB_COMPATIBILITY_LAYER_PLAN.md` only after this review passes for Tiers 1 and 2. Tier 3 may be implemented incrementally afterward, but the native 3D facade should already be coherent.

If raylib adapter code needs to implement sprite-sheet repacking, image conversion, Camera2D transform math, font baking, SCSP voice management, asset transcoding, or CD lookup itself, return to the corresponding native phase.

---

## Result-code review

Evaluate whether the result model needs useful distinctions such as:

```text
SAT_ERR_NOT_FOUND
SAT_ERR_IO
SAT_ERR_NOT_READY
SAT_ERR_NO_SOURCE
SAT_ERR_BUSY
```

Do not add codes speculatively. Add them only when callers can act differently and tests cover the distinction.

Compatibility layers must translate meaningful LibSaturn errors without parsing strings.

---

## Capacity and configuration strategy

Managed subsystems must publish their bounded nature.

Candidate capacities:

```text
arena/pool backing sizes
logical texture slots
prepared texture-region metadata
region-cache VRAM budget
render-state stack depth
event queue length
font/glyph resources
sound voices
audio streams and ring buffers
open files
asset registry entries
3D model/runtime handles where managed
```

Prefer central configuration through named compile-time definitions or an initialization config object rather than unrelated magic numbers.

Where practical expose diagnostics:

```text
used
capacity
high-water mark
overflow count
```

These are especially valuable when adapting desktop-oriented games to console budgets.

---

## Testing strategy

### Host tests

Keep pure logic host-testable:

- arenas/pools and alignment;
- surface conversion/blits/pitch;
- geometry/clipping;
- texture handle generations;
- region cache lookup/eviction/invalidation;
- draw transforms and rotated quads;
- Camera2D transforms/state stack;
- tint/blend capability selection;
- event synthesis/queue behavior;
- timer wrap/deltas;
- font metrics/layout;
- audio ring buffers;
- file-handle logic;
- asset registry/path normalization;
- 3D facade transform conversion where pure.

### Hardware/HAL-facing tests

Verify:

- texture upload and CRAM allocation;
- VDP1 source addresses/dimensions;
- scaled/distorted command fields;
- verified direct source-region paths;
- clipping/tint/blend hardware paths;
- cache uploads;
- font atlas drawing;
- SCSP sound/stream playback;
- controller ports;
- CD/file/asset reads;
- high-level 3D facade lowering.

### Regression gates

At every coherent public API phase:

```text
make test
build affected examples
build all examples before phase completion
run relevant emulator/harness acceptance
```

Do not defer repository-wide migration failures until the end.

---

## Explicit non-goals

This plan does **not** require:

- implementing SDL2 or raylib themselves;
- SDL/raylib ABI compatibility in the LibSaturn core;
- desktop-style multiple windows;
- OpenGL/Vulkan emulation;
- programmable shaders;
- full `rlgl` compatibility;
- arbitrary render-to-texture semantics;
- desktop clipboard APIs;
- general-purpose POSIX threading;
- hiding every Saturn hardware limit;
- turning LibSaturn into a generic software renderer;
- runtime parsing of PNG/JPEG/TTF/OGG/MP3 unless separately justified;
- heap allocation.

The purpose is to create strong native primitives, not reproduce a PC operating environment.

---

## Recommended implementation order

```text
Phase 0   baseline / audit
   |
Phase 1   geometry + color + memory + time
   |
Phase 2   caller-backed surfaces
   |
Phase 3   logical texture / native VDP1 split
   |
Phase 4   render2d + render state + Camera2D
   |
Phase 5   source-region resolver/cache
   |
Phase 6   multi-port input + events
   |
Phase 7   high-level fonts/text
   |
Phase 8   sounds + audio streaming
   |
Phase 9   file I/O + logical assets/VFS
   |
Phase 10  high-level 3D facade
   |
Phase 11  native runtime acceptance examples
   |
Phase 12  public API cleanup/docs
   |
   +----> Phase 13 SDL2-readiness review
   |
   +----> Phase 14 raylib-readiness review
              |
              +----> compatibility-layer plans
```

Critical 2D dependency chain:

```text
memory + surface
      -> logical texture
         -> source-region resolver
            -> render2d/state/Camera2D
               -> font/audio/assets
                  -> native acceptance
                     -> SDL2 + raylib adapters
```

Do not start by implementing SDL/raylib calls and then backfill native APIs around them.

---

## Final architectural acceptance rules

### SDL2

`SDL_RenderCopy` should eventually be close to:

```c
int SDL_RenderCopy(
    SDL_Renderer* renderer,
    SDL_Texture* texture,
    const SDL_Rect* src,
    const SDL_Rect* dst)
{
    sat_rect_t sat_src;
    sat_rect_t sat_dst;
    const sat_rect_t* src_ptr = convert_optional_rect(src, &sat_src);
    convert_rect(dst, &sat_dst);

    return translate_result(
        sat_draw_texture(texture->native, src_ptr, &sat_dst, NULL)
    );
}
```

### raylib

`DrawTexturePro` should eventually be close to:

```c
void DrawTexturePro(
    Texture2D texture,
    Rectangle source,
    Rectangle dest,
    Vector2 origin,
    float rotation,
    Color tint)
{
    sat_rect_t src = convert_rect(source);
    sat_rect_t dst = convert_rect(dest);
    sat_draw_params_t params = convert_draw_params(origin, rotation, tint);

    raylib_sat_set_error(
        sat_draw_texture(texture.native, &src, &dst, &params)
    );
}
```

And `LoadTexture("assets/player.png")` should resolve through LibSaturn's logical asset layer rather than teaching the raylib shim how to parse PNG, locate CD sectors, convert pixels, allocate CRAM, or prepare VDP1 textures.

If either real adapter needs to crop source pixels, allocate hardware texture space, manage palettes, convert coordinate origins, synthesize Camera2D transforms, bake fonts, manage SCSP voices, transcode assets, or invalidate region caches itself, the abstraction boundary is still wrong.

That is the gate this plan exists to enforce.
