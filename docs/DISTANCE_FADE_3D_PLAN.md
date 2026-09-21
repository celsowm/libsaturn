# 3D Distance Fade Plan

## Goal

Add a Saturn-native 3D distance-fade system to LibSaturn so distant VDP1 geometry can progressively blend into the VDP2 scene instead of appearing or disappearing abruptly at the draw-distance boundary.

The visual target is the class of distance-fade techniques associated with games such as **Sonic R**: geometry remains fully opaque nearby, enters a stepped transparency/blending region in the distance, then is culled after it has become visually negligible.

This is **not** a requirement to reproduce Sonic R's engine bit-for-bit. The feature should expose the underlying Saturn capabilities as a generic LibSaturn abstraction that games can combine with lighting, LoD, culling, and different VDP2 backgrounds.

The intended result is:

```text
camera
  |
  |       normal 3D                         fade zone                 culled
  v
  +-------------------------------+-----------------------------+---------->
                                  |                             |
                               fade_start                    fade_end

  100%  100%  100%  100%        87%  75%  62%  50%  37%  25%  12%   0%
```

The first implementation should prioritize **predictable hardware behavior, zero allocation, low SH-2 overhead, and source compatibility** over attempting to hide every Saturn restriction.

---

## Why this belongs in LibSaturn

The current 3D stack already provides the geometry side of the feature:

- `sat_vdp1_draw_mesh()` is the explicit low-level path for culling, projection,
  painter sorting and VDP1 submission; game code uses `sat_scene_t`.
- `sat_projected_vertex_t::w` already represents view depth in 16.16 units when the projection-cache path is used.
- textured model faces are submitted as VDP1 distorted sprites.
- VDP2 is already used by LibSaturn for NBG/RBG backgrounds.

What is missing is the bridge between VDP1 geometry and the VDP2 **sprite color-calculation** hardware.

The Saturn VDP2 exposes eight sprite color-calculation ratio slots through `CCRSA`..`CCRSD`. Depending on sprite format/type, sprite data can select one of those ratios. This is exactly the kind of hardware facility needed for a stepped distance fade without doing per-pixel work on the SH-2.

LibSaturn currently exposes neither the VDP2 sprite color-calculation ratios nor a public per-part selection mechanism. `include/saturn/vdp1.h` currently has only the basic sprite flags (`SAT_SPRITE_FLAG_OPAQUE`) and palette selection.

Therefore distance fade should be built in layers:

```text
fade3d math
    |
    v
VDP1 per-part color-calc selection
    |
    v
VDP2 sprite color-calc configuration
    |
    v
hardware
```

The high-level fade feature must not directly write VDP2 registers and the native
mesh path must not become responsible for global video configuration.

---

## Design principles

1. **Keep hardware configuration separate from fade policy.**
   The fade math chooses a level; the VDP1/VDP2 layer decides how that level is represented on Saturn.

2. **Do not make LoD and fade the same feature.**
   LoD changes geometry complexity. Distance fade changes how submitted geometry blends into the scene. They should compose cleanly.

3. **Prefer view depth over Euclidean distance.**
   A draw-distance plane should follow the camera plane. The existing projected-vertex `w` value is a natural depth quantity and avoids square roots.

4. **No hidden heap allocation.**
   The feature follows the existing LibSaturn caller-owned/static-storage model.

5. **Avoid breaking existing draw calls.**
   Existing sprite, mesh and model code must retain its current visual result unless the new color-calculation path is explicitly enabled.

6. **Treat Saturn restrictions as API contracts.**
   If a requested mode cannot be represented for a given VDP1 color format, return a clear error or documented fallback rather than silently doing the wrong thing.

7. **Measure command cost and frame cost.**
   A visual effect is not successful if it destroys the command budget or SH-2 frame time.

---

# Phase 0 - Hardware validation spike

Before designing the final public API, create the smallest possible example that proves exactly how the relevant VDP1 and VDP2 fields interact on real Saturn-compatible hardware/emulators.

Suggested example:

```text
examples/vdp1_vdp2_color_calc/
```

The scene should contain:

- a VDP2 background with obvious contrasting colors;
- eight identical VDP1 textured quads;
- each quad selecting a different sprite color-calculation ratio;
- labels showing slots 0..7;
- an opaque reference quad;
- optionally an RGB polygon reference.

The test must answer the following questions with screenshots / emulator validation and host-side register assertions where possible:

- Which current LibSaturn sprite type is configured in `SPCTL`?
- Which `CMDCOLR` bits are available for priority and color-calculation selection under that sprite type?
- Can indexed/color-bank distorted sprites select all eight color-calculation ratio slots cleanly?
- What ratio direction corresponds to "mostly sprite" versus "mostly background"?
- What happens to transparent texel code 0 while VDP2 color calculation is enabled?
- Does `SAT_SPRITE_FLAG_OPAQUE` interact with the selected path?
- What does RGB-coded polygon output select? The Saturn documentation says RGB sprite data selects sprite register 0, so this path must be verified separately rather than assumed equivalent to color-bank textures.
- How does VDP1 Gouraud shading interact with VDP2 sprite color calculation on RGB polygons?
- Does the effect behave identically on the project's supported emulators and target hardware?

No high-level `fade3d` API should be merged until this spike establishes the encoding we can reliably expose.

### Acceptance criteria

- Eight visibly distinct VDP2 blend ratios can be produced for the supported indexed textured-sprite path, or the exact smaller supported set is documented.
- Existing opaque sprite drawing remains unchanged when color calculation is disabled.
- The required VDP1 command bits and VDP2 registers have host-testable pure helper functions.

---

# Phase 1 - Low-level VDP2 sprite color-calculation API

Expose the global VDP2 state required by sprite color calculation.

Proposed public types, subject to the Phase 0 hardware spike:

```c
typedef struct sat_vdp2_sprite_color_calc_config {
    uint8_t enabled;
    uint8_t ratio[8]; /* hardware range 0..31 */
} sat_vdp2_sprite_color_calc_config_t;

sat_result_t sat_vdp2_sprite_color_calc_configure(
    const sat_vdp2_sprite_color_calc_config_t* config
);

sat_result_t sat_vdp2_sprite_color_calc_set_ratio(
    uint8_t slot,
    uint8_t ratio
);

void sat_vdp2_sprite_color_calc_disable(void);
```

The exact shape may change if the hardware validation shows that the enable condition / priority threshold also needs to be part of the public contract. If so, expose those concepts explicitly instead of baking undocumented magic values into the implementation.

### Implementation

Likely files:

```text
include/saturn/vdp2.h
src/core/vdp2_api.cpp
src/hal/vdp2.hpp
src/hal/vdp2.cpp
src/core/logic.hpp
```

The HAL should own register writes for:

- sprite color-calculation enable/condition;
- `CCRSA`;
- `CCRSB`;
- `CCRSC`;
- `CCRSD`.

Pure composition/validation helpers belong in `src/core/logic.hpp` so host tests can verify every register value without Saturn hardware.

### Validation

Reject:

- null configuration;
- slot >= 8;
- ratios outside the hardware range;
- unsupported sprite-control combinations if the API cannot represent them safely.

### Tests

Add host tests for:

- ratio packing for slots 0..7;
- boundary values 0 and 31;
- invalid slot / invalid ratio;
- enable and disable state composition;
- preservation of unrelated VDP2 register bits.

---

# Phase 2 - Per-VDP1-part color-calculation selection

A global table of eight ratios is not enough. Individual VDP1 parts need a way to select a ratio slot.

Do **not** overload `palette_override` with hidden bit packing. Palette/color-bank selection and VDP2 blend-slot selection are different concepts and must remain independently understandable.

The exact public representation should follow the Phase 0 result. Preferred direction: add an explicit small attribute structure that can be shared by normal, scaled and distorted sprite commands.

For example:

```c
typedef struct sat_sprite_attributes {
    uint16_t palette_override;
    uint16_t flags;
    uint8_t priority;
    uint8_t color_calc_slot;
    uint8_t color_calc_enable;
    uint8_t reserved;
} sat_sprite_attributes_t;
```

