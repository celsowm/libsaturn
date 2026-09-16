# LibSaturn High-Level Runtime API — Execution Plan

## Purpose

Evolve LibSaturn's native public API before implementing SDL compatibility, so SDL2 and other future compatibility layers can remain thin translations instead of becoming repositories of Saturn-specific workarounds.

The target is not to reshape LibSaturn into SDL. The target is to provide reusable, Saturn-native runtime primitives that are useful to ordinary LibSaturn games and also happen to cover the needs of portable game APIs such as SDL, raylib, Allegro, and custom engines.

The guiding rule is:

> **LibSaturn owns Saturn complexity. Compatibility layers translate API semantics; they must not own VDP1/VDP2/SCSP/SMPC workarounds.**

The intended end state is:

```text
                   Game / engine
                        |
          +-------------+-------------+
          |                           |
   Native LibSaturn API          SDL2 compatibility
          |                           |
          |                    thin semantic mapping
          |                           |
          +-------------+-------------+
                        |
              LibSaturn runtime
       surfaces / textures / render2d
       events / time / audio / files
                        |
          +-------------+-------------+
          |             |             |
         VDP1          VDP2          SCSP / SMPC / CD
          |             |             |
          +-------------+-------------+
                        |
                    Sega Saturn
```

SDL is an acceptance consumer of this work, not the architectural template for it.

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
7. Keep low-level Saturn APIs available. A high-level renderer must not remove the ability to submit explicit VDP1/VDP2 operations.
8. Do not put SDL names, SDL types, SDL constants, or SDL-specific behavior in the LibSaturn core.
9. Hardware limitations belong behind LibSaturn abstractions whenever a generic solution is possible.
10. Hardware-specific APIs must remain visibly hardware-specific (`sat_vdp1_*`, `sat_vdp2_*`, `sat_scsp_*`, etc.).
11. Prefer deterministic capacity failures (`SAT_ERR_CAPACITY`) over implicit allocation or undefined behavior.
12. Preserve source compatibility where practical, but do not keep a leaky abstraction solely to avoid a justified migration. When a breaking migration is necessary, provide a deliberate compatibility path and update all repository-owned consumers in the same coherent phase.
13. Commit coherent, tested phases directly to `main`.
14. Use repository build/run scripts and emulator harnesses according to `AGENTS.md`.
15. Do not begin the SDL compatibility implementation until the native acceptance gate at the end of this document passes.

---

## Design principles

### 1. High-level and low-level APIs are both first-class

The public surface should have two layers:

```text
High-level, portable game-runtime concepts
------------------------------------------
surface.h
texture.h
render2d.h
time.h
event.h
audio.h
file.h

Low-level Saturn concepts
-------------------------
vdp1.h
vdp2.h
scsp.h
smpc.h
cd.h / filesystem backends
```

A normal 2D game should not need to understand CMDSRCA, VDP1 center-origin coordinates, CRAM ownership, texture upload alignment, or command selection.

A Saturn-specific engine must still be able to use those facilities directly.

### 2. No heap is a feature, not an implementation accident

Managed resources must not imply dynamic allocation.

Use one or more of:

- fixed runtime pools with generation-checked handles;
- caller-owned backing memory;
- explicit caller-provided arenas;
- compile-time capacities with documented defaults;
- startup-time preparation of static resources.

Every bounded resource should have a queryable capacity or a deterministic `SAT_ERR_CAPACITY` failure path.

### 3. Public logical objects must not expose hardware addresses

A logical texture is not the same thing as a VDP1 character pattern.

Hardware-native structures may expose hardware fields in `vdp1.h`, but generic objects in `texture.h` and `render2d.h` must describe game intent rather than command-table layout.

### 4. Sprite-sheet regions are a native runtime concern

Source rectangles are useful to native LibSaturn games, not only SDL ports.

