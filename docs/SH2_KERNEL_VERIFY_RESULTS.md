# SH-2 kernel verification — first target capture

## Provenance

- Source checkout: `756b690da2440c021e9d0cf30a94f5a661c56675`.
- User-supplied capture from macOS, `sh2_kernel_verify-return.zip`.
- Build: GNU Make, sh2eb-elf GCC/G++, -m2 -mb -O2; linked native ELF, no static constructors; boot-memory and disc checks passed.
- Ymir: injection boot, 90 BIOS fields, 600 application fields, no expansion RAM, cache emulation enabled, Master and Slave CCR = 1.
- FRT frequency: 210540 Hz. Probe division timing model: **immediate-result-no-busy-wait**.
- No compiler version, BIOS hash or probe binary hash recorded in the supplied capture. Mednafen and physical hardware: pending.

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
- [ ] Repeat independently on Mednafen and ultimately hardware.
- [ ] Add explicit ABI/register sentinels, forced interrupt tests and wider boundary coverage.
- [ ] Capture matched production baseline, especially static projection and painter distribution; keep actual production optimized C++ distinct from the independent oracle.
- [ ] Measure realistic batch sizes, occupancy and skip distributions, paired repeat order, timing overhead and code/cache footprints.
- [ ] Record toolchain/probe identity and exact build flags; compare divider latency probe against independent emulator/hardware before interpreting projection timing.

No production optimization was introduced by this capture. Phase A remains partially complete.
