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
