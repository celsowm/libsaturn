# Static Q16 projection — experimental candidate

Production remains unchanged. The experiment targets the generic camera batch
in rendering/logic.hpp, NOT the Q15 character kernels. Every product is floored
individually before the row sum. The assembly uses DMULS.L / XTRCT, overlaps X
division with Y arithmetic, and Y division with the next vertex's X/W rows.
It accepts a conservatively bounded prefix. Scalar fallback handles every
refused vertex; behind-plane X/Y stay untouched. It drains a pending divide
before returning. Stack and register contracts are in the assembly header.

The diagnostic compares the complete candidate wrapper, including fallback,
against the existing production project_vertices batch and against a separate
software quotient oracle. It verifies whole guarded output arrays, alternating
timed order, reports prefix coverage and runs 128 batches per scenario on each
of three CPU lanes. Scenarios: empty, singleton, general 32 and 256, sparse,
behind, clamp X and Y, large W and mixed 640x448. No approximate arithmetic,
new public API, or release-path selection is introduced.

Run both emulator captures: python3 harness/scripts/run_static_projection.py
(return the generated static_project_bench-return.zip). Timing is inclusive
FRT, with no assertion that the candidate is faster. Decide whether to
integrate only after matched workload measurements and hardware-aware review.
The Ymir immediate divider model cannot establish hardware divider latency.
