# C1 V2 — full painter order

Preserve production default C++ prefix and wide scatter. Mode 1
selects native byte scatter only; mode 2 additionally fuses the
equal-live-key case into one pass, computes grouped bucket shares
using bounded 32-bit arithmetic, and avoids clearing/summing unused
trailing grouped buckets. All modes use the same existing range and
spread kernels; no API dispatch change. The 32-bit share proof depends
on the existing public precondition count <= 65535.

Compare full plain and grouped sort entry points in one binary, with
the same input, rotated call order and independent mapping oracle.
24 scenarios x 32 repetitions x three lanes x two emulators. Include
1/2/4/8/9 groups, interleaved and contiguous passes, group shifts 1
and 31, equal/sparse/empty, byte and wide orders, unsigned extremes
and workloads up to 2048 faces. Verify every key, order slot, untouched
tail and guard value, and live count. Timing includes all sort stages
but not input generation or oracle. No scene rendering/VDP1 bytes
or full 65535-face test yet; those remain later qualification gates.
No new default activation.

Run `python3 harness/scripts/run_paint_order.py`.
Return `harness/build/paint_order_bench-return.zip` with both target
reports, Ymir screenshot, and local changes.

## V2 target results — 2026-11-17

Checkout `70871106ac8b1a2c62c8fbd9980eec359bb22eb7`.
Both 2432-byte big-endian telemetry reports independently decoded;
every header, lane and scenario field matches JSON. Separate Mednafen
ZIP duplicates the combined archive's Mednafen entries byte for byte.
Complete local source manifest; empty tracked, staged and unstaged
patches; no nonignored untracked files.

All six lanes completed: 4608 cases, 6015936 input faces and 5083968
live faces per implementation, zero recorded errors. Parallel
initialization and execution status zero, slave available, CCR 1,
SR.S clear on entry. No additional edge checks in this report (edge
counter zero). Ymir target unittest passed, zero reported misaligned
accesses; FRT 210540 Hz. Mednafen native CD pass, return code 0,
swapped16 normalized WRAM; FRT 210600 Hz; ELF SHA256
`0bb25892cdcfdb14cb96605831ccc6f5276cb48f9f0aa37a66c696165fdee7c7`.
No synthetic explicit panic dump or matching Ymir ELF identity.

### Complete sort, serial Master sums (32 calls)

| Scenario | Ymir control / byte / V2 ticks | Mednafen control / byte / V2 ticks | V2 reduction Ymir / Mednafen |
|---|---:|---:|---:|
| singleton | 70 / 72 / 33 | 108 / 103 / 50 | 52.86% / 53.70% |
| byte_small_span | 3327 / 3269 / 3266 | 3325 / 3264 / 3276 | 1.83% / 1.47% |
| equal | 6625 / 6627 / 3897 | 6463 / 6469 / 3852 | 41.18% / 40.40% |
| robot_1800 | 28517 / 28517 / 28513 | 29222 / 29213 / 29204 | 0.01% / 0.06% |
| dense_random | 30459 / 30460 / 30460 | 31497 / 31434 / 31483 | 0.00% / 0.04% |
| sparse_random | 20479 / 20478 / 20478 | 23129 / 23197 / 23111 | 0.00% / 0.08% |
| byte_sparse | 4038 / 4006 / 4003 | 4139 / 4114 / 4115 | 0.87% / 0.58% |
| two_live | 15826 / 15831 / 10532 | 18197 / 18193 / 12094 | 33.45% / 33.54% |
| two_pass_runs | 82634 / 82638 / 82556 | 81379 / 81376 / 82153 | 0.09% / -0.95% |
| four_pass_interleaved | 117322 / 117328 / 116659 | 114907 / 114907 / 115466 | 0.57% / -0.49% |
| eight_pass | 140520 / 140518 / 139698 | 138137 / 138132 / 138593 | 0.58% / -0.33% |
| nine_pass_fallback | 31007 / 31007 / 31003 | 31588 / 31592 / 31587 | 0.01% / 0.00% |
| byte_grouped | 17461 / 17400 / 17247 | 17152 / 17074 / 16735 | 1.23% / 2.43% |
| mixed_skip_grouped | 98757 / 98751 / 98180 | 97544 / 97546 / 97805 | 0.58% / -0.27% |
| group_equal_sparse | 2656 / 2628 / 1626 | 2708 / 2691 / 1742 | 38.78% / 35.67% |

Equal workload reductions across lanes 41.08–41.18% (Ymir) and
40.33–40.40% (Mednafen); sparse two-live equal keys 33.44–33.45%
and 33.54–33.57%. Singleton absolute savings are only about 5.5–8.6
microseconds per invocation for serial master. Empty calls have
small absolute regressions (3/6 total ticks per 32 calls).
Byte-only mode saves 1.74%/1.83% on the small-span workload and
0.79%/0.60% on byte-sparse serial master. For wide queues with
varied keys neither candidate produces meaningful gains. Mode 2
combines grouped-share arithmetic and trailing bucket trimming;
this experiment does not isolate their costs individually.

Summing raw complete-sort times across equally repeated synthetic
scenarios within each lane, V2 reduces time by 1.435–1.440% on
Ymir and 0.917–0.925% on Mednafen. Byte-only mode is effectively
neutral in that mix. This is not representative weighting by any
game's actual sort invocations or a frame-time prediction.

**Disposition:** exactness PASS within the 2048-face synthetic
scope. Retain equal-key fused path for separate qualification;
byte scatter is a low-priority optional candidate. Do not adopt
the combined grouped changes on this evidence. Measure live-key
equality incidence in representative consumers before investing
in real-frame A/B; if rare, prioritize C2 clipping. Preserve
C++ prefix and wide scatter. Still require 65535-face boundaries,
complete VDP1 command-stream equivalence, real scene raster checks,
explicit IRQ/ABI proof and physical hardware qualification before
any default activation.
