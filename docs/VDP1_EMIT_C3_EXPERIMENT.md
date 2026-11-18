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
