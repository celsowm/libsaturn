# B2 real character experiment

Robot Police is a true runtime skeletal asset (unlike Dino's baked poses).
Three GNU Make builds select instrumented original, reorganized C++, and
native SH-2 at the same public hierarchy API. B1 disabled; no default
dispatch change. The viewer runs in serial skinning mode, fixed camera,
48 simulation steps (24 explicit clip times, each repeated once), then
holds the final pose with a constant FINAL POSE HUD. The first pair is excluded from
timing statistics. Whole-frame work includes begin, animation, local
construction, hierarchy, skinning, scene preparation, flush and present.
The hierarchy interval uses FRT; other phases use elapsed ticks. Matrix
and deformed vertex FNV-1a hashes and command/face counts are compared at
each simulation step, outside timed work. Hash agreement is not literal
command-list equality. Final world matrices and deformed vertices are also dumped in full and
compared by SHA-256 between builds and emulators. Full per-step matrix
and VDP1 command-byte equality, other characters and parallel scene
preparation are separate future gates.

Ymir saves two late screenshots and a done-triggered reference image
120 fields after the guest completion word is set. The late sample must
match the triggered reference. Mednafen boots the same discs natively
in fresh profiles, first without recording for timing and independently
with lossless recording for two late screenshots. Compare whole rasters
within each emulator and verify each variant's held-pose stability. Images
are included in the ZIP, with diff masks; no large movies or discs.
The fixed recording indices must actually be after the final measured pose;
stability checks and guest completion provide evidence, not an explicit
per-video-frame telemetry binding. If the scene is not stable, reject.
No FPS/hardware claim follows.

Run from repository root:
```sh
python3 harness/scripts/run_hierarchy_character.py
```
Requires toolchain, both emulators, BIOS and FFmpeg. Returns
`harness/build/hierarchy_character-return.zip` including partial logs on
failure. Target builds and capture remain to be validated.

## Completed character A/B/C — 2026-11-17

Archive `hierarchy_character-return.zip`, checkout
`b03a1c6165598a323e6104aae72cfd9bfa8804ce`. Summary completed=true.
All nine reports contain 48 completed simulation steps, 102 joints,
933 vertices, zero draw timeouts. The binary 2720-byte reports were
independently decoded; all 432 rows match their JSON records. All nine
80-byte panic records are zero. All three Ymir misalignment reports
have zero Master/Slave accesses. Native Mednafen 1.32.1 reports identify
the corresponding build ELF by SHA-256 and verify native WRAM .text.
Final 8192-byte joint arrays and 11196-byte vertex arrays are identical
in all nine captures. Across all simulation steps, pose times, matrix
and vertex hashes, command counts (833–873) and face counts (809–849)
match; 23 distinct matrix hashes occur in the sequence. Fourteen
manifest RGB comparisons independently agree (2,640,512 pixel
comparisons, zero differences); all 15 original screenshots agree
within each emulator, including the three Ymir done-triggered images.
The displayed final character is nonuniform and clearly rendered.

### New-pose medians (ms), first pose pair excluded

| Emulator/stage | Control | C++ | ASM | ASM reduction |
|---|---:|---:|---:|---:|
| Ymir hierarchy | 5.282 | 3.947 | 3.671 | 30.49% |
| Ymir skeleton including local construction | 7.808 | 6.474 | 6.203 | 20.56% |
| Ymir whole measured work | 80.104 | 78.797 | 78.484 | 2.02% |
| Mednafen hierarchy | 5.351 | 3.800 | 3.657 | 31.66% |
| Mednafen skeleton including local construction | 8.215 | 6.664 | 6.521 | 20.61% |
| Mednafen whole measured work | 85.418 | 83.894 | 83.751 | 1.95% |

Independent medians from raw FRT records; 23 new and 23 repeated
poses after warmup. Ymir 210540 Hz; Mednafen original 210600 and
candidates 210540 Hz (converted separately). On this real character
the new C++ reduces new-pose hierarchy by 25.27% Ymir and 29.00%
Mednafen. ASM further reduces the reorganized C++ time by 6.98%
and 3.75% respectively. In Mednafen the incremental whole-work
saving over new C++ is only 0.143 ms (0.17%). Across the mixed
new/repeated sequence, whole-work reduction vs original is 1.01%
Ymir and 0.85% Mednafen for ASM; 0.83% and 0.85% for C++.
Repeated poses already skip hierarchy recomposition (around 4.75 us).
No FPS increase is established.

### Decision and limitations

Pass B2 real-character gate within tested scope. The portable C++
candidate captures most of the improvement; prefer it as the first
candidate for release qualification, with the SH-2 specialization
remaining independently selectable. No default dispatch change yet.
The checkout is dirty: tracked local modifications to
`src/graphics/3d/scene/scene.cpp` and `tests/host/test_scene_api.cpp`,
plus untracked .DS_Store files. The exact local patch is not included
in the return archive, so clean-revision reproduction is not certified.
B1 disabled, serial skinning, one asset, fixed camera, one capture
per variant; no physical hardware or explicit interrupt/ABI proof.
The final-pose visual check does not certify every intermediate
raster or command byte. Full per-step command streams and further
consumer tests remain open.

Next independent experiment: B3 arbitrary-weight skinning. Profile
existing rigid, half-weight and general blend paths separately, use
the current optimized C++ general path as comparator, and preserve
rounding and saturation semantics. Keep B1/B2 default in the A/B to
avoid attributing unrelated gains to B3.
