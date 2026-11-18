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
