# SCU DSP lighting alongside both SH-2s

The experiment tests whether giving the SCU DSP a share of the lighting work
makes the complete frame faster than using both SH-2s. It compares equal work
on identical inputs, first as a batch and then in a rendered scene.

## Build and reproduce

On macOS, run `./scripts/qualify-heterogeneous-3d.py`. The script runs
`git pull --ff-only`, builds both examples with the project wrappers, runs
all host tests and checks the legacy DSP example in Ymir. It then runs the
new scene with Ymir injection and with a native Mednafen CD boot. It needs the
existing SH-2 toolchain, GNU Make, FFmpeg, a caller-owned Saturn BIOS and
Mednafen. The project harness builds its pinned Ymir probe if necessary.
Use `--bios` and `LIBSATURN_TOOLCHAIN_ROOT` for non-default paths.
The output directory and ZIP contain the compiler and emulator versions,
Git revision and checkout status, binary hashes, ELF/size/map files, FRT
metrics, screenshots, visual diffs and logs. The BIOS and the temporary
Mednafen movie and profile are excluded.

In `heterogeneous_3d_bench`, auto qualification replays the same 120 poses
in five schedules. The sustained run then alternates between the fastest
tested dual and three-processor scene schedules with a resident PCM8 stream.
A selects Master only, B selects dual SH-2 and C selects both SH-2s plus DSP.
The HUD reports raw FRT ticks, swap requests per nominal second and polygon
counts. Mednafen's estimates are from the same guest counters, recovered from
its exit autosave after matching the loaded .text bytes to the retained ELF.

## What was changed

The stock [DSP API](../include/saturn/scu_dsp.h) and its HAL implement CPU
ports, DSP DMA and two resident 3×3 transform entry points. The stock
transform starts afresh every 21 vertices and returns three words per vertex.
The renderer already has SH-2 `mac.l` skinning, joint composition and fused
projection kernels. The per-face path covers projection, clipping, culling
and painter ordering. The Slave executor supplies bounded submission, wait,
cache publication and abort. The experiment uses these APIs directly. Its
DSP program and scheduling policy are local to the example.

