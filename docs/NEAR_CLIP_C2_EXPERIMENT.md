# C2 — exact world near-plane clipping

Mode 0 is the existing world depth and interpolation algorithm.
Mode 1 specializes exact signed unit camera axes and narrows world
position differences only when all three fit int32; the products and
their sum remain signed int64, including truncation toward zero.
Mode 2 additionally uses the existing per-SH-2 signed DIVU helper
for exact interpolation. A crossing constrains the ratio to [0,1].
The denominator must fit [-INT32_MAX, INT32_MAX] and each accelerated
coordinate delta must fit [-INT32_MAX+1, INT32_MAX-1], proving
the quotient cannot trigger the documented signed divider overflow
edges. Numerator equals zero or denominator returns the respective
endpoint. Zero coordinate differences need no division. All wider
operands use the original formula. No fixed-point reciprocal
approximation. No new assembly ABI. Existing default mode remains 0.

Scope is inputs on which the existing signed-64 control arithmetic
is defined. This does not repair overflow in the original API for
unbounded non-normalized forward vectors or extreme products; any
future overflow hardening must be a separate correctness change.
The DIVU registers are CPU-local; interrupt reentrancy must be
qualified separately, not inferred from parallel master/slave success.

20 workloads x 64 batches x 32 quads x three lanes x two emulators.
Both windings, all-in/out, one/two/three inside, alternating,
boundary equality, duplicate corner, signed axis cameras, oblique
and nonunit cameras, invalid arguments, 1-LSB crossings, large
coordinate differences, large eye deltas and denominator fallback.
Compare status, exact output vertices, order, count and untouched
sentinel slots against control and scalar ring oracle. The oracle
shares the original truncation contract, not an alternate geometry
specification. Timed complete clipping, not oracle or seed generation.
No renderer, textured cells, physical hardware or explicit IRQ test
in this stage. No default activation.

Run `python3 harness/scripts/run_near_clip.py`. Return
`harness/build/near_clip_bench-return.zip` with both emulators and
local changes.

## V1 target results — 2026-11-18

Checkout `f492ee3423aa7fa62c82bbedd6771488b980e791`.
Both 2048-byte reports independently decoded as big-endian words;
every reported header, lane and scenario field matched JSON.
Standalone Mednafen archive duplicates its combined archive members
byte for byte. Local source manifest complete, all three tracked
patches empty, no nonignored untracked files.

Six lanes completed, 7680 batches, 245760 input quads and 442890
output primitives per implementation; zero recorded errors. Parallel
initialization and execution status zero, slave ready, CCR 1, SR.S
clear on entry. Ymir target unittest passed, 9000 fields and no
misaligned accesses reported on either CPU. Mednafen native CD PASS,
exit 0, swapped16 WRAM normalized against ELF text; ELF SHA256
`677b7a3bd4eee738d12833246282629dc0f48ce88606e214871edeaf6f87de8a`.
No explicit panic dump or independent Ymir ELF check.

### Serial Master FRT sums (64 calls of 32 faces per scenario)

| Scenario | Ymir control / depth / combined | Mednafen control / depth / combined | Combined reduction Ymir / Mednafen |
|---|---:|---:|---:|
| inside | 10669 / 6581 / 6647 | 11145 / 6850 / 6994 | 37.70% / 37.25% |
| outside | 9632 / 5251 / 5322 | 10100 / 5614 / 5763 | 44.75% / 42.94% |
| one_inside | 42764 / 38562 / 19847 | 45257 / 41068 / 23198 | 53.59% / 48.74% |
| two_inside | 43938 / 39807 / 21313 | 46579 / 42405 / 24708 | 51.49% / 46.96% |
| three_inside | 45221 / 41199 / 22535 | 47980 / 43865 / 26066 | 50.17% / 45.67% |
| alternating | 73489 / 69217 / 32317 | 77862 / 73570 / 37951 | 56.03% / 51.26% |
| on_plane | 44785 / 40753 / 18641 | 47557 / 43427 / 21192 | 58.38% / 55.44% |
| duplicate_corner | 43939 / 39848 / 22255 | 46569 / 42420 / 26034 | 49.35% / 44.10% |
| axis_x | 43940 / 39816 / 21181 | 46571 / 42393 / 24335 | 51.80% / 47.75% |
| axis_negative_z | 44061 / 40515 / 21883 | 46708 / 43102 / 25212 | 50.34% / 46.02% |
| oblique_xz | 42665 / 44444 / 25160 | 45212 / 47032 / 28195 | 41.03% / 37.64% |
| oblique_xyz | 32004 / 33462 / 19746 | 33901 / 35386 / 21860 | 38.30% / 35.52% |
| wide_coordinate_delta | 44021 / 39895 / 30684 | 46660 / 42484 / 34694 | 30.30% / 25.65% |
| wide_depth_delta | 37330 / 33296 / 35387 | 40069 / 35976 / 38706 | 5.21% / 3.40% |
| wide_eye_delta | 42759 / 44587 / 26492 | 45386 / 47236 / 29890 | 38.04% / 34.14% |
| invalid_args | 1360 / 1362 / 1358 | 1651 / 1623 / 1622 | 0.15% / 1.76% |
| axis_negative_y | 44057 / 40391 / 21808 | 46698 / 43052 / 25231 | 50.50% / 45.97% |
| axis_negative_x | 44064 / 40455 / 21715 | 46702 / 43104 / 24855 | 50.72% / 46.78% |
| nonunit_forward | 43922 / 46095 / 27575 | 46554 / 48720 / 30993 | 37.22% / 33.43% |
| one_lsb_crossing | 43905 / 39790 / 21299 | 46545 / 42377 / 24713 | 51.49% / 46.91% |

Combined reduction across equally repeated synthetic workloads
(summing raw FRT ticks within each lane) 45.645–45.650% Ymir,
41.361–41.600% Mednafen. Depth-only reduction 6.823–6.833%
and 6.537–6.545%, respectively. Max combined gain 58.401%
(Ymir parallel Master, on_plane). Depth-only oblique and nonunit
workloads regress about 4–5%; wide denominator path retains only
a small combined gain. FRT 210540 Hz Ymir, 210600 Hz Mednafen.
The Ymir probe explicitly reports division timing model
`immediate-result-no-busy-wait`: the Ymir DIVU speedup is not
cycle-accurate evidence for hardware division latency. Report both
emulators separately; neither establishes physical timing. Synthetic
workload mix is not a game profile or a frame-time prediction.

**Disposition:** combined mode 2 passes the tested arithmetic
gate and merits a matched immediate renderer A/B with genuinely
near-straddling geometry, axis-aligned and oblique cameras, full VDP1
command/Gouraud captures and lossless screenshots. Keep screen clip
and B1/B2/B3 identical (original) between builds. Include inside-only
oblique workloads to check for regressions. Consider hoisting axis
selection once per face and testing an interpolation-only variant
if profiling warrants it. No default activation. Interrupt safety
of the CPU-local DIVU registers, physical timing, textured cells,
full gameplay and original out-of-domain overflow hardening remain
unqualified.