The native API must support drawing a region of a logical texture without requiring the caller to manually crop, repack, upload, cache, and track VDP1-compatible subtextures.

### 5. Conversion happens at explicit boundaries

CPU pixel storage, logical textures, and hardware-native textures are distinct concepts.

Conversion between them must be testable and have clear ownership/lifetime rules.

### 6. Compatibility layers should be boring

A good eventual SDL implementation should mostly perform operations like:

```text
SDL_Rect            -> sat_rect_t
SDL_Surface         -> sat_surface_t wrapper
SDL_Texture         -> sat_texture_t handle
SDL_RenderCopy      -> sat_draw_texture
SDL_RenderCopyEx    -> sat_draw_texture + params
SDL_PollEvent       -> sat_event_poll mapping
SDL_GetTicks        -> sat_time_ms
SDL_QueueAudio      -> sat_audio_stream_write
```

If an SDL source file must understand VDP1 texture stride, CRAM, SCSP slots, SMPC register formats, or CD block details, this runtime plan is incomplete.

---

## Definition of done

This runtime foundation is ready for an SDL2 compatibility layer only when a native LibSaturn acceptance example can do all of the following without directly using VDP1/VDP2/SCSP/SMPC APIs from game code:

- initialize and shut down cleanly;
- use millisecond/sub-frame timing;
- read at least two controller ports through the high-level input/event API;
- create or wrap CPU-side pixel surfaces without heap allocation;
- convert supported pixel formats;
- upload logical textures;
- draw a full texture;
- draw arbitrary sprite-sheet regions;
- scale a source region to a destination rectangle;
- flip horizontally and vertically;
- rotate around a configurable center where supported by VDP1 distorted sprites;
- draw filled rectangles and lines;
- update at least a rectangular region of a dynamic texture;
- explicitly prewarm sprite-sheet regions;
- handle region-cache capacity failure deterministically;
- stream PCM audio through the high-level audio API;
- read game data through the high-level file API;
- run without runtime heap allocation;
- build all existing examples after migration.

The final acceptance path should conceptually look like:

```text
caller-owned image bytes / generated asset
                 |
                 v
            sat_surface_t
                 |
        conversion if needed
                 |
                 v
            sat_texture_t
                 |
      +----------+-----------+
      |                      |
 full texture             src rect
      |                      |
      |                region resolver
      |               /      |       \
      |          native   prepared   cached
      |             path     region    copy
      +---------------+-------+--------+
                              |
                              v
                      render2d commands
                              |
                              v
                             VDP1
```

---

# Phase 0 — Baseline and repository audit

Before modifying code, inspect at minimum:

```text
include/saturn/core.h
include/saturn/video.h
include/saturn/input.h
include/saturn/vdp1.h
include/saturn/app.h
include/saturn/saturn.h
src/core/
src/hal/
tests/host/
examples/
Makefile
build-example.ps1
run-example.ps1
harness/
```

Also inspect the vendored Saturn hardware documentation before encoding any VDP1, VDP2, SCSP, SMPC, CRAM, VRAM, or timing limits.

Record the current answers to these questions in implementation notes or tests where appropriate:

- how VDP1 texture VRAM is allocated;
- whether texture upload storage is monotonic for the lifetime of the process;
- legal indexed and direct-color texture formats;
- legal VDP1 texture dimensions and alignment;
- current command-list limits;
- current CRAM allocation/ownership policy;
- current frame lifecycle and VBlank behavior;
- how controller ports are currently read;
- what free-running timer state is already maintained by `sat_frame_count()`;
- whether any public or internal audio API already exists;
- current CD/filesystem helpers, if any;
- current host-test coverage for VDP1 upload and drawing.

Baseline gate:

```text
make test
```

Also build the repository's standard examples target and run the normal harness suite when available in the environment.

Do not proceed from a red baseline without first identifying whether the failure predates this plan.

---

# Phase 1 — Common runtime value types and timing

## 1.1 Geometry types

