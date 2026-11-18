# C2 — world-space visibility V1

The scene has two world visibility consumers: a compiled chunk path
calling quad_visible_small_body then quad_visible_general, and a general
path calling quad_visible. Screen-space culling remains untouched.
Existing kernels already use signed16 edge proofs, exact split normal
dots below 2^50 and a historical scaled-normal fallback. Do not alter
winding, centre truncation, collapsed corner order, tie-to-hidden,
double-sided rules or the large normal fallback.

Mode 0 is original. Mode 1 conditionally compares the eye to the exact
truncated centre on the only nonzero normal axis, avoiding the full dot
and unrelated centre arithmetic. In the small path this follows the
existing centre range proof; in wide it follows the exact-limit check.
Mode 2 adds an explicitly unrolled high/low decomposition and skips the
low dot if all low residues are zero. All three modes preserve the
original public defaults, including the inline compiled scene path
(which is not yet redirected to a candidate). No new hardware divide or
Assembly kernel. Scope is the defined original arithmetic domain; this
does not fix signed overflow in the old edge/view subtraction for
arbitrary out-of-domain int32 positions.

24 workloads: small/wide, six signed orientations by winding reversal,
collapsed B/D, collinear, signed16 boundary, wide centre, negative
rounding, exact limit and just below, nonplanar, integer and fractional
normal splits, tangent, extreme view offsets and mixed inputs.
64 batches of 32 faces, serial master and parallel master/slave,
Ymir and Mednafen. Each mode receives identical input; call order
rotated. Verify every visibility byte against scalar signed
quotient/remainder oracle (split 2^26 vs original 2^24); compatibility
oracle uses existing scaled-normal helper for historical fallback.
Guard both ends of each result array. No hardware or real renderer
execution yet. If candidate wins, qualify both compiled and general
scene paths with real command bytes and images before changing default.

Run `python3 harness/scripts/run_world_cull.py`; return
`harness/build/world_cull_bench-return.zip`.
