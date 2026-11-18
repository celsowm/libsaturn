# C2 — selective near-plane V2 qualification

V1's general depth checks cost 14–17% on the oblique inside
renderer phase. Do not adopt V1 as-is.

- Mode 0: original kernel. Public flag 1 selects instrumented control.
- Mode 2: historical V1 combined candidate, kept reproducible.
- Mode 3 (V2-A): original depth arithmetic, exact bounded DIVU
  intersections from V1.
- Mode 4 (V2-B): same intersections, plus unit axis selection once
  per face. Select one member of sat_vec3_t and sign outside the
  four-vertex loop; subtract at signed 64-bit width. If not an exact
  signed unit axis, use the entirely original general depth expression,
  without V1's narrow-delta range guards.
- Flag zero remains default. No altered public API or precision.

Synthetic gate: 24 workloads, 64 x 32 quads per workload, three CPU
lanes per emulator. Compare modes 0/3/4 to the same scalar oracle,
status, complete vertex data and untouched guards. Includes the 20
V1 cases plus oblique all-in/out, positive Y and a wide axial eye
difference. 9216 batches, 294912 quads per implementation across
both emulators. Version 2 report is 2432 bytes. Old V1 harness retained.

Renderer: new example retains V1 geometry and adds axial inside.
Four phases of four poses, each repeated twice, 32 frames per run.
Axial crossing, axial inside, oblique inside, oblique crossing.
Control/3/4 builds; Ymir and both Mednafen timing/visual captures
for each, nine executions and 288 measured frames. Same 29 faces
per frame, 18 crossings in the crossing phases and zero in inside
phases (one wholly rejected face always present). All other
experiments off, screen clip original. Version 2 scene report
1568 bytes; capture commands and Gouraud tables for all 32 frames.
Compare full literal bytes including END/unused tails and bank links,
plus final frozen oblique crossing raster. Phase medians omit each
first repeated pair. Report incremental effect of axis choice via
the separate candidates; no overall median as a gameplay estimate.

Gate progression via run_near_clip_v2.py: run both emulators' synthetic
tests, proceed to renderer only on successful synthetic exit and
archive, preserve submitted source snapshot and stage archives even
on failure. Keep both candidates opt-in pending evidence. Select
based on consistent timing, especially non-crossing cases, not
maximum synthetic gain. Ymir DIVU immediate result is not physical
latency; explicit IRQ/ABI stress and Saturn hardware qualification
remain required. No production activation or full game performance claim.

Run `python3 harness/scripts/run_near_clip_v2.py`. Return
`harness/build/near_clip_v2-qualification-return.zip`.

## First V2 execution — invalid benchmark, corrected before retry

Checkout `fb328c5b0cc292a70e13a0cfd0b0ff2c36f82e8f`.
All three returned ZIPs passed integrity checks; the nested synthetic
archive matched the standalone archive byte for byte, and standalone
Mednafen members matched their combined counterparts. Clean checkout.
The 2432-byte raw reports agree with JSON scenario words. The wrapper
correctly stopped after the synthetic stage failed; no renderer ran.

Both serial master lanes completed 1536 batches but failed 1472
(all 23 valid-argument scenarios, all 64 repetitions). Invalid
arguments alone passed. Each parallel lane recorded just one failed
batch and never completed. Parallel status was nonzero in both
emulators (1 Ymir, 4294772122 Mednafen); these corrupted status values
are not interpretable as a diagnosis of the parallel runtime.
Ymir reports no misaligned accesses, which does not rule out aligned
out-of-bounds stores.

**Root cause:** the V2 synthetic harness reused the kernel ID as
the output buffer index. `Work::out[3][32]` was accessed as
`w.out[M][i]` for M=3 and M=4, outside the valid buffers 0..2.
The returned build log explicitly emits GCC -Warray-bounds for both
instantiations. This is an error in the experiment harness, not
evidence of a clipping arithmetic defect. All timings and output
equivalence data from this execution are invalid and must not be
used as qualification evidence. Fix the mode-to-slot mapping
explicitly with compile-time assertions: 0->0, 3->1, 4->2.
No candidate algorithm changes. Re-run the complete qualification
from scratch; do not bypass the synthetic gate.

## Corrected V2 qualification — 2026-11-18

Checkout `dac68006a248f54c49480a388914fbc2154c071e`. Both
stages returned 0 and qualification completed=true. Outer and nested
archives pass integrity checks; standalone renderer ZIP is byte-identical
to nested renderer ZIP. All three local manifests complete and clean.
No array-bounds warning in corrected Ymir synthetic build log.

Synthetic: independently decoded both 2432-byte big-endian reports
and compared lane and row fields against JSON. Six lanes completed,
9216 batches, 294912 quads and 492042 output pieces per implementation,
zero errors. Both parallel init and execution status zero, slave ready,
CCR 1, SR.S clear on entry. Ymir target unittest PASS and zero reported
misaligned accesses on both CPUs. Mednafen native CD PASS, exit 0,
swapped16 normalized WRAM, ELF SHA256
`f718b85941500a8e8964332d8203822f0caf386913786a17ba92004f1057e73b`.
No separate synthetic panic capture.

### Synthetic serial Master reductions versus control (%)