Add a small header for game-facing 2D value types, or place them in the narrowest existing common header if that is cleaner after inspection.

Conceptually:

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

Do not tie these types to VDP1 center-origin coordinates. High-level 2D coordinates use screen-space top-left origin unless an API explicitly documents otherwise.

Add pure host tests for rectangle bounds, clipping helpers, intersection, and safe integer behavior if those helpers are introduced.

## 1.2 Pixel-format enumeration

Introduce a game-facing format enum independent of VDP1 command bits:

```c
typedef enum sat_pixel_format {
    SAT_PIXEL_INDEX8,
    SAT_PIXEL_RGB555,
    SAT_PIXEL_ARGB1555,
    SAT_PIXEL_RGB565,
    SAT_PIXEL_RGBA8888
} sat_pixel_format_t;
```

The exact initially supported set may be reduced after auditing the hardware and current conversion pipeline, but the API must distinguish source CPU formats from native VDP1 storage formats.

Unsupported conversions return `SAT_ERR_UNSUPPORTED`; they must not silently reinterpret bytes.

## 1.3 Time API

Promote the existing free-running-timer knowledge behind `sat_frame_count()` into a reusable time facility.

Target public operations:

```c
uint32_t sat_time_ms(void);
uint64_t sat_time_us(void);
sat_result_t sat_delay_ms(uint32_t ms);
```

If a microsecond API would imply misleading precision on the actual implementation, document the real resolution and choose a name/contract that does not promise more than the hardware provides.

Requirements:

- `sat_time_ms()` must advance while game code is busy, not only at VBlank calls;
- wrap behavior must be documented;
- `sat_frame_count()` must continue to work;
- no duplicate independent timer calibration logic;
- host logic for wrap-safe deltas should be unit-tested.

Phase gate:

- existing video/frame tests pass;
- new timing tests pass;
- existing examples build unchanged or with mechanical include adjustments only.

---

# Phase 2 — CPU-side surfaces

Add `include/saturn/surface.h` and corresponding core logic.

A surface is a non-owning or explicitly caller-backed view of pixels in CPU-visible memory. It must not allocate memory by default.

A suitable shape is conceptually:

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

The final field widths must be chosen after checking maximum useful dimensions and overflow behavior.

Required operations:

```text
sat_surface_init
sat_surface_subview
sat_surface_fill
sat_surface_blit
sat_surface_blit_scaled        (only if a bounded CPU implementation is justified)
sat_surface_convert
sat_surface_get_pixel          (debug/convenience; not necessarily fast path)
sat_surface_set_pixel          (debug/convenience; not necessarily fast path)
```

Rules:

- caller owns the pixel buffer;
- surface destruction is unnecessary for a pure non-owning view;
- overlapping blits must have defined behavior;
- palette ownership must be explicit;
- pitch must be honored; never assume tightly packed rows;
- integer overflow in `pitch * height`, offsets, or conversion loops must be rejected where user input can trigger it;
- conversion code must be host-testable without Saturn hardware.

Do not make VDP1 upload functions depend on `SDL_Surface`-like hidden allocation.

Phase gate:

- host tests cover every supported format conversion;
- host tests cover pitch larger than row width;
- host tests cover subviews and clipping;
- no runtime heap is introduced.

---

# Phase 3 — Split logical textures from native VDP1 textures

This is the central architectural migration.

## 3.1 Introduce an explicitly native VDP1 texture representation

The current hardware-facing texture representation containing fields such as source address/palette belongs to the VDP1 layer.

Introduce an explicitly named type, conceptually:

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

Low-level upload/draw paths may operate on this object.

Naming of existing low-level upload functions should become consistently VDP1-specific when they expose VDP1 storage semantics.

## 3.2 Introduce a logical texture handle

The high-level texture should be a small value handle into a fixed-capacity runtime table, not a heap pointer.

Conceptually:

```c
typedef struct sat_texture {
    uint16_t slot;
    uint16_t generation;
} sat_texture_t;
```

The exact representation is implementation-defined, but stale handles must be detectable if resources can be destroyed and slots reused.

Internal metadata may contain:

```text
width / height
source format
usage flags
optional caller-owned source surface reference
native full-texture representation
prepared region records
dirty/update state
generation / validity
```

The public logical handle must not expose VRAM addresses.

## 3.3 Define explicit source/backing policies

Arbitrary source rectangles sometimes require materializing a packed VDP1-compatible region. Therefore texture creation must make backing lifetime explicit.

Support policies equivalent to:

```text
UPLOAD_ONLY
    Source bytes are needed only during creation/update.
    Full-texture rendering works.
    Arbitrary unprepared regions may be unsupported.

PERSISTENT_SOURCE
    LibSaturn stores a non-owning reference to caller-owned source pixels.
    Caller guarantees the bytes remain valid for the texture lifetime.
    Enables lazy region materialization.

DYNAMIC
    Caller-owned writable backing is expected to change.
    Explicit updates invalidate affected cached regions.
```

Do not secretly duplicate an entire texture in work RAM merely to make region rendering convenient.

## 3.4 High-level texture API

Target operations:

```text
sat_texture_create_from_surface
sat_texture_update
sat_texture_update_rect
sat_texture_info
sat_texture_destroy
sat_texture_prepare_region
sat_texture_forget_region      (if explicit eviction is useful)
sat_texture_region_stats       (debug/capacity visibility, optional)
```

Creation must return deterministic errors for unsupported source format, illegal dimensions, exhausted logical texture slots, CRAM exhaustion, and VRAM exhaustion.

## 3.5 Migration strategy

Because `sat_texture_t` currently represents a hardware-native object, this phase may require a breaking rename.

Do not keep two unrelated meanings of `sat_texture_t` simultaneously.

Preferred migration:

1. introduce `sat_vdp1_texture_t` and convert low-level APIs internally;
2. migrate repository-owned low-level call sites to the explicit native type;
3. introduce the new high-level `sat_texture_t` handle;
4. update higher-level mesh/model code according to whether it actually needs a logical texture or a native VDP1 texture;
5. if necessary, provide a short-lived opt-in compatibility header/macro for external migration, but do not permanently alias a hardware object and a logical object under the same type name.

Document the break clearly in the eventual release notes.

Phase gate:

- all stale-handle tests pass;
- pool exhaustion returns `SAT_ERR_CAPACITY`;
- existing repository examples are migrated and build;
- low-level VDP1 tests still validate exact command data;
- no high-level header exposes texture VRAM addresses.

---

# Phase 4 — High-level 2D renderer

Add `include/saturn/render2d.h`.

The renderer should express game intent and lower to VDP1/VDP2 as appropriate.

## 4.1 Unified texture draw operation

The central call should be conceptually similar to:

```c
typedef enum sat_flip {
    SAT_FLIP_NONE = 0,
    SAT_FLIP_X = 1,
    SAT_FLIP_Y = 2
} sat_flip_t;

typedef struct sat_draw_params {
    sat_fx16_t rotation;
    sat_point_t center;
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

Final ABI details may differ, but preserve these semantics:

- `src == NULL`: entire texture;
- `dst == NULL`: document one useful deterministic behavior or reject it; do not copy SDL semantics blindly;
- destination uses top-left screen coordinates;
- source regions are logical pixel coordinates;
- scaling is supported;
- horizontal/vertical flip is supported;
- rotation lowers to distorted sprites where representable;
- rotation center is explicit;
- unsupported combinations return an error rather than silently rendering differently.

## 4.2 Shape drawing

Provide high-level screen-space primitives for at least:

```text
sat_draw_rect
sat_fill_rect
sat_draw_line
```

These should lower to existing VDP1 line/polygon functionality without requiring callers to convert to native center-origin coordinates.

## 4.3 Preserve low-level draw APIs

Existing explicit native operations remain available under the low-level VDP1 API.

Do not implement `sat_draw_texture()` by duplicating VDP1 command construction in a second unrelated subsystem. Reuse the same tested command-building logic beneath both layers.

Phase gate:

- full texture rendering works;
- scaling works;
- flip tests cover all combinations;
- rotation tests verify generated distorted-sprite vertices;
- shape drawing tests verify coordinate conversion;
- previous VDP1 APIs remain independently usable.

---

# Phase 5 — Native texture-region resolver and cache

This phase removes the largest source of future SDL-shim hacks.

## 5.1 Region resolution belongs below `sat_draw_texture`

When `src` selects only part of a logical texture, the high-level renderer asks the texture system for a native representation of that region.

The resolver chooses the cheapest correct path available.

Conceptually:

```text
sat_draw_texture(texture, src, dst)
                |
                v
        texture-region resolver
          /          |           \
         /           |            \
 direct/native   prepared hit   materialize
      path            |          packed copy
         \            |            /
          +-----------+-----------+
                      |
                      v
                 VDP1 draw
```

## 5.2 Exploit hardware-safe direct paths first

Before copying pixels, investigate which subregions can be represented by changing native source address/dimensions without violating VDP1's actual memory traversal rules.

Do not infer this from intuition. Prove legal cases from the VDP1 documentation and test them.

Examples may include some aligned/full-row or vertical-offset cases, but the implementation must use only verified cases.

## 5.3 Fixed-capacity prepared-region cache

For regions that require packed copies, use a bounded cache.

Requirements:

- no heap;
- fixed metadata capacity;
- VRAM usage accounted explicitly;
- key includes texture identity/generation and exact source rectangle;
- dynamic texture updates invalidate overlapping cached regions;
- texture destruction invalidates all owned regions;
- stale entries cannot resolve to a reused texture slot;
- capacity behavior is deterministic.

A simple fixed-size LRU or clock policy is acceptable if tests prove it and costs remain bounded.

## 5.4 Explicit prewarming

Games must be able to eliminate first-use work during gameplay:

```c
sat_texture_prepare_region(texture, &idle0);
sat_texture_prepare_region(texture, &run0);
sat_texture_prepare_region(texture, &run1);
sat_texture_prepare_region(texture, &jump0);
```

This is especially important for sprite sheets and makes the same infrastructure useful to native games.

## 5.5 Lazy behavior

A persistent-source texture may allow `sat_draw_texture()` to materialize a missing region lazily.

The lazy path must be documented as potentially doing copy/upload work.

An upload-only texture with an unprepared region that cannot use a direct hardware path must return a clear error, not draw garbage.

If a new error code is needed (for example `SAT_ERR_NOT_READY` or `SAT_ERR_NO_SOURCE`), add it deliberately and update error tests.

Phase gate:

- repeated region draws hit cache after first preparation;
- two textures with identical rectangle coordinates never alias incorrectly;
- generation reuse is safe;
- dynamic updates invalidate affected regions only where practical;
- cache exhaustion has a deterministic tested outcome;
- prewarming avoids lazy materialization during the acceptance animation loop.

---

# Phase 6 — Input ports and event abstraction

The current `held / pressed / released` pad state is a good low-level/high-level polling primitive. Extend it rather than replacing it.

## 6.1 Multi-port polling

Add an explicit controller index/port API, conceptually:

```c
sat_result_t sat_pad_poll_port(uint8_t port, sat_pad_state_t* out_state);
```

Keep the current single-pad helper as a convenience mapping to port zero if preserving it is useful.

Do not hard-code future event infrastructure to a single controller.

## 6.2 Event queue

Add a bounded event system backed by a fixed ring buffer.

Conceptual event types:

```text
SAT_EVENT_NONE
SAT_EVENT_PAD_CONNECTED
SAT_EVENT_PAD_DISCONNECTED
SAT_EVENT_BUTTON_DOWN
SAT_EVENT_BUTTON_UP
SAT_EVENT_AXIS
SAT_EVENT_QUIT_REQUEST      only if LibSaturn itself has a meaningful source
```

A pad state transition should be expressible both as polling state and events.

Requirements:

- no allocation;
- deterministic overflow policy;
- overflow state/query must be visible for debugging;
- multiple events from one hardware poll retain stable ordering;
- raw polling remains available for latency-sensitive games.

This gives future compatibility layers a clean source for keyboard/controller-style event synthesis.

Phase gate:

- two-controller host tests;
- press/release ordering tests;
- event queue overflow test;
- polling and event views agree on state transitions.

---

# Phase 7 — High-level audio streaming API

Before SDL audio support, LibSaturn needs a native audio abstraction above raw SCSP details.

Audit existing audio work first; do not build a duplicate subsystem if suitable primitives already exist.

Target concepts:

```c
typedef enum sat_audio_format {
    SAT_AUDIO_S8,
    SAT_AUDIO_S16
} sat_audio_format_t;

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