Then either embed this in new `_ex` commands or append equivalent fields in a source-compatible way.

Possible extended entry points:

```c
sat_result_t sat_draw_sprite_ex(const sat_sprite_cmd_ex_t* cmd);
sat_result_t sat_draw_sprite_scaled_ex(const sat_scaled_sprite_cmd_ex_t* cmd);
sat_result_t sat_draw_sprite_distorted_ex(const sat_distorted_sprite_cmd_ex_t* cmd);
```

The final choice should minimize duplication in `vdp1_api.cpp` and preserve every existing function as a default/no-color-calc wrapper.

### Important format restriction

The implementation must explicitly distinguish:

- indexed/color-bank textured sprites;
- RGB-coded VDP1 polygons;
- Gouraud-shaded RGB polygons.

The Saturn documentation states that RGB sprite data selects sprite color-calculation register 0, so an API promising arbitrary 0..7 selection for every VDP1 primitive would be misleading unless the spike proves an alternative representation.

The first production-ready fade path may therefore support **indexed textured distorted sprites first**, with RGB polygon support added only after its actual hardware behavior is understood.

### Tests

Host-side tests must verify the generated VDP1 command words for every supported slot and must verify that the old API produces byte-for-byte equivalent commands to the pre-feature path.

---

# Phase 3 - Hardware-independent `fade3d` policy

Add a tiny generic module that maps view depth to a discrete fade level.

Suggested files:

```text
include/saturn/fade3d.h
src/core/fade3d_api.cpp
```

Proposed API:

```c
typedef struct sat_fade3d {
    sat_fx16_t start;
    sat_fx16_t end;
    uint8_t levels;
    uint8_t flags;
    uint16_t reserved;
} sat_fade3d_t;

#define SAT_FADE3D_CULL_AFTER_END 0x01u

typedef struct sat_fade3d_result {
    uint8_t level;
    uint8_t culled;
} sat_fade3d_result_t;

sat_result_t sat_fade3d_eval(
    const sat_fade3d_t* fade,
    sat_fx16_t view_depth,
    sat_fade3d_result_t* out
);
```

Semantics:

```text
depth <= start      -> level 0, fully visible
depth between       -> quantized level 0..levels-1
depth >= end        -> final level or culled
```

For the Saturn VDP2 backend, `levels` should normally be at most 8. The generic math itself does not need to know why.

### Mapping direction

`fade3d` should not hard-code that slot 0 means opaque or transparent. It only returns a normalized/discrete fade level. A separate Saturn mapping translates level -> VDP2 ratio slot.

This avoids coupling generic fade policy to a particular register convention.

### Math requirements

- 16.16 fixed point only;
- no floating point;
- no square root;
- avoid 64-bit division inside a per-face hot loop where possible;
- clamp all boundary cases deterministically;
- `start >= end` is invalid;
- `levels == 0` is invalid.

### Tests

Test:

- exact start/end boundaries;
- values immediately before and after boundaries;
- each quantization bucket;
- negative/behind-camera depth;
- invalid ranges;
- saturation with large fixed-point values.

---

# Phase 4 - High-level Saturn distance-fade binding

Provide a convenience layer that joins `fade3d` levels to the configured VDP2 sprite color-calculation slots.

Do not make the generic fade object itself Saturn-specific.

Possible API:

```c
typedef struct sat_fade3d_sprite_blend {
    sat_fade3d_t fade;
    uint8_t slot_for_level[8];
    uint8_t level_count;
    uint8_t reserved[3];
} sat_fade3d_sprite_blend_t;

sat_result_t sat_fade3d_sprite_blend_eval(
    const sat_fade3d_sprite_blend_t* config,
    sat_fx16_t depth,
    uint8_t* out_slot,
    int* out_culled
);
```

A helper may provide a useful default table:

```c
sat_result_t sat_fade3d_sprite_blend_default(
    sat_fade3d_sprite_blend_t* out,
    sat_fx16_t start,
    sat_fx16_t end
);
```

The default VDP2 ratios should be selected empirically from Phase 0 rather than assuming mathematically linear alpha looks visually linear on Saturn output.

