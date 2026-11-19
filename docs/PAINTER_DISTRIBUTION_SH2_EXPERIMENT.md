# C1 V1 — stable painter distribution

The existing range/spread/quantise native routines are unchanged.
The production C++ prefix and stable placement are extracted into
shared reference helpers; no default native dispatch. The native
prefix walks aligned big-endian count pairs backward and writes
descending starts; the native scatter walks ascending face index,
skips UINT32_MAX, zero-extends the 16-bit cursor, advances it, and emits
an 8-bit or 16-bit index. No scratch growth, no key rewriting during
scatter, no MAC/DIVU or SR mode changes in the new kernels.

The target compares the actual reference stages and the experimental
stages in one binary on identical bucket inputs. 16 scenarios, 32
rounds, three lanes, both emulators. Check range, transformed keys,
histogram, starts, final order, cursor arrays, guard/tail bytes, empty
calls, 65535 cumulative prefix count and unsigned keys. Workloads include
8/16-bit boundary, all skipped, equal keys, ordered, dense/sparse,
1800-face size, near VDP1-size 2048, clustered and pass-bit keys.
Pass-bit keys exercise global spread, not the grouped allocation policy.
Full 65535-face scatter and grouped/renderer command-stream equivalence
are later gates.

Time range, spread, prefix and placement separately. Range/spread are
shared measurements; estimated staged pipeline reduction sums them
with each candidate's prefix/placement, not an end-to-end measurement.
Counter clearing, allocation, oracle and comparison are outside timing.
No activation, no FPS/hardware claim. Native instruction/ABI and physical
tests still pending.

Run `python3 harness/scripts/run_paint_distribution.py` and return
`harness/build/paint_distribution_bench-return.zip`.
Includes both emulators and local source changes, even on failure.

## V1 target results

Checkout `1836f151d6fa26d9c9b3008f27134e49f9a8c2a4`. Both
2432-byte big-endian reports independently decoded and all scenario
fields matched JSON. Standalone Mednafen ZIP is identical by member
contents to the Mednafen entries in the combined ZIP. Local source
snapshot complete, no staged/unstaged changes or nonignored untracked
files.

Across six lanes: 3072 cases, 3362112 input faces and 2533248 live
faces per implementation, 714372 counted-prefix words, 30 additional
edge checks, zero reported errors. All lanes completed, parallel
init/status zero, slave ready, CCR 1, SR.S clear on lane entry,
FRT 210540 Hz in both emulators. Ymir completed 6000 fields, target
unittest passed and no misaligned accesses reported on either CPU.
Mednafen 1.32.1 native CD, data-only cache, exit 0, swapped16 WRAM
normalized; ELF SHA256
`71fc469c7a1d33cf586f1cf55dc2e9b465bd10491ce302a06ca1365f244cdc74`.
No separate panic dump, Ymir ELF identity or command-stream comparison.

### Serial Master FRT sums (32 calls per row)

| Workload | Ymir prefix C++ / ASM | Mednafen prefix C++ / ASM | Ymir scatter C++ / ASM | Mednafen scatter C++ / ASM | Estimated staged reduction Ymir / Mednafen |
|---|---:|---:|---:|---:|---:|
| byte_small_span | 53 / 59 | 56 / 64 | 1421 / 1229 | 1417 / 1288 | 5.39% / 3.51% |
| wide_boundary | 524 / 594 | 560 / 648 | 1301 / 1299 | 1295 / 1290 | -1.65% / -1.96% |
| ascending | 2068 / 2350 | 2216 / 2548 | 5224 / 5226 | 5549 / 5519 | -1.73% / -1.74% |
| robot_1800 | 2063 / 2348 | 2211 / 2552 | 9155 / 9159 | 9641 / 9566 | -1.06% / -0.94% |
| dense_random | 2068 / 2349 | 2208 / 2552 | 9785 / 9779 | 10370 / 10294 | -0.94% / -0.88% |
| sparse_random | 2058 / 2337 | 2193 / 2535 | 5941 / 5940 | 6857 / 6859 | -1.42% / -1.55% |
| pass_bits | 1555 / 1768 | 1661 / 1923 | 5179 / 5180 | 5167 / 5162 | -1.35% / -1.64% |
| byte_sparse | 1029 / 1168 | 1095 / 1268 | 1051 / 951 | 1078 / 1016 | -1.04% / -2.73% |
| bucket_edges | 1558 / 1769 | 1661 / 1921 | 10386 / 10394 | 10719 / 10666 | -0.79% / -0.72% |

Across all lanes the byte-small-span scatter improves 13.19–13.55%
(Ymir), 8.26–9.10% (Mednafen); byte-sparse improves 9.44–9.76%
and 5.75–6.35%. Wide scatter gains are negligible on Ymir and at
most 3.01% on Mednafen, with some regressions. Substantial prefix
workloads regress about 13–14% on Ymir and 15–16% on Mednafen.
Unweighted workload mix, summing raw stage ticks across scenarios
within each lane: final two stages regress 1.88–1.92% on Ymir and
1.00–1.97% on Mednafen. Adding measured shared range/spread stages
gives estimated regression 0.77–0.79% and 0.42–0.81% respectively.
These are staged timing estimates, not complete sort/frame times.

**Disposition:** exactness PASS in tested scope; reject the V1
combined native dispatch for performance. Keep C++ prefix and
16-bit scatter as baseline. Qualify native byte scatter separately,
with unchanged prefix. Investigate a one-bucket fast path that
preserves overwritten keys, skipped sentinels and ascending index
order while avoiding histogram clear/count/prefix/scatter passes.
For wide workloads prioritize reducing memory traffic and eliminating
passes, rather than duplicating compiler-quality scatter. Benchmark
complete plain and grouped painter entry points, with exact bucket
mapping and tie semantics; include 65535-face unsigned cursor
boundaries in segmented or independent correctness checks.
Compare real scene command order and visual output before enabling
anything. Pass-bit scenario in V1 is global spread, not grouped policy.
No physical hardware or IRQ/ABI stress qualification.

## Tekken fight: where the grouped painter spends (2026-11-19)

The Tekken 3 fight profile (Ymir, cache on, Lei vs King, the tick-replay
combat tape, fields 2000-2047) lists `paint_order_grouped_buckets` at about
2.7% of the Master. Its per-instruction costs settle what that is:

- About 97% (10,465 of about 10,800 cycles per field) is the stable scatter
  (`place_reference`): about 500 live faces per field at about 21 cycles each.
- The group scan and the group-mapped count pass cost about 0. The fight's keys
  usually hold one live group, so the call takes the single-span path, whose
  range and spread are already assembly.

A rewrite of the group scan and count loops (current group in registers,
membership by one unsigned compare) gave identical output on the host and
changed the window by 0.1% (517,508 to 516,876 cycles). It was reverted.
With V1 above showing a native scatter no faster than GCC's, this stage has no
kernel left to write. A gain would need fewer passes, for example a
per-bucket chain built in the count pass and walked by the emitter.
