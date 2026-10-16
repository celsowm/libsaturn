# vdp2_rotation_coefficients

The modern RBG0 rotation manager's parameter sets A and B and the whole
per-line coefficient table contract, live on a procedural ocean (E02 of the
advanced VDP2 examples plan).

## The four scenes (60 frames each, cycling; A skips ahead, B toggles the overlay)

| Scene | RPMD | A's coefficient mode | What shows |
|---|---|---|---|
| SWELL | 0 — set A | kx/ky | The perspective floor with a sine swell travelling toward the horizon; rows at the horizon are hidden through the coefficient flag bit, so the sky bands on NBG0 show. |
| WAVE | 0 — set A | Xp | Each line's sampling origin is displaced by a travelling sine — including negative displacements, which is why the entry encoding is sign-magnitude (figure 6.7). The manual's Xp mode shares the coefficient slot with kx/ky, so the per-line depth factor is traded for the displacement and the scene flattens while it shimmers. |
| YAW B | 1 — set B | kx/ky (B's own table) | The same floor yawed 30 degrees by its rotation matrix: a visibly different picture, B's coefficient table driving k. |
| MIX | 2 — per-line switching | kx/ky | The coefficient flag bit picks the image per line: the yawed B above an animated boundary, the flat A below it. |

Every frame writes the *next* frame's parameter region and both coefficient
tables into the inactive half of a double buffer while the VDP2 reads the
active half; `sat_vdp2_layers_commit()` switches RPTA and both KTAOF offsets
in VBlank, so no table is ever rewritten while it is being read.

## The illegal combinations, demonstrated and recorded

- the rotation window mode (RPMD 3) needs the rotation window control
  register, which the manager does not model: `sat_vdp2_rotation_configure`
  and `sat_vdp2_rotation_set_parameter_mode` both refuse it with
  `SAT_ERR_INVALID_ARG`;
- a parameter region whose byte address has bit 7 set: RPTA's bit 6 is
  ignored and fixed to 0 for A and 1 for B (manual 6.3), so such a region is
  not what it seems — `sat_vdp2_rotation_write_params` refuses it before
  writing anything.

## Where the pieces live

- Scene math (perspective floor, swell, wave, A/B parameter sets):
  `include/saturn/vdp2_rotation_coefficients.h` — public, header-only,
  host-testable, the same header the ROM uses.
- Independent reference check: `tests/host/test_vdp2_rotation_coefficients.cpp`
  recomputes every value with double-precision libm.
- VRAM layout: reserved through the transactional plan
  (`sat_vdp2_plan_*`), including the ROTATION_NAMES/ROTATION_CHARS kinds
  that only live inside banks whole-claimed for rotation.
- Acceptance: `harness/run-vdp2-rotation-coefficients.ps1` (struct, reference
  math recomputed in Python, per-mode screenshots, zero misaligned accesses).

## Limits

- The window switching mode (RPMD 3), per-dot coefficients (they need a
  whole rotation bank or Color RAM) and the coefficient line-colour
  composition (RAKLCE/RBKLCE) are not modeled; the first two are refused,
  all three are recorded in `docs/VDP2_SUPPORT_MATRIX.json`.
- RBG1 is not implemented; that is E01 (`vdp2_dual_rotation`).
- Emulator evidence only (Ymir); the hardware checklist row stays PENDING
  until a physical run is recorded.