A likely starting point is eight perceptually ordered steps from opaque-ish to nearly background-only.

---

# Phase 5 - `mesh3d` integration

The first mesh integration should be deliberately simple: **one fade level per mesh draw**, not one independently changing level per polygon.

Reasons:

- stable appearance across a model;
- fewer state variations;
- predictable command encoding;
- lower CPU overhead;
- avoids visible stripes across a large object when adjacent faces fall into different quantization buckets;
- makes later LoD transitions easier to reason about.

Extend `sat_mesh_draw_t` only after the low-level VDP1 representation is stable.

Preferred shape:

```c
typedef struct sat_mesh_draw {
    ... existing fields ...

    /* Optional VDP2 sprite color-calculation selection applied to
     * supported textured faces. Disabled by default. */
    uint8_t color_calc_enable;
    uint8_t color_calc_slot;
    uint16_t reserved_color_calc;
} sat_mesh_draw_t;
```

Existing callers zero-initialize this struct, so the feature remains disabled by default.

Do **not** make the native mesh path configure the global VDP2 ratio table. That belongs to scene/video setup.

### Obtaining depth

The fade level should normally be computed once per object before native mesh lowering.

Preferred helpers:

```c
sat_result_t sat_project_depth(
    const sat_mat4_t* view_proj,
    const sat_vec3_t* point,
    sat_fx16_t* out_depth
);
```

or reuse a projected model center when one is already available.

The chosen quantity must match the semantics already documented for `sat_projected_vertex_t::w`: view depth in 16.16 world units.

Avoid using camera-to-object Euclidean distance for the default fade because it creates a spherical cutoff instead of a camera-aligned distance plane.

### Optional future per-face mode

A future flag may allow per-face depth selection for very large geometry:

```c
SAT_MESH_FADE_PER_FACE
```

This is intentionally not part of the first implementation. It needs profiling and visual validation first.

---

# Phase 6 - Model API convenience

Once mesh integration is stable, add convenience to `model3d` so compiled textured models can use distance fade without game-specific VDP1 plumbing.

Possible helper:

```c
sat_result_t sat_model_bind_draw_faded(
    ... existing bind arguments ...,
    const sat_fade3d_sprite_blend_t* fade,
    sat_fx16_t object_depth,
    sat_mesh_draw_t* out_draw,
    int* out_culled
);
```

However, prefer extending `sat_model_bind_draw_ex()` cleanly or adding a small post-bind helper instead of creating an explosion of bind variants.

A likely cleaner API is:

```c
sat_result_t sat_mesh_draw_apply_distance_fade(
    sat_mesh_draw_t* draw,
    const sat_fade3d_sprite_blend_t* fade,
    sat_fx16_t depth,
    int* out_culled
);
```

Usage:

```c
sat_mesh_draw_t draw = {};
sat_model_bind_draw_ex(..., &draw);

sat_fx16_t depth;
sat_project_depth(&view_proj, &object_center, &depth);

int culled;
sat_mesh_draw_apply_distance_fade(&draw, &fade, depth, &culled);
if (!culled) {
    sat_vdp1_draw_mesh(&mesh, &draw);
}
```

This keeps model binding, fade policy and actual drawing independently testable.

---

# Phase 7 - Gouraud / lighting transition research

Do not pretend that the textured color-bank fade path automatically solves every lighting case.

The project already has:

- flat-shaded RGB polygons;
- Gouraud-shaded RGB polygons;
- indexed textured distorted sprites.

These paths do not have identical VDP1/VDP2 color-calculation behavior.

Create a dedicated investigation for the transition between:

```text
near geometry:
    normal lighting / Gouraud

far geometry:
    VDP2 sprite color-calculation fade
```

The goal is to avoid a visible "lighting pop" at `fade_start`.

Investigate, in order:

1. whether the relevant Gouraud path can coexist with the chosen VDP2 sprite color calculation;
2. whether only color-bank textured faces can use the multi-slot fade;
3. whether a short pre-fade lighting ramp is needed for RGB/Gouraud geometry;
4. whether baked shade palettes can provide a cheap transition for models that already use the model pipeline;
5. whether unsupported combinations should intentionally use a simpler cutoff/fallback.

