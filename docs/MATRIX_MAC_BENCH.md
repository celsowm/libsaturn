# Exact SH-2 matrix multiply experiment

`examples/matrix_mac_bench` compares signed 3×3 Q12 multiplication using four
kernels. It has no external assets, imported poses or game rules. The ordinary
SDK math and renderer implementations are unchanged. This is an experiment,
not a public matrix API or a consumer performance qualification.

| Kernel | Accumulation | Final conversion |
| --- | --- | --- |
| C64 | Three full-width C products and a 64-bit sum | Floor shift by 12, then clamp to signed 16 bits |
| MAC64 | Three `MAC.W` operations and the full MACH:MACL result | Same conversion |
| C32 | Same full-width C accumulation | Compare wide sum against output bounds before narrowing and shifting |
| MAC32 | Same full-width MAC accumulation | Same bounded conversion as C32 |

C32/MAC32 do **not** narrow the products or their sum prematurely. Values above
`32767 * 4096` return 32767; values below `-32768 * 4096` return -32768. Every
remaining value fits signed 32 bits and can use `sat_floor_shift_i32`. This
identity applies to this saturating Q12 output; it is not a replacement for a
wide shift whose caller needs an unrestricted result.

## Arithmetic and processor contract

The local [SH-1/SH-2 programming manual, section 6.31](sega_saturn_hardware/manuals/sh1-sh2-programming/06-section-6-instruction-descriptions.md)
specifies that SH-2 `MAC.W` accumulates into 64 bits only with SR.S cleared.
With SR.S set it saturates at 32 bits, which changes these results. Each matrix
MAC kernel saves SR, clears S and restores SR before returning. It includes
transposing the right operand, all nine dots, rounding, saturation and an
alias-safe temporary result. Transposition stays inside each timed call.
The inline assembly declares MACH, MACL and memory clobbers and distinct,
early-clobbered postincrement pointer operands.

MACH/MACL must survive interrupts. The SDK's SCU wrappers use GCC's
`interrupt_handler` attribute. The tested ELF's `sat_scu_isr_0` disassembly saves
both with `sts.l` on entry and restores both with `lds.l` before `rte`; other
interrupt environments need their own proof. The benchmark keeps interrupts
enabled, tests incoming S=0 and S=1, and checks S restoration. It does not prove
an arbitrary application handler or another runtime safe.

A deterministic 1024-input corpus contains signed extrema, an identity/sign
case and full-range pseudorandom matrices. There are 9216 raw dot comparisons
against wide C sums, including sums outside signed 32 bits, and 24576 matrix
comparisons across four kernels, two S settings and three output alias modes.
An independent division/remainder oracle supplies negative-floor and clamp
results; output guards and exact checksums are also checked. The required
completion count is **33792**, with zero errors. This is a sampled corpus,
not exhaustive input coverage.

## Reproduction

From the SDK root, with the SH-2 toolchain and built Ymir harness available:

```sh
python3 harness/scripts/matrix_mac_bench.py \
  --bios /path/to/saturn_101a_us.bin \
  --out build/diagnostics/local-matrix-o3 \
  --optimization O3 --mednafen-seconds 30
```

The default is O2. `--optimization O3` uses Make's example compile flags and
retains the exact command; `--cache off` selects a separate Ymir cache control.
Use fresh output directories. Avoid another build of this example while the
runner copies its exported images on systems without Make's `flock` support.

The runner preserves ELF/map/CD images, build/probe commands and logs, source,
compiler, BIOS/probe/ELF fingerprints, panic dump, raw telemetry, screenshot and
validated `analysis.json`. It rejects incomplete arithmetic checks, wrong case
order, zero/unbounded clock intervals, panic and differing kernel checksums.
Decoder corruption tests run through the ordinary `make test` tools suite.

Timing uses five local extended FRT intervals per case. Each interval contains
64 single-matrix calls or eight 18-link chains, including function dispatch,
input/result copying, checksum work and interrupts. Reported microseconds per
matrix divide the complete interval by its matrix count; these are not isolated
instruction timings. Both original inputs can reside in High or Low WRAM;
chain accumulators, transpose and output temporaries stay on the Master stack.
Timed inputs have Q12-scale coefficients; qualification uses full signed range.
The final timed checksums must agree; intermediate chain links are not separately
recorded by the timing loop.

Mednafen boots the actual CD. Its recording and final image are retained;
inspect **CHECKS**, **ERRORS** and **FRAME**. A successful process exit alone
does not establish completion. HUD rows show raw median FRT ticks for High-WRAM
18-link batches, not microseconds or FPS. The runner does not automatically
extract numeric telemetry from the Mednafen image.

## Recorded result: 2026-10-19

SDK base `f11b277`, GCC 13.2.0, normal cache enabled, pinned Ymir harness,
600 application fields after 90 BIOS fields. Final retained artifacts are
`build/diagnostics/matrix-mac-final-o3`; O2 control is
`build/diagnostics/matrix-mac-narrow`. All 33792 checks pass in each capture.
The final O3 Mednafen recording also displays CHECKS 33792, ERRORS 0 and FRAME 974.

| O3 kernel, High inputs | Single, µs/matrix | 18-link chain, µs/matrix | Chain reduction vs C64 |
| --- | ---: | ---: | ---: |
| C64 | 39.779 | 33.182 | — |
| MAC64 | 37.775 | 31.170 | 6.06% |
| C32 | 38.591 | 31.665 | 4.57% |
| MAC32 | 36.142 | 29.389 | 11.43% |

The final Mednafen image shows raw chain-batch medians 954, 908, 922 and 846
respectively: the combined candidate improves that procedural interval by
11.32%. The earlier O2 Mednafen control shows 960, 957, 926 and 902, while Ymir
reports a 9.58% combined O2 chain reduction. Compiler optimization and emulator
choice matter; do not substitute one control's numbers for another.

These results do not justify replacing the port's authoritative arithmetic or
adding an assembly-backed public API yet. They fall below the selected 20%
stage recovery target, and the measured port matrix routine occupies only a
small exclusive fraction of the whole path. No release Paul/Dino speedup or
physical Saturn timing is claimed. Keep this procedural control and measure
complete callers before promoting either arithmetic variant. Ymir's known
immediate-result DIVU model remains a general timing limitation; this MAC
kernel does not issue hardware divisions.
