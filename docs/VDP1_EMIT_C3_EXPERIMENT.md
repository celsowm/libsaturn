# C3 — full-record VDP1 emission A/B

Generic scalar sprite, scaled, distorted, projected sprite, projected
program, polygon/polyline and line emitters explicitly assign 16
halfwords. Normal sprite batches and compiled program-run are already
specialized and are not modified. The first candidate pairs all
sixteen fields into eight aligned volatile 32-bit writes using the
existing endian-aware pair() and may_alias type. No skipped fields,
no new assembly, no new public API, no change to capacity, validation,
Gouraud patching, END, publication fences or bank selection. Flag
SAT_VDP1_EMIT_EXPERIMENT defaults to 0; 1 is original diagnostic
control, 2 packed. Compiler may already combine some original stores;
inspect disassembly and measure complete calls rather than assuming
a 2x speedup. Existing command format guarantees four-byte alignment.

The new renderer example exercises the actual HAL with real indexed8
texture, signed coordinates, flips, mesh, opaque and user clip.
Six phases: normal, scaled, distorted, projected, precompiled-prefix,
and mixed polygon/polyline/line/Gouraud. Four measured frames per
phase (two positions, repeated twice), each 96 successful timed
commands, precomputed inputs outside timing. Direct HAL timed calls
include dispatch/validation, not projection. After each timed batch
the world allocation reaches its capacity limit via overlay
reservation. Check invalid-width and one further valid sprite
refusals, unchanged checkpoint and unchanged next slot. Enter overlay
pass, append one shaded swatch, capture every live command and Gouraud
word, including final END. Compare full captures and zero tails,
panic, serial and timeouts across both builds and two emulators.
Six executions (Ymir and Mednafen timing/visual for both modes),
144 recorded frames, 13824 measured commands. Per-phase median over
four frames. Final frozen mixed phase screenshots only; intermediate
frames qualified through literal commands. Capture disassembly.

First stage intentionally tests default framebuffer behavior, not
all interlaced/on-submit modes, LUT4, direct RGB textures, text,
native scene batches, worker publication or full capacity 2048
stress. The reserve/refusal check is a logical quota boundary,
not a physical end-of-bank stress. Real asset/scene profiling and
these additional scenarios are follow-ups only if a gain merits it.
No hardware timing or adoption claim. Keep C2 and other experiments
off. Compilation and target execution pending.

Run `python3 harness/scripts/run_vdp1_emit_renderer.py` and return
`harness/build/vdp1_emit_renderer-return.zip`.

## C3 V2 — hybrid full-record emission

V1 at `458dc5e` passed literal command/Gouraud comparison and final
pixel comparison on both emulators. Global emit+END median reduction:
Ymir -2.86%, Mednafen +8.00%. Normal and scaled regress on Ymir; the
three distorted/projected/program phases improve on both. No production
promotion or hardware timing claim.

Mode 3 selects the already-existing packed implementation at compile time
only for `push_distorted_sprite`, `push_projected_sprite` and
`push_projected_program`. All other emitters remain original; mode 2
remains the full packed candidate and mode 0 remains production.
No runtime selection, new stores, fence change or API change.

V2 runner measures all three modes with identical seven-phase scenes.
The extra synthetic mixed 3D phase uses 96 commands in six repeats of
16 primitives: normal 1, scaled 1, distorted 2, projected 4, program 4,
polygon 1, polyline 1, line 1, Gouraud polygon 1. It is a HAL workload,
not a full game frame benchmark; final frozen screenshot now shows this
mixed scene. Retain the six isolated phases and separate isolated
aggregate (same warmup exclusion as V1) as well as all-phase median.
28 measured frames per execution, nine executions, 252 frames and
24,192 timed commands total. The schema version is 2; existing V1
archives stay separate. Compare exact command/Gouraud bytes, rejection,
END, serial, panic, timeouts and final pixels against the control.
Capture compiler and complete SH-2 disassembly per mode. No adoption
before reproducible non-regression on both emulators, then actual scene
profiling and hardware qualification. All other experiments stay off.

Run `python3 harness/scripts/run_vdp1_emit_renderer.py` and return
`harness/build/vdp1_emit_renderer_v2-return.zip`.
