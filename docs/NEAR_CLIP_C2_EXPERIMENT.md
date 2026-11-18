# C2 — exact world near-plane clipping

Mode 0 is the existing world depth and interpolation algorithm.
Mode 1 specializes exact signed unit camera axes and narrows world
position differences only when all three fit int32; the products and
their sum remain signed int64, including truncation toward zero.
Mode 2 additionally uses the existing per-SH-2 signed DIVU helper
for exact interpolation. A crossing constrains the ratio to [0,1].
The denominator must fit [-INT32_MAX, INT32_MAX] and each accelerated
coordinate delta must fit [-INT32_MAX+1, INT32_MAX-1], proving
the quotient cannot trigger the documented signed divider overflow
edges. Numerator equals zero or denominator returns the respective
endpoint. Zero coordinate differences need no division. All wider
operands use the original formula. No fixed-point reciprocal
approximation. No new assembly ABI. Existing default mode remains 0.

Scope is inputs on which the existing signed-64 control arithmetic
is defined. This does not repair overflow in the original API for
unbounded non-normalized forward vectors or extreme products; any
future overflow hardening must be a separate correctness change.
The DIVU registers are CPU-local; interrupt reentrancy must be
qualified separately, not inferred from parallel master/slave success.

20 workloads x 64 batches x 32 quads x three lanes x two emulators.
Both windings, all-in/out, one/two/three inside, alternating,
boundary equality, duplicate corner, signed axis cameras, oblique
and nonunit cameras, invalid arguments, 1-LSB crossings, large
coordinate differences, large eye deltas and denominator fallback.
Compare status, exact output vertices, order, count and untouched
sentinel slots against control and scalar ring oracle. The oracle
shares the original truncation contract, not an alternate geometry
specification. Timed complete clipping, not oracle or seed generation.
No renderer, textured cells, physical hardware or explicit IRQ test
in this stage. No default activation.

Run `python3 harness/scripts/run_near_clip.py`. Return
`harness/build/near_clip_bench-return.zip` with both emulators and
local changes.