Architecture:

```text
Game / decoder
      |
      v
sat_audio_stream_write
      |
 bounded ring buffer
      |
 SCSP transfer / playback backend
      |
     SCSP
```

Requirements:

- fixed/caller-owned ring buffer;
- underrun and overrun behavior documented;
- supported sample rates/formats explicit;
- conversion/resampling either belongs in a separate reusable helper or returns unsupported; do not hide an expensive unbounded resampler;
- mono/stereo behavior explicit;
- stream state queryable;
- host tests cover ring-buffer arithmetic independently of hardware.

The first acceptance implementation only needs the smallest robust PCM streaming subset that can later back queued SDL audio.

Phase gate:

- continuous PCM playback in an example or harness fixture;
- no underrun under the documented normal producer cadence;
- ring-buffer host tests pass;
- no heap allocation.

---

# Phase 8 — File/data I/O abstraction

Portable game code should not need to know whether bytes came from ISO9660, CD sectors, RAM, or another LibSaturn backend.

Audit existing CD/filesystem code first.

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

- bounded fixed handle table or caller-owned file objects;
- explicit read-only support is acceptable initially;
- seek semantics and error cases documented;
- paths have a documented normalization/case policy;
- no implicit whole-file allocation;
- streaming reads are supported;
- backend-specific APIs remain available separately.

This abstraction is useful for native asset/data streaming and later maps naturally to SDL's stream/I/O concepts without importing SDL semantics into core.

Phase gate:

- read a repository-owned acceptance data file from the generated ISO;
- seek and partial reads tested;
- handle exhaustion returns `SAT_ERR_CAPACITY`;
- invalid path/file errors are distinguishable from invalid arguments if the result-code system is extended.

---

# Phase 9 — Native 2D runtime acceptance example

Create a native example specifically to prove the high-level API before any SDL code exists.

A suitable name is:

```text
examples/runtime_2d
```

The example must use high-level runtime headers only for normal game operations. Hardware-specific headers may be used by library internals but not by the example's gameplay/render/audio code.

The example should demonstrate:

1. one sprite sheet with at least four animation frames;
2. drawing via source rectangles;
3. explicit region prewarming during startup;
4. scaling;
5. horizontal flip;
6. rotation using the high-level API;
7. filled rectangle and line drawing;
8. controller input;
9. millisecond-based animation timing rather than one-step-per-rendered-frame logic;
10. a small dynamic texture update or animated CPU-generated surface;
11. streamed PCM audio;
12. reading at least one small data/config asset through `sat_file_*`;
13. on-screen or serial/debug visibility for capacity/errors where practical.

Acceptance gameplay code should look conceptually like this:

```c
sat_surface_t sheet_surface;
sat_texture_t sheet;

sat_surface_init(...);
sat_texture_create_from_surface(&sheet, &sheet_surface, ...);

sat_texture_prepare_region(sheet, &run_frames[0]);
sat_texture_prepare_region(sheet, &run_frames[1]);

while (running) {
    sat_event_t event;
    while (sat_event_poll(&event) == SAT_OK) {
        handle_event(&event);
    }

    update(sat_time_ms());

    sat_begin_frame();
    sat_draw_texture(sheet, &run_frames[frame], &dst, &params);
    sat_end_frame();
}
```

Exact lifecycle calls may differ after implementation, but the example must remain free of VDP1 source-address math, texture repacking, manual CRAM ownership, and direct SCSP streaming details.

Run/build gate:

- host tests green;
- all examples compile;
- `runtime_2d` builds to ISO/CUE;
- run it through the normal emulator path;
- verify animation, source rectangles, flip, rotation, input, audio, dynamic update, and file read.

---

# Phase 10 — Public API cleanup and documentation

After the acceptance example passes, review the complete public header set for naming and ownership consistency.

Target organization:

```text
include/saturn/
    core.h
    color.h
    geometry2d.h       or equivalent common placement
    time.h
    input.h
    event.h
    surface.h
    texture.h
    render2d.h
    audio.h
    file.h

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

Do not create headers solely to match this proposed tree if an existing header has a cleaner responsibility. The important constraint is conceptual separation.

Document for every resource type:

- owner;
- lifetime;
- whether it references caller memory;
- fixed capacity involved;
- update/invalidation rules;
- thread/CPU assumptions if relevant;
- hardware limitations visible at the abstraction boundary.

Add a README section explaining the two-level API model:

```text
High-level runtime: portable game concepts with Saturn-aware implementation.
Low-level hardware: explicit VDP1/VDP2/SCSP/SMPC control for Saturn-specific work.
```

Do not advertise SDL compatibility yet unless the shim actually exists.

---

# Phase 11 — SDL-readiness review

Only after Phases 0-10 are green, perform a paper mapping of the intended SDL2 subset onto native LibSaturn APIs.

The mapping should require no new hardware workaround inside the SDL layer for the basic subset:

| SDL2 concept | Expected LibSaturn mapping |
|---|---|
| `SDL_GetTicks` | `sat_time_ms` |
| `SDL_Surface` | caller-backed `sat_surface_t` wrapper |
| `SDL_CreateTextureFromSurface` | `sat_texture_create_from_surface` |
| `SDL_UpdateTexture` | `sat_texture_update_rect` |
| `SDL_RenderCopy` | `sat_draw_texture` |
| `SDL_RenderCopyEx` | `sat_draw_texture` + draw params |
| `SDL_RenderFillRect` | `sat_fill_rect` |
| `SDL_RenderDrawLine` | `sat_draw_line` |
| `SDL_PollEvent` | `sat_event_poll` + semantic mapping |
| controller polling | `sat_pad_poll_port` |
| queued audio | `sat_audio_stream_write` |
| basic stream/file reads | `sat_file_*` |

Create `docs/SDL2_COMPATIBILITY_LAYER_PLAN.md` only after this review.

If the mapping requires SDL code to implement any of the following, return to the relevant native phase first:

- VDP1 coordinate conversion;
- source-rectangle pixel repacking;
- VDP1 region caching;
- CRAM allocation;
- VRAM allocation;
- texture-format conversion;
- raw SMPC button transition tracking;
- raw SCSP ring-buffer mechanics;
- CD-sector/file streaming details.

Those are LibSaturn responsibilities.

---

## Result-code review

The current result set may become too coarse as the runtime grows.

During implementation, evaluate whether the public error model needs explicit values equivalent to:

```text
SAT_ERR_NOT_FOUND
SAT_ERR_IO
SAT_ERR_NOT_READY
SAT_ERR_NO_SOURCE
SAT_ERR_BUSY
```

Do not add codes speculatively. Add them only when callers can usefully distinguish the condition and tests cover the distinction.

Compatibility layers must be able to translate meaningful LibSaturn failures into their own error mechanisms without parsing strings.

---

## Capacity and configuration strategy

Every new managed subsystem must publish its bounded nature.

Candidate capacities include:

```text
logical texture slots
prepared texture-region metadata entries
region-cache VRAM budget
event queue length
audio stream count
audio ring-buffer capacity
open file handles
```

Prefer central configuration through clearly named compile-time definitions or an initialization config object rather than unrelated magic numbers scattered across implementation files.

Where practical, add read-only diagnostics such as:

```text
used
capacity
high-water mark
overflow count
```

These are especially valuable on Saturn, where a compatibility layer must diagnose why a desktop-oriented game exceeded console budgets.

---

## Testing strategy

### Host tests

Keep pure logic host-testable:

- surface conversion and blits;
- pitch handling;
- rectangle clipping/intersection;
- texture handle generation checks;
- pool exhaustion;
- region cache lookup/eviction/invalidation;
- draw coordinate conversion;
- rotated quad generation;
- event synthesis and queue behavior;
- timer delta/wrap helpers;
- audio ring buffer;
- file handle table logic.

### Hardware/HAL-facing tests

Verify exact Saturn behavior for:

- native texture upload;
- CRAM allocation;
- VDP1 source addresses and dimensions;
- scaled/distorted sprite command fields;
- verified direct-source-region paths;
- cache uploads;
- SCSP streaming;
- controller port reads;
- CD/file backend reads.

### Regression gates

At every coherent public API phase:

```text
make test
build affected examples
build all examples before phase completion
run relevant emulator/harness acceptance
```

Do not defer mass migration failures until the end.

---

## Explicit non-goals for this plan

This plan does **not** require:

- implementing SDL2;
- implementing SDL3;
- API compatibility with SDL naming or ABI;
- desktop-style multiple windows;
- OpenGL or Vulkan emulation;
- desktop clipboard APIs;
- general-purpose POSIX threading;
- hiding every Saturn hardware limit;
- converting LibSaturn into a software renderer;
- runtime parsing of general desktop image formats unless separately justified;
- heap allocation.

The purpose is to create strong native primitives, not to reproduce a PC operating environment.

---

## Recommended implementation order

```text
Phase 0   baseline / audit
   |