This phase is where a more Sonic-R-like multi-stage appearance transition can be explored, but it must remain optional and based on measured Saturn behavior.

---

# Phase 8 - Dedicated end-to-end distance-fade example

Create a **new standalone LibSaturn example specifically to test, tune and demonstrate the finished distance-fade feature**. This is a required deliverable, not just documentation or an optional showcase.

The low-level Phase 0 example proves the VDP1/VDP2 register behavior. This second example must exercise the **public high-level API exactly as a game would use it**.

Required example:

```text
examples/distance_fade_3d/
    main.c
    Makefile.inc        # or the repository's current example-build equivalent
    assets/             # only if the example needs generated model/texture assets
```

The example must be wired into the repository's normal example/ISO build flow so it is easy to build and run without private scripts or manual register patches.

## Test scene

Use a deliberately simple scene whose only purpose is to make distance fade obvious and measurable:

- VDP2 sky/background with a color that clearly shows blending;
- VDP2 ground extending toward the horizon;
- a long row or grid of repeated textured 3D objects placed at increasing view depths;
- at least one imported textured `sat_model_asset_t` or representative textured `sat_mesh_t`, so the test covers the real model path rather than only hand-built quads;
- enough objects beyond `fade_start` that several fade levels are visible simultaneously;
- a final object beyond `fade_end` to prove culling;
- optional world-space markers/gates at `fade_start` and `fade_end` so the thresholds are visually obvious.

A useful layout is:

```text
camera
  |
  v

 [near]      [near]       [fade 1] [fade 2] [fade 3] ... [fade 7]   [culled]
   O            O             O        O        O             o          x
---+------------+-------------|-------------------------------|-----------> Z
                              ^                               ^
                          fade_start                       fade_end
```

The camera should be movable, and there should also be an optional automatic forward/backward motion mode so objects continuously enter and leave the fade zone without requiring perfect manual input.

## Runtime comparison modes

The example must support at least these render modes using the **same scene, camera and cutoff distances**:

```text
MODE 0  no far fade / normal draw
MODE 1  hard cutoff
MODE 2  4-step distance fade + cutoff
MODE 3  8-step distance fade + cutoff
```

The important A/B comparison is:

```text
hard cutoff:
    object suddenly disappears / appears

8-step fade:
    object progressively blends into / emerges from the VDP2 scene
```

Changing mode must not rebuild the scene or move objects; only the fade policy should change.

## Controls

Suggested controls:

```text
D-pad       move camera
L / R       rotate camera or adjust view depth
A           cycle render mode
B           freeze/unfreeze automatic camera motion
C           cycle fade preset/range
X/Y/Z       optional direct 4-step / 8-step / hard-cutoff selection
START       exit
```

Use only buttons that fit the project's current input conventions; the exact mapping may be adjusted during implementation.

## On-screen diagnostics

Render a small debug HUD showing at least:

```text
mode
camera/view depth
fade_start
fade_end
fade level of a selected/reference object
selected VDP2 color-calc slot
visible object count
culled object count
VDP1 command count, if exposed cheaply
frame/display rate
```

If useful, allow selecting one reference object and print:

```text
object depth -> fade level -> VDP2 slot
```

This makes incorrect bucket boundaries immediately visible during emulator and hardware testing.

## Visual validation cases

The example must make it easy to inspect all of these cases:

1. **Entering the fade zone** — no sudden brightness or transparency jump at `fade_start`.
2. **Crossing every step** — each configured ratio is visibly ordered from more object to more background.
3. **Final cutoff** — the last fade level transitions to culling without an obvious pop.
4. **Camera reversal** — approaching an object uses the same levels in reverse, with no hysteresis bug or stale slot.
5. **Multiple objects at once** — objects at different depths can use different color-calc slots in the same frame.
6. **Transparent texture texel 0** — transparent areas remain correct while fading.
7. **Opaque compatibility** — disabling fade returns exactly to the ordinary textured-mesh path.
8. **Sorting** — painter ordering remains correct for the supported test scene while several faded meshes overlap.
9. **Background contrast** — repeat the test against at least two sufficiently different VDP2 background colors/presets if practical, to catch a ratio mapping that only looks correct against one sky color.

