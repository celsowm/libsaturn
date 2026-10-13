# Character frame handoff performance

This study follows the [preparation optimization](CHARACTER_PREPARATION_PERFORMANCE.md).
It measures the existing VBlank wait and VDP1 submission paths with two HIGH
characters, the walking clip, unchanged textures/camera and 60 Hz simulation.
Command transfer is the only experimental rendering switch. The standard
examples keep their existing CPU-upload and automatic-framebuffer defaults.

## Instrumentation and reproduction

`g_character_handoff` is a bounded companion to the existing benchmark log.
Each row contains the benchmark iteration, calibrated FRT durations around
`sat_wait_vblank` and `sat_end_frame`, cumulative late-swap/draw-wait/timeout
counters, display-field count and VBlank count after the initial wait.
The original benchmark wire format remains unchanged. The analyzer requires
matching row counts/iterations and zero draw timeouts, discards four transition
samples and reports steady-interval medians, p95 values and counter differences.
Archived benchmarks without the companion log remain readable.

```powershell
.\harness\run-character-plan.ps1 -Bios bios/saturn_bios_us.bin -Scenario prepare
.\harness\run-character-handoff.ps1 -Bios bios/saturn_bios_us.bin -Transfer cpu
.\harness\run-character-handoff.ps1 -Bios bios/saturn_bios_us.bin -Transfer dma
```

Each run covers 1,400 NTSC fields, selecting two actors and then AUTO, MASTER
and SLAVE policies. The transfer runner creates a temporary example with the
actual stock stress Makefile, including its eight-actor storage configuration,
and enables the same library profiling. It calls the existing command-DMA
switch before initialization and removes only that temporary source directory
afterward. It does not patch the library or change production example settings.
Maps, dumps, captures, JSON and logs use `harness/build/character_handoff_cpu*`
and `character_handoff_dma*`; the standard scenario uses `character_prepare*`.

## Matched transfer results

| CPU policy | CPU upload median / p95 | DMA upload median / p95 | CPU / DMA submissions per second | CPU / DMA frame p95 |
| --- | --- | --- | --- | --- |
| AUTO | 1.462 / 1.481 ms | 2.560 / 2.588 ms | 15.08 / 15.00 | 68 / 68 ms |
| MASTER | 1.467 / 2.104 ms | 2.579 / 2.662 ms | 15.00 / 15.00 | 68 / 69 ms |
| SLAVE | 1.467 / 1.511 ms | 2.569 / 2.598 ms | 15.00 / 15.00 | 68.05 / 68 ms |

These upload columns measure the complete `sat_end_frame` call, including any
draw wait. Both runs accepted two HIGH actors throughout the steady intervals,
passed serial/worker equivalence and had zero panic/errors/draw timeouts.
CPU upload had 97/96/120 steady samples; DMA had 96/96/120. Simulation remained
about 60 steps/s in both. The CPU wrapper exactly reproduced the normal
example's workload results, including the handoff timings and counter deltas.

DMA added approximately 1.1 ms to the median submission call on Ymir and did
not improve throughput. Its shorter initial VBlank wait compensated for work
already spent submitting; that is not an independent saving. MASTER's frame
p95 also increased by 1 ms. Command/pose counts vary slightly because simulation
samples elapsed time; neither build changes geometry, LOD or material admission.
The CPU transfer remains the production default. Earlier Mednafen measurements
in `saturn/dma.h` describe a different workload and do not override this result.

## Interpretation

`wait_us` includes the ordinary wait for the next VBlank and, where applicable,
the existing swap-request wait. `end_us` includes safe reuse of the previous
command table, upload and submission. Neither duration is an independent VDP1
rasterization measurement. Draw-wait counters indicate CPU stalls for VDP1
completion; they do not imply zero GPU work when unchanged.

The examples use `SAT_VDP1_FRAME_AUTO`. Its `late_swaps` counter does not describe
late automatic draws: that counter applies to pending ON_SUBMIT swaps. Likewise,
`display_frames` counts display fields, including repeated images. Submission
throughput is measured from benchmark iteration/VBlank differences, and is not
claimed as a count of distinct presented images or a physical Saturn result.

The initial normal-example run measured CPU medians of 50 ms AUTO and 54 ms
MASTER/SLAVE, with 22.5–23 ms preparation, about 2.8 ms sorting and 4.6 ms emission.
It spent approximately 15.1 ms AUTO and 10.9–11.0 ms MASTER/SLAVE in the initial
VBlank wait. Submission itself took about 1.46 ms median. AUTO had no draw waits;
MASTER/SLAVE had 10/6 waits over their steady intervals, with no timeouts.
The complete frame remained about 67 ms, or approximately 15 submissions/s.
Waiting for the next field reflects the current CPU budget and synchronization;
removing draw/swap protections is not a justified optimization.

The next experiment should use the existing scene batch/parallel preparation
adapter to partition coarse character geometry work. Keep painter sorting and
command emission on Master, preserve immutable binding and cache ownership,
and measure projection, preparation, merge and complete-frame costs together.
Compare canonical output before enabling any new default. With approximately
23 ms spent preparing two actors, this is a larger measured target than the
1.46 ms submission path. No new 20/30 submissions/s tier has been established.

## Validation and memory

The complete host/tool gate and all three stock-RAM ISO builds passed after
instrumentation. The arena also rendered in Mednafen with `-MednafenCart none`;
the fresh capture is `harness/build/character_handoff_mednafen.png`.
The companion log occupies 32,784 bytes in WRAM-L, outside actor/archive/scene
storage reported by `OWN RAM KB`. The standard stress map ends WRAM-L at
`0x0026e360` and BSS at `0x060f5848`, leaving 42,936 bytes above BSS, including
the enforced 16 KiB Master stack reserve. No new worker queue or allocation
is introduced. PAL and physical Saturn timing acceptance remain open.
