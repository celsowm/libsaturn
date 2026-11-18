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
