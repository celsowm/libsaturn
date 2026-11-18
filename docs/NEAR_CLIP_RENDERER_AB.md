# C2 — near-plane clipping renderer A/B

The synthetic mode 2 candidate is tested through the production
`sat_clip_quad_near` public API, with matching instrumented mode 1
(original implementation). The Make flag SAT_NEAR_CLIP_EXPERIMENT
defaults to zero, preserving the production build. The flag participates
in the existing library build key. No changes to screen clip dispatch
in this experiment: screen clip is original in both builds, as are
projection, skinning and hierarchy. Direct internal near-clip calls
elsewhere are not redirected by the public API flag.

The 320x224 example draws the same 29 world faces through the
public RGB, indexed solid and Gouraud immediate renderer. It uses a
Q16 camera basis with forward +Z or an oblique yaw basis (sine 39322,
cosine 52429), a matching perspective projection, and near depth 2
world units. Geometry is defined in camera coordinates and transformed
into world coordinates; all input points have positive W, some lie
behind the near plane. Two parallel world edges at depths 1 and 4
form the actual straddling faces. One wholly rejected face is present
in every pose.

24 measured frames, 12 poses, each repeated twice, in three phases:
- poses 0–3 axial crossing: 18 straddling faces;
- poses 4–7 oblique inside: 28 all-inside, one all-outside, no crossings;
- poses 8–11 oblique crossing: 18 straddling faces.
Each crossing emits two near-clipped fan pieces. Thus 29 near calls,
18 multi-piece calls and 46 pieces in crossing frames; 29, 0 and 28
in non-crossing frames. The decoder enforces these counts. This does
not claim that every near output is a visible screen primitive.

Reuse the screen renderer capture method: current VDP1 live command
words including END and all Gouraud tables for every measured frame,
zero capture tails. Two matched builds, each with Ymir, Mednafen timing
and Mednafen lossless visual execution. Require identical literal
command/table snapshots across builds and emulators, zero panic and
draw timeouts. Compare final frozen oblique crossing screenshots in
each emulator and Ymir done-triggered image. Intermediate poses are
checked by command bytes, not individual screenshots.

Report median clip, submission and end interval overall (exclude first
pair), and separately for each phase (exclude first pair in each phase).
No input-dependent simulation or asset randomness. Submission excludes
begin-frame wait and diagnostic snapshot; end is presentation call
only. Do not equate with FPS. Ymir DIVU immediate-result timing is not
physical divider latency; both emulators and eventually hardware are
needed. The scene deliberately stresses clipping; no claim about
Skybridge queued rendering or actual gameplay frequency. The experiment
does not qualify IRQ reentrancy of DIVU, combined C2 optimizations,
textured cells or physical console performance.

Run `python3 harness/scripts/run_near_clip_renderer.py`, return
`harness/build/near_clip_renderer-return.zip`. Failure logs and local
source changes included. Compilation and target execution pending.

## Matched renderer results — 2026-11-18

Checkout `2550c3e30152447cb2f543f0855bd9b960e8bed5`.
Return summary completed=true; ZIP integrity verified. Source manifest
complete and clean, all tracked/staged/unstaged patches empty.
Independently decoded six 1184-byte big-endian telemetry records
against JSON. All captures use FRT 210540 Hz, GCC 13.2.0 builds,
only SAT_NEAR_CLIP_EXPERIMENT differs; other experiments disabled.

Six executions, 144 measured frames, 4176 near calls, 1728
multi-piece calls, 5760 near output pieces, 8016 native commands
including setup and END, and 2244 Gouraud tables. Per-frame counts
match all three phase expectations: 29 calls, 18/0/18 multi-piece
and 46/28/46 near pieces. Commands range 43–66, tables 12–18.
All six 80-byte panic records zero, no draw timeouts; zero
misaligned accesses on both Ymir CPUs.

All six 196608-byte command buffers are literally identical,
SHA256 `e2a7d73a58a6806ae1e4915fdc7ced8af5ecc5fc88d5e52db72ff439b314b14a`.
All six 49152-byte Gouraud buffers identical, SHA256
`f226a5edfa7c92049fa9546d35371c3d4bbf4d688d628ecf721fbc9feec1908b`.
Independently verified each live slice hash, END marker and unused
zero tails. Native Mednafen WRAM normalized (swapped16), ELF hashes
agree with build manifests in both timing and visual captures:
control `2aedb7a9867157f57f1e9c582b340c5ec54c6c903f4350533510d7d0b1399bfe`,
bounded `7d7bc8b200a5a6220ba091c5b53c1f884ab632e19f3ced76c73417a7083a6edf`.
All ten PNG originals independently decoded: six Ymir (320x224)
and four Mednafen (682x448); no differing RGB pixels within each
emulator, 106 distinct RGB colors each. Eight manifest comparisons
cover 1508864 pixel comparisons, plus two done-triggered Ymir
comparisons. Images represent final frozen oblique crossing pose,
not screenshots at each intermediate pose; native command captures
cover every measured step.

### Median milliseconds (first pose pair excluded per phase)

| Phase | Emulator | Clip control / bounded | Clip reduction | Submit control / bounded | Submit reduction |
|---|---|---:|---:|---:|---:|
| Axial crossing | Ymir | 2.146860 / 1.180298 | 45.02% | 17.628479 / 16.652418 | 5.54% |
| Axial crossing | Mednafen | 2.270352 / 1.353662 | 40.38% | 18.609290 / 17.668852 | 5.05% |
| Oblique inside | Ymir | 0.748076 / 0.876318 | -17.14% | 14.182578 / 14.306070 | -0.87% |
| Oblique inside | Mednafen | 0.793198 / 0.907191 | -14.37% | 14.909281 / 15.001900 | -0.62% |
| Oblique crossing | Ymir | 2.146860 / 1.579272 | 26.44% | 17.628479 / 17.046642 | 3.30% |
| Oblique crossing | Mednafen | 2.272727 / 1.740762 | 23.41% | 18.604541 / 18.058326 | 2.94% |
| All (22 frames) | Ymir | 2.139736 / 1.180298 | 44.84% | 17.070390 / 16.376935 | 4.06% |
| All (22 frames) | Mednafen | 2.263228 / 1.353662 | 40.19% | 18.034578 / 17.383870 | 3.61% |

Mednafen visual and timing run medians agree. Median END duration
unchanged, 3 ticks Ymir, 4 Mednafen; combined submit+end overall
17.084639 -> 16.391185 ms Ymir and 18.053577 -> 17.405244 ms
Mednafen. Overall medians mix different phases and are not
paired per-frame savings or gameplay-weighted averages. DIVU timing
in Ymir remains immediate-result-no-busy-wait; no physical timing claim.

**Disposition:** V1 passes exactness in the controlled immediate
renderer, but do not adopt the combined depth path unchanged.
Oblique non-crossing regression is measurable: approximately
0.123 ms Ymir and 0.093 ms Mednafen per 29 near calls in this scene.
V2 should compare interpolation-only (unchanged original depth)
against a selective axial depth path with axis selection hoisted
outside the per-vertex loop. Avoid the general narrow-delta range
checks unless independently justified. Include the same workload and
an axial non-crossing phase in the next qualification; preserve exact
command bytes and test wide fallback. Do not infer IRQ safety from
these captures. No production activation, physical hardware,
queued Skybridge/gameplay or combined screen/near optimization proof.
