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