## Performance test mode

Include a stress toggle or compile-time preset that places enough repeated meshes to make command/CPU cost measurable.

Compare at minimum:

```text
hard cutoff
4-step fade
8-step fade
```

The fade implementation should not submit duplicate geometry simply to create the transition. For an equal visible-object set, the expected command count should remain essentially the same as the non-faded textured path; the difference should be command attributes and small per-object fade evaluation cost.

## Build and regression requirements

The example must:

- compile with the normal LibSaturn example build path;
- require no direct register writes from `main.c` once the public API is implemented;
- use `sat_fade3d_*`, VDP1/VDP2 public APIs and mesh/model integration rather than calling internal HAL functions;
- be suitable for the modified Ymir harness and real Saturn validation where available;
- stay in the tree after the feature lands as a regression/demo example, not be deleted as a temporary experiment.

Where the repository already performs example compile checks in CI, add `distance_fade_3d` to that set. If examples are not currently part of CI, at minimum document the exact build target alongside the example.

## Acceptance criteria for the example

The example is complete only when:

- [ ] `examples/distance_fade_3d` builds through the normal example build system.
- [ ] A user can visibly compare hard cutoff with 4-step and 8-step fades at runtime.
- [ ] The HUD exposes the active fade level and selected hardware slot for debugging.
- [ ] Objects at multiple depths display different fade levels in the same frame.
- [ ] An object crossing `fade_end` disappears without the abrupt pop visible in hard-cutoff mode.
- [ ] Disabling fade restores the ordinary rendering path without visual regressions.
- [ ] No direct VDP2 register pokes are required from the example.
- [ ] The example has been checked on at least one accurate Saturn emulator; hardware validation should be recorded when available.

---

# Phase 9 - Combine with LoD

Distance fade and LoD should remain separate but complementary.

Recommended scene policy:

```text
near
 |
 | LOD0
 |----------------
 | LOD1
 |----------------
 | LOD2
 |---------------- fade_start
 | LOD2 + VDP2 distance fade
 | .
 | .
 |---------------- fade_end
 | culled
 v
far
```

The first integration should use fade only near the final draw-distance boundary.

Do not initially crossfade two LoD meshes simultaneously. Drawing both meshes during an LoD transition can nearly double VDP1 command cost exactly where the system is supposed to save work.

A later experiment may use the distance fade to mask a discrete LoD switch without drawing both LoDs at once, for example switching geometry at a low-contrast point inside the fade zone.

---

# Phase 10 - Performance and command-budget validation

Distance fade must not accidentally become more expensive than the geometry it hides.

Benchmark at least:

- fade disabled;
- hard cutoff only;
- 4 fade levels;
- 8 fade levels;
- mesh-level fade;
- any experimental per-face fade.

Measure:

- SH-2 frame time / display frames per game frame;
- VDP1 command count;
- VDP1 command-list bytes;
- extra VDP2 register writes per frame;
- visible object count;
- CPU cost of fade-level evaluation.

The expected optimal implementation updates the eight global ratios rarely (scene setup or preset change) and changes only the small per-part selector during normal rendering.

Per-object fade evaluation should be a few fixed-point comparisons/multiplies, not a new matrix-heavy subsystem.

---

# Proposed API ownership

Final module boundaries should look approximately like this:

```text
saturn/fade3d.h
    generic fixed-point distance -> fade-level math

saturn/vdp2.h
    global sprite color-calculation enable + 8 ratio slots

saturn/vdp1.h
    per-part selection of the supported VDP2 color-calc slot

saturn/mesh3d.h
    optional mesh-wide use of a preselected slot

saturn/model3d.h
    convenience only; no new hardware policy
```

Dependency direction:

```text
model3d
   |
mesh3d       fade3d
   |           |
   +----- application policy
   |
vdp1  <---->  vdp2 configuration
   |
  HAL
```

`fade3d` must not depend on `mesh3d`, `model3d`, VDP1 or VDP2.

---

# Proposed implementation order

## Milestone A - prove the hardware

