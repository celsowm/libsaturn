# B3 selective skinning consumer qualification

The public plain-deform path is the only experimental dispatch change.
Two matched Robot Police builds use the existing fixed 48-step viewer,
serial skinning and identical original hierarchy instrumentation
(`SAT_HIERARCHY_EXPERIMENT=1`, not the optimized B2 path); B1 disabled.
`SAT_SKIN_CHARACTER_EXPERIMENT=1` is the original control, 2 is
selective V2. Default is 0. Placed/projected paths are unchanged.
Mode is reported from the linked library, rather than a viewer-only flag.

Before the consumer test, run the extended synthetic V3 gate: unchanged
V2 scenarios plus Q10/Q12/Q0 unequal pairs and a 96-vertex Q10 proxy
(22 rigid,40 pairs,34 triples), matched V1/V2/control with guard checks.
The report grows to 2048 bytes, 16 scenarios, version 3. The old V1/V2
reports remain documented; current decoder expects version 3. Q10 proxy
is a distribution approximation, not the actual asset or joint locality.

Compare all 48 step hashes and command/face counts, final full matrices
and deformed vertices, independent Ymir / native Mednafen timing,
lossless late images and Ymir done-triggered image stability. Save exact
generated Robot Police C SHA256 and weight inventory per build, reject
changed source between builds. The skin interval includes copying joint
matrices as in B2; no inference of isolated kernel speed or physical FPS.

Run `python3 harness/scripts/run_weighted_character.py`.
One `harness/build/weighted_b3-qualification-return.zip` contains nested
synthetic and character archives, logs, images and local changes.
A failed synthetic gate prevents the character run and returns
failure evidence. No default changes. Build and target execution pending.

## Target qualification results — 2026-11-17

Checkout `77c34c81a9850a9351ec0439ecf3bfdf496fca7e`. Combined
archive reports synthetic success and completed character test; the
separately supplied character ZIP has identical member contents.
Local source snapshot complete, no untracked files or local changes.
All report fields used for numerical comparisons independently checked
against the binary telemetry.

### Extended synthetic gate

Two 2048-byte reports, three lanes per emulator, 64 rounds of sixteen
scenarios: 6144 cases, 497664 vertices per implementation, 26880 edge
checks, zero recorded errors. All lanes completed, parallel init and
execution status zero, slave ready, SR.S clear on entry, CCR 1.
Oracle classification across emulators: 567936 unequal-pair axes,
448128 triple axes and 73728 V1 predicted fallback axes.
Ymir 210540 Hz, Mednafen 210600 Hz; comparisons within each capture.
Ymir reported zero misaligned accesses. Mednafen native CD pass and
WRAM normalization against ELF text, ELF SHA256
`c01614a91a1f04d6ab8e0a93a1d4eb69510e68b64e3fe7d495ea893319b63143`.
No synthetic explicit panic dump or independent Ymir ELF verification.

| Serial master workload | Ymir control / V1 / V2 ticks | Mednafen control / V1 / V2 ticks | V2 reductions |
|---|---:|---:|---:|
| Q15 unequal pairs | 17779 / 15751 / 15557 | 16975 / 15296 / 15279 | 12.50% / 9.99% |
| Q10 unequal pairs | 22640 / 20125 / 20320 | 21913 / 19765 / 19597 | 10.25% / 10.57% |
| Q12 unequal pairs | 22352 / 19844 / 20031 | 21630 / 19486 / 19294 | 10.38% / 10.80% |
| Q0 unequal pairs | 21486 / 18978 / 19167 | 20754 / 18603 / 18446 | 10.79% / 11.12% |
| Q10 robot proxy | 21149 / 21211 / 20334 | 20256 / 20679 / 19333 | 3.85% / 4.56% |

Q0 unequal pairs reach 12.675% on Mednafen Slave. Triple-only Q15
regression persists (-3.40% Ymir, -2.39% Mednafen serial master);
extreme mixed workloads regress 1.02% / 1.15%. Keep selective
candidate opt-in; unchanged arithmetic does not mean unchanged timing.

### Matched Robot Police consumer

Both builds use GCC 13.2.0, B1 off, B2 original instrumented mode 1,
serial skinning. Generated C input SHA256 identical
`df7ef0c17187c88b6bea96f1378a57b30f942a0433ec682fb93434cb62debca5`.
Q10, 102 joints, 933 vertices, 210 rigid, 391 unequal pairs, 332 triples.
Six complete captures (288 steps), 23 distinct pose/vertex hashes,
all step hashes and command counts match (833–873 commands, 809–849
flushed faces). All six 8192-byte world matrix buffers and 11196-byte
vertex buffers are byte-identical. All six panic buffers contain zero,
no recorded draw timeouts, Ymir zero misaligned accesses.
All ten RGB images independently compared within each emulator,
including done-triggered Ymir captures: zero pixel differences.
Manifest eight comparisons cover 1508864 pixel comparisons, with
two additional done-triggered comparisons.

| Emulator / phase | Original median ms | V2 median ms | Reduction |
|---|---:|---:|---:|
| Ymir skin, all | 15.579 | 14.938 | 4.12% |
| Ymir work, all | 76.078 | 75.463 | 0.81% |
| Mednafen skin, all | 15.280 | 14.663 | 4.04% |
| Mednafen work, all | 81.255 | 80.622 | 0.78% |
| Ymir work, new pose | 80.070 | 79.448 | 0.78% |
| Mednafen work, new pose | 85.257 | 84.611 | 0.76% |

Median of 46 measured steps after first pair; normalize each report
by its own calibrated timer frequency. Mednafen control 210540 Hz,
candidate 210600 Hz. Work is measured inclusive guest work, not FPS.

**Disposition:** B3 V2 passes the tested plain-deform consumer gate.
Keep it as an experimental candidate; prioritize C1 next, given
the larger geometry/flush cost. No production activation. Real hardware,
explicit IRQ/ABI stress, projected/placed consumers, Fighter scenes,
parallel renderer, combined B1/B2/B3, and per-step literal VDP1
command streams remain unqualified. Final-pose screenshots are not
every intermediate raster. No unqualified claims of universal speedup.