Tekken's previous visibility predicate was exact on its bounded domain.
Its 197-word program used tiled indexed geometry and returned a visibility
mask. Mednafen CD boot at Release `-O3` measured roughly 45 FRT ticks per
tile on SH-2 and 113 including DSP packing and transport. Tekken's
`WORLD_COMPOSITION_EXPERIMENT.md`, `BRANCH_INPUT_FILTER.md`,
`SATURN_PERFORMANCE_PLAN.md` and `src/saturn/dsp_visibility.dsp`
also describe why the original per-actor scheduling left work on the
critical path. Lighting here has a fixed parameter set, contiguous input and
one output word per vector. The SCU DSP advances the DMA addresses and
processes its whole assigned range per start. [Tekken Saturn](https://github.com/celsowm/tekken3-saturn)

## DSP and memory behavior

- The SCU DSP has 256 32-bit program words, four 64-word data banks,
  and a nominal 14 MHz internal X/Y path when the SH-2 is at 28 MHz. An
  arithmetic word can express ALU, X, Y and D1 operations in parallel.
  `AD2` combines 48-bit operands. Multiplication contributes the low 48 bits
  of the signed 32×32 product. [SCU manual, register model](sega_saturn_hardware/hard/scu_/hon/p04_01.md), [Sega SCU User's Manual](https://antime.kapsi.fi/sega/files/ST-097-R5-072694.pdf)
- The kernel uses M0 for the three light components, M1 for 48 interleaved
  normal words, M2 for 16 results and M3 for four DMA parameters. A final
  accumulation can both store M2 and preload X/Y from M0/M1. The next dot
  gets the new X/Y on the following word. The second output store uses the
  prefetched BTM delay slot. This schedules nine words per pair inside the
  loop plus one initial preload per tile. The final speculative read stays
  within M1. CT0 is rewound on a word without an M0 read and the M3 count
  is read, decremented and written on separate words. [SCU operand execution](sega_saturn_hardware/hard/scu_/hon/p04_03.md), [kernel](../examples/heterogeneous_3d_bench/lighting.dsp)
- DMA2 uses a RA0/WA0 word address and transfers 48 words in and 16 out.
  Both emulators advance the High-WRAM address by four bytes per transferred
  word in this mode. The manual describes increments in bus-dependent units
  and describes different A/B bus restrictions. The DSP joins on T0 before
  reusing M1 or M2 and overlaps the M3 bookkeeping with pending output DMA.
  ENDI signals once after the last tile. The Master still waits for EX/T0
  to clear before consumption. Clearing EX can leave a DMA outstanding.
  The experiment bounds waits and stops on an error before reusing buffers.
  [SCU DMA instructions](sega_saturn_hardware/hard/scu_/hon/p04_56.md), [SCU END/ENDI](sega_saturn_hardware/hard/scu_/hon/p04_59.md)
- Pins and bus routes matter. In the SDK's qualified routes, DSP DMA uses
  High WRAM. Low WRAM is not a substitute for these DMA buffers. DSP input
  lands through the SH-2 write-through cache. The Slave purges before reading
  new normals and the Master invalidates the separate output spans after
  their writers finish. The scene also uploads 2 KiB per frame from High
  WRAM to the inactive VDP1 texture while the previous list can be drawing.
  The SDK's frame submission settles the preceding swap before publishing
  the next list. Two textures suffice for this ownership protocol. VDP2
  composites the background independently. [SH7604 cache section](sega_saturn_hardware/manuals/sh7604/12-section-8-cache.md), [SCU DMA routes](../src/hal/scu/dma_logic.hpp)
- The two lighting arrays use 32 KiB of High WRAM at the maximum 2,048
  vectors. DSP payload is 16 bytes per assigned normal. At 192 normals,
  this is 3 KiB per frame in addition to the 2 KiB SCU DMA texture upload
  and CPU traffic for geometry and command emission. A single PCM8 stress
  ring adds 16 KiB plus a 1 KiB source period. These are payload sizes.
  Shared-bus utilization and worst-case arbitration require hardware
  measurement. The emulator profiler here measures elapsed work and wait
  times.
- The pinned Ymir core halves incoming system cycles before its DSP loop.
  Its DMA implementation checks bus waits and explicitly leaves transfer
  cycle accounting incomplete. Mednafen advances a DSP cycle counter,
  charges two base units per instruction and models DMA completion through
  a separate T0 deadline. The host simulator validates instructions, flags
  and data flow. Emulator agreement is evidence for arithmetic and routing.
  Physical bus contention and timing remain open. [Pinned Ymir core](https://github.com/ymir-emu/Ymir/blob/5c571f680e3606de08813af4de43dc00e90cc6e3/libs/ymir-core/src/ymir/hw/scu/scu_dsp.cpp), [Mednafen 1.32.1 source release](https://mednafen.github.io/releases/)

## Benchmark methodology

All competing application paths use the same binary and compile with
`-O3`. They share the normal Makefile runtime, including its documented
translation-unit overrides. The CPU baseline includes both optimized C
and assembly. The assembly uses three signed `mac.l` operations and
extracts MACL. An independent signed 64-bit reference checks every result
after measurement. The signed Q10 input magnitudes are at most 1,024.
With the direction `(383, -619, 728)`, the largest absolute dot is bounded
by 1,771,520, in signed 32 bits. The scene rounds normals toward zero on
conversion to Q10, keeps the exact dot and truncates positive intensities
only during the common Gouraud conversion.

The 16–2,048 sweep rotates backend order over 16 timed repeats and one
warm-up. It measures Master C and MAC, two-SH-2 C and MAC, CPU-port DSP,
streaming DSP, concurrent Master and streaming DSP, and three splits of
concurrent Master, Slave and DSP. The 1/8, 1/4 and 3/8 tile assignments
round to 16-normal boundaries. A 16- or 32-normal three-way candidate
reduces to dual SH-2. Results include sum, minimum and maximum FRT ticks,
CPU work windows, transport through CPU ports, dispatch and join. DSP
elapsed spans include launch and DSP DMA. IRQ timestamps are used when
observed, with the bounded wait completion as a fallback observation.

The scene processes 512 normals across four articulated characters and
32 checkerboard terrain polygons. It carries the same camera and joint pose
sequence through every schedule. LibSaturn evaluates the skeleton hierarchy.
The scene transforms rigidly attached geometry on the Master, uses CPU
backface rejection and LibSaturn's projected-face rejection/clipping, and
consumes lighting directly into VDP1 Gouraud entries. Character geometry
preparation, the VDP1 texture upload and lighting may overlap. The two extra
schedules move all SH-2 lighting to the Slave, leaving geometry preparation
on the Master to avoid loading it with half the lighting. The weighted
three-processor schedule also gives the DSP 3/8 of the normals.

Scene `active` is FRT from pose start through HUD generation, before the
submission call. `ready` includes submitting the list, which can wait for
the preceding swap. `latency` is pose start to the VDP1 swap request.
`dma_elapsed` spans texture DMA launch to IRQ or completion observation.
`dma_wait` is the remaining wait after other work. Scope these counters to
the measurements described here. The cropped Ymir captures exclude the HUD
and compare the same pose. Its field observations count changes to the world
pixels separately from the guest swap counts.

## Results

The three-processor schedule reduced the 2,048-normal batch by 30.8% in
Ymir and 21.8% in Mednafen relative to the faster dual-SH-2 implementation.
At 512 normals it saved 10.6% and 6.7% in the isolated sweep. Once the
renderer was involved, the strongest dual-SH-2 schedule already hid its
lighting behind the Master's geometry work. The scene remained at the
same display cadence. This evidence supports retaining the optimized
SH-2 renderer as the production baseline.

Mean end-to-end FRT ticks per batch, including dispatch, transfer and
join. The triple column uses the best of the three splits per size.
The two smallest cases have no DSP assignment.

| Normals | Ymir dual MAC | Ymir triple | Mednafen dual MAC | Mednafen triple |
| ---: | ---: | ---: | ---: | ---: |
| 16 | 32.8 | 32.6 | 44.9 | 48.0 |
| 32 | 34.0 | 34.0 | 58.6 | 62.0 |
| 64 | 36.4 | 42.0 | 65.2 | 52.8 |
| 128 | 41.8 | 44.9 | 72.5 | 58.6 |
| 256 | 51.6 | 52.0 | 69.3 | 71.0 |
| 512 | 72.1 | 64.5 | 92.8 | 86.5 |
| 1,024 | 129.9 | 91.5 | 140.7 | 120.1 |
| 2,048 | 211.6 | 146.4 | 232.6 | 181.9 |

Across 2,048 normals, the CPU-port DSP took 1,556.3 and 2,023.0 ticks,
including 996.1 and 1,303.1 ticks for port transfers and copies. The
streaming DSP alone took 192.1 and 312.3. The best three-way split took at
most 150 and 185 ticks over the timed repeats. The dual MAC maxima were
213 and 236. Mednafen's streamed DSP alone was slower than the dual CPU
baseline even though giving it a fraction of the job improved the batch.

Frame times below are means in FRT ticks across 120 submissions per backend.
Scene windows included normal command generation and the asynchronous 2 KiB
texture transfer. The calibrated FRT rates were 210,540 Hz in Ymir and
210,600 Hz in Mednafen. Worst submission times are the observed emulator maxima.

| Emulator and schedule | Active | Ready | Worst ready | Swap latency | Swaps/s |
| --- | ---: | ---: | ---: | ---: | ---: |
| Ymir Master MAC | 2,739.58 | 3,425.28 | 3,453 | 6,391.92 | 60.0 |
| Ymir dual MAC | 2,733.97 | 3,425.25 | 3,453 | 6,392.25 | 60.0 |
| Ymir triple, equal CPU shares | 2,726.03 | 3,425.22 | 3,453 | 6,392.52 | 60.0 |
| Ymir dual, lighting on Slave | 2,698.22 | 3,424.99 | 3,453 | 6,393.75 | 60.0 |
| Ymir triple, lighting on Slave and DSP | 2,703.97 | 3,425.13 | 3,453 | 6,393.67 | 60.0 |
| Mednafen Master MAC | 3,118.20 | 3,367.91 | 4,375 | 6,367.00 | 59.5 |
| Mednafen dual MAC | 3,123.38 | 3,367.92 | 4,371 | 6,367.06 | 59.5 |
| Mednafen triple, equal CPU shares | 3,116.15 | 3,367.97 | 4,379 | 6,367.11 | 59.5 |
| Mednafen dual, lighting on Slave | 3,081.92 | 3,367.85 | 4,411 | 6,367.27 | 59.5 |
| Mednafen triple, lighting on Slave and DSP | 3,089.74 | 3,367.83 | 4,403 | 6,367.03 | 59.5 |

The five world crops matched at every compared pixel in Ymir, and all
five interior 80-field windows contained 80 world-image changes. The
guest reported zero arithmetic mismatches in both emulators. The PCM8
stress recorded 35 refills over 381 further render iterations in Ymir
and 98 over 1,081 in Mednafen, with zero underruns in both. This is a
RAM-fed audio test. CD-streaming, SAROO, physical Saturn timing and bus
arbitration still require qualification. The macOS host suite reports
the existing fixed-address parallel-completion ABI test as unavailable.

The linked executable occupies approximately 199 KiB of High WRAM
for .text, .data and .bss, plus 13.2 KiB of linked Low WRAM and the
runtime's working regions. These figures are from the ELF section table
and exclude VDP and Sound RAM.

## Rejected and deferred paths

1. A per-tile CPU-port handoff pays packing, register access, a start and
   a join for every 16 outputs. Keep it as a transport control.
2. Reusing Tekken's tiled visibility and the stock 21-vector transform
   would retain the launch and packing cost already observed in those
   studies. Matrix parameters and shared vertex reuse need an independent
   design before they can compete with the fused SH-2 renderer and skinning.
3. Immediate waits after each DSP start are useful as an isolated throughput
   control. The scene submits independent work and joins after processing
   camera and geometry work.
4. A VBlank wait before building every command list cut early runs to about
   30 swaps per second in every mode. Preparing while the previous VDP1 list
   runs and relying on the SDK's submission ownership restored the cadence.
5. Triple buffering is deferred. The experiment uses one same-frame lighting
   result and two VDP1 texture banks. An extra frame of buffering would need
   to earn its latency.
6. Prioritize a real Saturn run of the same image, with bus pressure and
   streamed audio checks, before selecting a backend. Then test a transform
   kernel that retains parameters across larger work ranges and replaces a
   meaningful part of the existing fused CPU path. Cross products, weighted
   skinning, interpolation and spatial queries remain unbenchmarked here.
   No public scheduler API or game integration is promoted on these results.
