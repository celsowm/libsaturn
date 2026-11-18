# C2 — world visibility V2

V1 passed exactness but regressed small oblique and mixed geometry.
Retain earlier modes and default 0. Compare only these four modes:
- 0: original.
- 3: wide-only V1 combined specialization (axis and zero low normal
  residue). The signed16 small body is literally mode 0, including
  the general signed16 centre/dot logic on the fallback route.
- 4: mode 3 wide path with a separate signed16-proven DMULS.L leaf.
  Register products, explicit ADDC carry chain, restore MACH/MACL.
- 5: mode 3 wide path with a separate signed16-proven MAC.L leaf.
  Materialize two three-word arrays, clear accumulator, perform
  three MAC.L instructions, read full result and restore MACH/MACL.
  SR.S is read, not modified; when saturation is on use the C++
  dot. Compiler T clobber declared for DMULS.L carry operations.
  No promise to preserve T across an ordinary C++ function call.

All MAC/DMULS leaves are noinline so measured complete predicate
includes call, stack, register preservation and array materialization.
The existing signed16 edge proof bounds the normal L1 magnitude
by 2^32-1, so each partial dot fits signed64 even with INT32_MIN
view offsets. No changes to rounding, winding, degenerate ordering,
screen culling, or scaled-normal fallback. Mode 4/5 use native
instructions only under __sh__, with the same C++ expression on host.
Ordinary builds and scene call sites still use 0, including compiled
small-body path. The benchmark measures standalone complete predicate,
not inlined scene throughput. MACH/MACL are restored explicitly within
the asm blocks; independent ABI sentinels, SR.S-on execution and IRQ
stress are still required before release.

28 workloads: V1's same 24 plus exhaustive 64 signed16 box corners
with extreme view offsets, tangent and small view offsets, and
nearly parallel edges with extreme view. Extra quads have exact zero
centroid to keep original signed subtraction defined. Compare every
result byte and sentinels with V1 scalar split-26 oracle. The historical
scaled-normal branch uses the compatibility helper. A tangent to
one of the initial edges is not necessarily tangent to the selected
fallback normal on a degenerate input; oracle resolves that case.
64 batches of 32 faces, 3 lanes per emulator, 10752 batches and
344064 faces per implementation across both emulators. Rotate and
reverse call order; explicit mode-to-buffer slot mapping with static
assertions. Report 3152 bytes, version 2. Retain V1 runner unchanged.

Run `python3 harness/scripts/run_world_cull_v2.py`; return
`harness/build/world_cull_v2_bench-return.zip`. Compilation and
emulator timing pending. No activation. If native dot loses to the
compiler, reject the native leaf; if wide-only wins, profile real
face distributions before qualifying compiled and general scene
paths. No FPS, physical console or interrupt safety claim.