Phase 1   geometry + pixel formats + time
   |
Phase 2   caller-backed surfaces
   |
Phase 3   logical texture / native VDP1 texture split
   |
Phase 4   high-level render2d
   |
Phase 5   native source-region resolver/cache
   |
Phase 6   multi-port input + events
   |
Phase 7   audio streaming
   |
Phase 8   file I/O
   |
Phase 9   native runtime_2d acceptance example
   |
Phase 10  public API cleanup/docs
   |
Phase 11  SDL-readiness review
   |
   +----> only now design the SDL2 shim
```

The most important dependency chain is:

```text
surface
   -> logical texture
      -> source-region resolver
         -> render2d
            -> native acceptance
               -> SDL2 compatibility
```

Do not start by implementing SDL calls and then backfill missing native APIs around them. The native API must become coherent first.

---

## Final architectural acceptance rule

Before declaring this plan complete, inspect the future-looking pseudocode for an SDL renderer implementation.

It should be possible for `SDL_RenderCopy` to be essentially this simple:

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

    sat_result_t result = sat_draw_texture(
        texture->native,
        src_ptr,
        &sat_dst,
        NULL
    );

    return translate_result(result);
}
```

If the real implementation instead needs to know how to crop source pixels, allocate VDP1 texture space, upload a temporary character pattern, manage palettes, convert coordinate origins, or invalidate texture-region caches, the abstraction boundary is still wrong.

That is the gate this plan exists to enforce.