| Workload | Ymir V2-A | Ymir V2-B | Mednafen V2-A | Mednafen V2-B |
|---|---:|---:|---:|---:|
| inside | -0.61 | 55.88 | -0.97 | 53.62 |
| outside | -0.36 | 58.40 | -0.92 | 56.16 |
| two_inside | 42.12 | 54.88 | 38.42 | 50.41 |
| alternating | 50.39 | 58.46 | 46.69 | 54.09 |
| on_plane | 49.20 | 61.89 | 47.24 | 59.26 |
| oblique_xz | 45.11 | 44.13 | 42.00 | 41.04 |
| oblique_xyz | 42.79 | 41.70 | 40.23 | 39.21 |
| wide_coordinate_delta | 20.99 | 34.27 | 17.27 | 29.80 |
| wide_depth_delta | -5.79 | 8.58 | -5.85 | 7.64 |
| oblique_inside | -0.69 | -2.03 | -0.99 | -1.86 |
| oblique_outside | -0.41 | -3.41 | -1.14 | -2.78 |
| wide_axial_eye | -0.66 | 55.86 | -0.92 | 53.59 |

Summing equal-repeat workload ticks within each lane: V2-A
37.573–37.589% / 34.338–34.421% reduction (Ymir / Mednafen);
V2-B 48.628–48.630% / 44.557–44.810%. Peak V2-B 61.904%
Ymir parallel master and 59.264% Mednafen serial master (on_plane).
Synthetic FRT 210540/210600 Hz respectively. No gameplay weighting.

Renderer: all nine 1568-byte telemetry captures decoded against JSON;
independently recomputed phase medians (six frames after first pair).
GCC 13.2.0, matching flags except near-clip mode. FRT 210540 Hz
except Ymir DIVU-only build at 210600 Hz; normalize each capture using
its own frequency. 288 frames, 8352 clip calls, 2592 multi-piece calls,
10656 near output pieces, 15498 native commands including END and
4320 Gouraud tables. Commands 43–66 and tables 12–18 per frame.
Nine 80-byte panic records all zero, no draw timeouts, all three
Ymir misalignment reports master/slave zero.

All nine full 262144-byte command snapshots identical, SHA256
`7a9683df8a2b465260653d4f1309c9ff33cc51d5078ca048054aef89b01feb12`.
All nine 65536-byte Gouraud snapshots identical, SHA256
`d89529f2470f53974d8d0540981d39dd1a4a990a37e288863a76aab068360368`.
Verified each live slice hash, END bit and zero tails. Six Mednafen
captures agree with their build ELF hashes, swapped16 normalization.
ELFs control `a69107eaec4c0b4c058881af5ecccc5d5465fa5f9b12abd53a8473c23e696850`,
DIVU `ca7482b00e1933a847f08281db8c272fe7933518f2d012ab3c25924c9f9a2bff`,
axial `c809bf39029f73332a75821cde002b605ae52451151f13e14488eb17d55e9e11`.
All 15 original screenshots independently decoded, exact RGB within
each emulator: nine Ymir 320x224 (three done-triggered) and six
Mednafen 682x448, 106 colors each. Fourteen manifest comparisons
cover 2640512 pixel comparisons, plus three Ymir done comparisons.
These are frozen final oblique crossing rasters; intermediate
poses qualified by commands, not per-pose raster captures.

### Renderer phase medians in milliseconds

| Emulator / phase | Clip control / A / B | Submit control / A / B | B submit reduction |
|---|---:|---:|---:|
| Ymir axial crossing | 2.14686 / 1.45062 / 1.04968 | 17.62610 / 16.92783 / 16.53368 | 6.198% |
| Ymir axial inside | 0.73858 / 0.75261 / 0.34673 | 14.23482 / 14.23789 / 13.83585 | 2.803% |
| Ymir oblique inside | 0.75520 / 0.77160 / 0.76233 | 14.18020 / 14.18329 / 14.19445 | -0.100% |
| Ymir oblique crossing | 2.15161 / 1.43637 / 1.45578 | 17.62610 / 16.92545 / 16.94452 | 3.867% |
| Mednafen axial crossing | 2.25848 / 1.61252 / 1.22305 | 18.58079 / 17.94433 / 17.55011 | 5.547% |
| Mednafen axial inside | 0.77657 / 0.78607 / 0.39185 | 14.95915 / 14.96153 / 14.55305 | 2.715% |
| Mednafen oblique inside | 0.78370 / 0.79082 / 0.80270 | 14.89503 / 14.89741 / 14.90453 | -0.064% |
| Mednafen oblique crossing | 2.26798 / 1.62914 / 1.63864 | 18.57842 / 17.93721 / 17.94908 | 3.387% |

Mednafen visual and timing phase medians agree. V2-B cuts clip
time 51.106/45.846% axial crossing, 53.055/49.541% axial inside,
32.340/27.749% oblique crossing (Ymir/Mednafen); residual oblique
inside regression 0.943/2.424%. Submission savings up to 1.092/1.031
ms, residual oblique inside cost 0.01425/0.00950 ms.
V2-A is marginally preferable in oblique-only submission, but loses
the large axial gains. Choose V2-B as general opt-in qualification
candidate; neither candidate unconditionally improves every case.
Do not activate by default. No reason for another narrow timing-only
iteration before assessing broader workloads. Next investigate
world-space face visibility/backface classification (C2), and
qualify combined near/screen paths and IRQ/DIVU ownership separately.
Ymir divider model is immediate-result-no-busy-wait; no physical
timing, IRQ/ABI, textured cells or real game performance claim.
