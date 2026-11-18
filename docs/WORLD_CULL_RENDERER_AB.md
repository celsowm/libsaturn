# C2 — world-cull mode 3 renderer qualification

Use the existing textured Sonic OBJ/MTL/PNG fixture from basic_3d_texture
and its same import parameters. This is real static asset geometry,
not the Tekken or Skybridge game, and does not establish animated mesh
incidence. The separate Robot Police animation follow-up remains useful
if workload coverage or timing is inconclusive.

SAT_WORLD_CULL_EXPERIMENT 0 is the default, 1 matched control, 2 the
selected V2-A (kernel mode 3), 3 census-only original control. The
library build key includes this switch. The compiled submission branch
keeps the exact original signed16 small body, redirects only general
fallback to mode 3; the generic branch uses quad_visible<3>.
No changes to screen-only culling, standalone mesh API, culling order,
double-sided behavior, near clipping or painter. No new ASM leaf.
Enable existing instance projection/preparation metrics in all builds;
extra census branches exist only in mode 3, which is not used for
timing comparisons. The global diagnostic census is single-Master
only, not a concurrent/parallel-scene telemetry API.

Four yaw angles 0,45,90,135 at fixed pitch/distance, each twice.
Eight measured frames with original validated material binding, then
eight with the compiled program binding (same camera poses and mesh).
An explicit diagnostic double-sided mask every 17th face is not
original asset metadata; report that incidence separately. All model
materials must compile, and the census requires exactly the intended
compiled/general route in each frame. The census records input,
bypassed, screen rejected, world tests, small body hits, signed16
general hits, wide fallback, initial zero normal, final zero normal,
world rejects, and actual fast/general instance admissions.
It recomputes route classification only in the census build.

Three builds, each on Ymir and native Mednafen timing and visual runs:
nine executions, 144 recorded frames. Record actual VDP1 command
words including END, and live Gouraud tables with zero capture tails
for every measured frame. A fixed RGB shaded swatch is drawn after
the textured model to exercise Gouraud capture; it is not character
shading. All nine command/table buffers must match literally for
the same frame, including addresses; timing and census builds must
produce the same cull/emission counts. Require zero panic/draw
timeouts and lossless final frozen-pose screenshot equality within
each emulator. Intermediate camera poses compare actual commands,
not independent rasters. Require same generated C asset hash.

Per-phase medians omit first pose pair: submit, projection,
preparation, flush, end, submit+flush. Census-only report has no
performance medians. Report wide incidence and separate world/screen
counts, not extrapolated synthetic speedup. Explicitly keep the
near and screen experiments and B1/B2/B3 off. No activation or
hardware/IRQ/FPS claim. Compilation and target execution pending.

Run `python3 harness/scripts/run_world_cull_renderer.py`; return
`harness/build/world_cull_renderer-return.zip`.

## Target renderer results — 2026-11-18

Checkout `2e9b3467d7147777041c1f0903a93c4296eb27c9`.
ZIP integrity passes (110 entries); source manifest complete and clean,
all three tracked/staged/unstaged patches empty. All nine 1696-byte
telemetry reports independently decoded as big-endian words and
checked against JSON. Same 185-face, 285-vertex generated Sonic asset
SHA256 `8ed35384c5780d62cbbe788a4a5031e72ca3fb60a1d8ab86c3c5bbe94ea77df5`;
GCC 13.2.0 and matching flags except world-cull mode.

Nine executions, 144 measured frames, 13356 VDP1 commands (90–96 per
frame), 144 Gouraud tables, 12780 emitted faces and 13860 culled faces.
Every 80-byte panic record zero, no draw timeouts, Ymir misalignment
totals zero on both CPUs. All nine complete 131072-byte command
buffers identical, SHA256
`956771bf103ad65d31d250330b823fdee63004dd2eb3e4d4d203876fdad91e34`;
all nine 4096-byte Gouraud buffers identical, SHA256
`6c7312f98186cd09d06d57dacfcebbcff4170bccb7741ae50c4df14d56108ede`.
Independently verified live-slice hashes, END bit and zero tails.
Fifteen original screenshots independently decoded and RGB-identical
within each emulator (nine Ymir 320x224, six Mednafen 682x448;
110 distinct colors in each). Fourteen manifest comparisons cover
2640512 pixel comparisons; three additional Ymir done images match.
Only the final frozen pose has raster evidence; intermediate frames
are covered by command captures.

### Phase medians (ms; first repeated pose pair excluded)

| Emulator / route | Prepare control / candidate | Submit control / candidate | Submit+flush control / candidate | Submit+flush reduction |
|---|---:|---:|---:|---:|
| Ymir general | 4.103733 / 4.222476 | 6.678066 / 6.796808 | 8.506697 / 8.625439 | -1.396% |
| Ymir compiled | 3.225040 / 3.334283 | 5.804123 / 5.908616 | 7.086539 / 7.200532 | -1.609% |
| Mednafen general | 4.437322 / 4.559704 | 6.673789 / 6.792059 | 8.843780 / 8.967417 | -1.398% |
| Mednafen compiled | 3.511396 / 3.628764 | 5.752612 / 5.870618 | 7.421652 / 7.544885 | -1.660% |

Normalize every capture by its own reported frequency: 210540 Hz
Ymir, 210600 Hz Mednafen control/census, 210540 Hz Mednafen candidate.
Mednafen visual and timing medians agree. Projection/flush largely
unchanged; preparation regresses 2.76–3.39%. Submit+flush costs
approximately 114–124 us more. No timing conclusion from census-only
build or physical frame-rate inference.

### Route incidence

Each eight-frame phase: 1480 inputs, 88 diagnostic double-sided
bypasses, 752 screen rejects, 640 world tests, all 640 wide,
zero small/general16, zero degenerate/zero normal, 18 world rejects.
Compiled admissions 8/0 (compiled/general) in the compiled phase,
0/8 in the general phase. Across the 16-frame census: 2960 inputs,
176 bypasses, 1504 screen rejects, 1280 world tests, 36 world rejects
(2.8125% of world tests). The census does not measure single-axis or
zero-low-residue incidence; do not claim they are absent or diagnose
their exact overhead from these data.

**Disposition:** reject mode 3 for default adoption on this evidence.
The synthetic gains do not transfer to this textured real static
mesh, despite every world test taking the wide route. Keep the
original production predicate and the candidate opt-in for research.
Do not remove the remaining world test merely because its rejection
rate is low: it changes visible output. No additional speculative
micro-optimization iteration without workload-specific evidence.
Advance to C3 VDP1 command generation; animated geometry, physical
hardware, IRQ/ABI and combined C2 qualification remain open.