- [ ] Add `examples/vdp1_vdp2_color_calc`.
- [ ] Verify current `SPCTL` sprite type and command-bit layout.
- [ ] Demonstrate the available indexed-sprite color-calculation slots.
- [ ] Verify RGB polygon and Gouraud behavior separately.
- [ ] Record emulator/hardware observations in the example README or source comments.

## Milestone B - expose the primitive

- [ ] Add VDP2 sprite color-calculation ratio API.
- [ ] Add VDP1 per-part color-calculation selection.
- [ ] Preserve old APIs as no-color-calc wrappers.
- [ ] Add host tests for register and command composition.

## Milestone C - generic fade

- [ ] Add `fade3d.h`.
- [ ] Implement fixed-point stepped depth evaluation.
- [ ] Add boundary and invalid-input tests.
- [ ] Add a default Saturn 8-step mapping helper after visual tuning.

## Milestone D - mesh/model integration

- [ ] Add optional mesh-wide color-calc selection.
- [ ] Add `sat_project_depth()` or equivalent reusable depth helper.
- [ ] Add a helper that applies distance fade to a prepared mesh draw.
- [ ] Keep all existing draws visually unchanged by default.

## Milestone E - dedicated end-to-end example and optimization

- [ ] Add `examples/distance_fade_3d` as a permanent example.
- [ ] Wire it into the normal example/ISO build flow.
- [ ] Build a VDP2 sky/ground plus repeated textured 3D-object test scene.
- [ ] Add runtime modes for no fade, hard cutoff, 4-step fade and 8-step fade.
- [ ] Add movable/automatic camera motion through the fade zone.
- [ ] Add HUD diagnostics for depth, fade level, hardware slot, visible/culled counts and frame rate.
- [ ] Demonstrate multiple objects using different fade levels in the same frame.
- [ ] Add a stress preset for SH-2 and VDP1 command-cost comparison.
- [ ] Tune default fade ratios visually using the example.
- [ ] Validate on at least one accurate Saturn emulator and record hardware results when available.
- [ ] Document supported/unsupported lighting combinations.

## Milestone F - advanced integration

- [ ] Investigate a Gouraud-to-fade transition for a Sonic-R-like multi-stage effect.
- [ ] Combine the final fade zone with generated model LoDs.
- [ ] Experiment with hiding an LoD switch inside the fade region.
- [ ] Consider per-face fade only if profiling and visual tests justify it.

---

# Definition of done

The feature is complete when all of the following are true:

1. A game can configure up to eight supported sprite blend ratios through a documented LibSaturn API without direct register writes.
2. A supported VDP1 textured part can explicitly select the intended blend slot.
3. Generic fixed-point code can map object view depth into discrete fade levels.
4. A textured `sat_mesh_t` can apply one selected fade level without changing existing call sites that do not request the feature.
5. Objects can fade smoothly through the far-distance region and then be culled with no abrupt pop at the final boundary.
6. Existing VDP1, mesh, model and Gouraud tests remain green.
7. Hardware-specific restrictions for indexed textures, RGB polygons and Gouraud shading are explicitly documented and tested.
8. `examples/distance_fade_3d` visibly demonstrates no fade, hard cutoff, 4-step fade and 8-step fade using the public API.
9. `examples/distance_fade_3d` remains a permanent regression/demo target in the normal example build flow.
10. The implementation performs no heap allocation and does not require floating point.
11. Profiling shows that the feature adds only small per-object CPU overhead and does not add extra geometry commands solely to create the fade.

---

# Non-goals for the first implementation

- exact reverse engineering of Sonic R's renderer;
- arbitrary continuous alpha per polygon;
- order-independent transparency;
- solving VDP1 transparency sorting in general;
- automatic LoD generation or switching;
- drawing two complete LoD meshes for crossfade;
- per-pixel fog calculations on the SH-2;
- silently emulating unsupported RGB/Gouraud combinations in software.

The first target is intentionally narrow: **a clean, reusable, Saturn-native stepped distance fade for supported VDP1 3D geometry, backed by VDP2 color calculation and designed to compose with the rest of LibSaturn.**
