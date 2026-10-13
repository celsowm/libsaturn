# Paired character geometry preparation

The character examples now prepare consecutive actor pairs with the existing
scene geometry adapter: Master prepares the first actor while Slave prepares
the second. Matched Ymir measurements increase two-HIGH-actor AUTO and SLAVE
submission throughput from approximately 15 to 20/s. Simulation remains 60 Hz.
Geometry, textures, animation, LOD and camera are unchanged. This follows the
[handoff study](CHARACTER_HANDOFF_PERFORMANCE.md), which retained CPU command
uploads after SCU-DMA proved slower for this workload.

## Ownership and ordering

All skin jobs finish before geometry starts. Each actor has its own projected
and world scratch buffers. Placement, materials and shadows are prepared on
Master before dispatch. The two actors share only immutable source data during
their geometry jobs. The existing adapter publishes nested inputs and consumes
worker output through its normal cache and executor ownership rules.

A single reusable 401-face buffer and matching key array hold the second
actor's output in WRAM-L. The first actor enters the canonical scene directly;
the second is joined and merged afterward, preserving actor and face order.
The next pair reuses the buffer only after release. Shadows retain their own
pass, and all character faces retain their common pass. One global painter
sort and VDP1 emission run on Master. No worker draws or writes VDP registers.

One actor, an unavailable Slave, or MASTER policy uses serial preparation.
AUTO and SLAVE both use paired geometry; SLAVE still selects serial Slave
dispatch for the skin jobs. No second executor, worker owner or renderer is
introduced. Command-upload and framebuffer defaults remain unchanged.

The example compares the prepared actor with canonical serial submission on
the first occurrence of each CPU/LOD/material combination. It checks face
counts, culling/clipping counts, painter keys, materials, projected coordinates,
Gouraud values and the world coordinates used by clipping fallback. Unused
world fields and padding are excluded. Resource reload clears the verification
mask. `CHARACTER_DEMO_VERIFY_GEOMETRY=1` repeats the comparison every paired
frame for a dedicated correctness gate; verification cost is explicitly
reported and excluded from steady performance samples.

## Executor completion fix

Continuous verification exposed an existing completion race. Slave writes a
task's terminal slot before publishing its completion mailbox message. Master
could observe that slot, read/release it and reuse its token before consuming
the message, making a later completion fail token validation.

Active Slave slots now remain publicly RUNNING/BUSY until their single-task or
batch completion message is consumed. Result reads, release and dependent
dispatch respect that boundary. Abort can stop an active task after its early
terminal write, then reclaim its storage safely. Cache consumption still occurs
at acknowledged completion.

`test_parallel_completion` runs the real executor with a hardware-mailbox stub
that deliberately separates the terminal write from message publication. It
covers early success, early failure, a Master dependency, a multi-task batch,
slot reuse and abort. The new test fails against the preceding executor and
passes with the fix. Continuous material and collision gates that exposed the
race pass after the fix.

## Matched Ymir results

Both builds use the corrected executor and the same 1,400-field input script.
Four transition samples are discarded; no steady sample contains verification
work. The serial build disables only paired geometry. All actors are accepted
at HIGH and serial/worker equivalence passes.

| Policy | Serial / paired CPU | Serial / paired preparation | Serial / paired submissions/s | Paired frame median / p95 |
| --- | --- | --- | --- | --- |
| AUTO | 50 / 41 ms | 22 / 13 ms | 15.08 / 20.00 | 50 / 52 ms |
| MASTER | 54 / 54 ms | 22 / 22 ms | 15.00 / 15.00 | 67 / 69 ms |
| SLAVE | 54 / 45 ms | 22 / 13 ms | 15.00 / 20.00 | 50 / 52 ms |

AUTO CPU cost drops approximately 18%. The serial preparation metric now times
the complete actor loop once, rather than adding individually rounded actor
milliseconds; use the matched serial build above for comparison with 13 ms.
The previous study's 23 ms is historical. Paired steady sample counts are
129/96/162; serial counts are 97/96/120. Small command/pose differences reflect
elapsed-time sampling, rather than geometry or face-admission changes.

