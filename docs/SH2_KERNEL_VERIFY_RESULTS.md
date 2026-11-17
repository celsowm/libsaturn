# SH-2 kernel verification — first target capture

## Provenance

- Source checkout: `756b690da2440c021e9d0cf30a94f5a661c56675`.
- User-supplied capture from macOS, `sh2_kernel_verify-return.zip`.
- Build: GNU Make, sh2eb-elf GCC/G++, -m2 -mb -O2; linked native ELF, no static constructors; boot-memory and disc checks passed.
- Ymir: injection boot, 90 BIOS fields, 600 application fields, no expansion RAM, cache emulation enabled, Master and Slave CCR = 1.
- FRT frequency: 210540 Hz. Probe division timing model: **immediate-result-no-busy-wait**.
- No compiler version, BIOS hash or probe binary hash recorded in the supplied capture. Mednafen subsequently passed (see below); physical hardware pending.

## Correctness

All 3,840 cases passed: 128 cases per entry point, ten entry points, three lanes. Serial Master, parallel Master and parallel Slave completed; Slave-ready = 1, initialization and parallel status = 0. Master SR = 481 (0x1E1), Slave SR = 241 (0xF1); SR.S clear in each. Misaligned accesses: Master 0, Slave 0. Both Python tests passed.

## Inclusive diagnostic FRT sums

| Entry point | Serial asm/ref | Parallel Master asm/ref | Parallel Slave asm/ref | Serial reduction |
|---|---:|---:|---:|---:|
| rigid | 3865 / 18478 | 3869 / 18481 | 3871 / 18510 | 79.08% |
| blend | 5976 / 20924 | 5964 / 20920 | 5975 / 20921 | 71.44% |
| clip_joints | 1110 / 2316 | 1116 / 2316 | 1114 / 2315 | 52.07% |
| paint_range | 420 / 440 | 427 / 436 | 426 / 441 | 4.55% |
| paint_spread | 512 / 644 | 512 / 648 | 512 / 653 | 20.50% |
| paint_quantise | 664 / 629 | 656 / 635 | 670 / 633 | -5.56% |
| bounds | 621 / 854 | 616 / 858 | 621 / 850 | 27.28% |
| screen_cull | 1904 / 4075 | 1903 / 4075 | 1898 / 4064 | 53.28% |
| program_run | 1543 / 1931 | 1551 / 1930 | 1543 / 1923 | 20.09% |
| adx | 1095 / 3788 | 1096 / 3789 | 1103 / 3803 | 71.09% |

These are inclusive sums, not isolated performance qualification. The projection oracle deliberately uses software 64-bit division; it is not the optimized C++ production baseline. The Ymir timing model does not simulate divider busy waits, so no hardware divider-overlap speedup can be inferred. Cases are small, synthetic, and timed individually; no percentile or cold-cache measurement is present. The quantise result warrants a matched batch benchmark with realistic distributions and repeated samples, but does not justify reverting the existing historically beneficial implementation.

## Next evidence

- [x] Initial differential capture on real target instruction execution in Ymir, both CPUs.
- [x] Repeat independently on Mednafen via native CD boot.
- [ ] Repeat on physical hardware.
- [ ] Add explicit ABI/register sentinels, forced interrupt tests and wider boundary coverage.
- [ ] Capture matched production baseline, especially static projection and painter distribution; keep actual production optimized C++ distinct from the independent oracle.
- [ ] Measure realistic batch sizes, occupancy and skip distributions, paired repeat order, timing overhead and code/cache footprints.
- [ ] Record toolchain/probe identity and exact build flags; compare divider latency probe against independent emulator/hardware before interpreting projection timing.

No production optimization was introduced by this capture. Phase A remains partially complete.


## Independent Mednafen capture (native CD)

- Checkout: `40588f2be1626eae76a563032abff5e322b79445`; launcher fix applies settings before the disc filename.
- Mednafen 1.32.1 on macOS, native CD boot, isolated profile, neutral input, Software SDL, data-only cache emulation, backup-memory cartridge, NTSC region 0x4.
- Unattended 30-second run; SIGINT exit returned 0 and produced one 431,233-byte autosave. WRAM-H byte order was swapped16 and verified against the built ELF .text.
- ELF SHA-256: `b0c6f4176de9c9b1a80c15342a73dc70e09398536257473357073a48b79bc391`.
- BIOS SHA-256: `96e106f740ab448cf89f0dd49dfbac7fe5391cb6bd6e14ad5e3061c13330266f`.
- Mednafen executable SHA-256: `0e246709060695d1a7451645c115c3e863aabc245bb7023e933e49d46d52860a`.
- Disc SHA-256: `020a849951ff1d1d4e7f2f0d0736e463deec1f525863903134be8d7b508b1209`.
- Telemetry binary (704 bytes) independently decoded and matched the JSON for every field and row. All 3,840 cases passed; all lanes completed; init and parallel status 0, Slave-ready 1. FRT 210540 Hz, Master SR 481, Slave SR 241, both CCR 1. No Mednafen misalignment counter was captured.

| Entry point | Serial asm/ref | Parallel Master asm/ref | Parallel Slave asm/ref | Serial reduction |
|---|---:|---:|---:|---:|
| rigid | 3287 / 19140 | 3305 / 19300 | 3332 / 19245 | 82.83% |
| blend | 5280 / 21431 | 5327 / 21576 | 5328 / 21551 | 75.36% |
| clip_joints | 1027 / 2397 | 1023 / 2399 | 1039 / 2402 | 57.15% |
| paint_range | 494 / 507 | 501 / 508 | 499 / 518 | 2.56% |
| paint_spread | 536 / 684 | 527 / 689 | 529 / 679 | 21.64% |
| paint_quantise | 639 / 631 | 631 / 620 | 644 / 631 | -1.27% |
| bounds | 668 / 895 | 662 / 896 | 679 / 897 | 25.36% |
| screen_cull | 2243 / 4057 | 2249 / 4065 | 2228 / 4067 | 44.71% |
| program_run | 1794 / 2014 | 1805 / 2025 | 1825 / 2018 | 10.92% |
| adx | 1181 / 3781 | 1193 / 3773 | 1197 / 3783 | 68.76% |

The direction of all ten serial timing comparisons agrees across both emulators: nine reductions and a small quantise regression. This is evidence for the diagnostic workloads only. CPU/cache models differ, the original Ymir capture did not record an ELF hash, and the independent projection/ADX oracles are not the optimized production paths. Do not treat cross-emulator differences as hardware speedups. A total of 7,680 case executions passed across both runs (not 7,680 distinct test vectors). Physical hardware, forced interrupts, explicit ABI sentinels and production A/B benchmarks remain pending.
