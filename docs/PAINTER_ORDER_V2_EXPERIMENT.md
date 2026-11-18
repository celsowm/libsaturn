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