The paired FRT geometry medians include publication, projection/preparation,
waiting, merge and release. Stage medians are not an additive decomposition of
the total, and Master/Slave preparation overlaps.

| Policy | Total geometry | Dispatch/publication | Master preparation | Residual wait | Merge |
| --- | --- | --- | --- | --- | --- |
| AUTO | 13.759 ms | 0.460 ms | 11.261 ms | 1.505 ms | 0.408 ms |
| SLAVE | 13.802 ms | 0.455 ms | 11.166 ms | 1.655 ms | 0.408 ms |

Sorting remains approximately 2.8 ms and emission 4.6 ms. These figures do not
separate the worker's projection from its face preparation: the library's
last-instance profiling globals are not independent measurements of concurrent
submissions. Whole-stage FRT intervals are used instead. No new presentation
counter is inferred from display fields; these are submitted loops/s on Ymir,
not physical Saturn timings or a claim of 30/60 rendered frames/s.

The final 3,100-field stress gate accepted all twelve count/CPU combinations
with six geometry equivalence checks, zero errors/panic and 780 stored samples.
These are current absolute results; the historical one-actor/eight-actor
figures predate the earlier vertex-transform optimization and are not a
matched baseline for attributing every change to paired geometry.

| Actors / admitted LOD | AUTO submissions/s | MASTER submissions/s | SLAVE submissions/s |
| --- | --- | --- | --- |
| 1 / HIGH | 30.00 | 30.00 | 30.00 |
| 2 / HIGH | 20.00 | 15.00 | 20.00 |
| 4 / MEDIUM | 12.07 | 10.00 | 12.00 |
| 8 / LOW | 10.00 | 8.57 | 9.98 |

The final varied-clip eight-actor SLAVE interval measured 9.52 submissions/s.
The 1,150-field arena gate passed attacks, guard, hit flashes and asset reloads;
the 750-field collision gate recorded 721 ticks with zero overlaps. The
1,860-field viewer passed all nine clips, lighting and pause pixel equivalence.
The 800-field material gate compared 69 paired frames against the serial path
with zero differences. The complete host/tool gate and all three stock-RAM ISO
builds passed with the corrected executor.

## Reproduction and memory

```powershell
.\harness\run-character-geometry.ps1 -Bios bios/saturn_bios_us.bin -Geometry serial
.\harness\run-character-geometry.ps1 -Bios bios/saturn_bios_us.bin -Geometry paired
.\harness\run-character-plan.ps1 -Bios bios/saturn_bios_us.bin -Scenario stress
.\harness\run-character-plan.ps1 -Bios bios/saturn_bios_us.bin -Scenario materials
```

The study runner copies the actual stress configuration into a temporary
example, selects the geometry build flag and restores its environment and
source directory afterward. It writes matched maps, dumps, logs, captures and
analysis under `harness/build/character_geometry_serial*` and
`character_geometry_paired*`. The `materials` gate enables continuous verification automatically, restores
the caller's environment and ordinary runnable artifact, and exercises indexed/RGB
lighting, CPU policies, zoom and varied clips with continuous equivalence.
Use `CHARACTER_DEMO_PARALLEL_GEOMETRY=0` to build the serial fallback explicitly.

The face/key buffer occupies 41,704 bytes in WRAM-L; item and batch descriptors
add 148 bytes. Family-owned actor/archive/scene/preparation storage is 568,520 B
for stress and 503,564 B for arena. The one-actor viewer does not allocate the
paired buffers. The additional bounded geometry sample log occupies 40,976 B
and is separate from `OWN RAM KB`, as are the existing diagnostic logs.

The stock stress map ends WRAM-L at `0x00282658` and BSS at `0x060f6928`.
It leaves 38,616 bytes above BSS, including the enforced 16 KiB Master stack
reserve. There are no per-frame heap allocations or expansion-RAM requirements.
Physical Saturn and PAL character acceptance remain open.
