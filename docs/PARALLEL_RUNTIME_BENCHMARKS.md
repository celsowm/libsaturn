# Parallel runtime benchmark report

This report records the first reproducible Master-only versus Slave-assisted
comparison for the high-level runtime and its geometry-enabled example.

## Reproduction

Repository commit under test before the changes: `ae9416a`.
Validation was run from the working tree after the geometry batch changes.

```powershell
.\build-example.ps1 parallel_runtime -ForceRebuild
.\harness\run-harness.ps1 parallel_runtime `
  -Bios .\bios\saturn_bios_us.bin -Frames 120 -BootFrames 90 `
  -ProfileInstructions .\build\parallel_runtime.final_slave.instructions.csv `
  -ProfileCycles .\build\parallel_runtime.final_slave.cycles.csv
```

The Master-only run used the identical example, assets, camera, object count,
animation state, and 120-frame window, with only the initial executor policy
temporarily changed from `SAT_PARALLEL_AUTO` to `SAT_PARALLEL_MASTER` for the
benchmark build. That temporary source change was reverted before the final
build. It was necessary because the harness pad event is useful for manual
mode cycling but did not provide a reliable steady-state backend transition
for this comparison.

The harness used Ymir's direct-injection path with the repository's US BIOS
dump, 90 BIOS boot frames, and 120 program frames. The instruction CSV has
210 rows: the first 90 are excluded from the comparison. Ymir's fixed cycle
profile is reported separately and is not treated as CPU-work time.
For `parallel_runtime`, the harness also supplied the Slave reset-vector
handoff resolved from `build/parallel_runtime.map`; without that handoff Ymir
would execute Slave bootstrap instructions but the high-level task counter
would correctly fall back to Master.

## Animation and geometry-enabled workload

The example renders 32 animated procedural objects and prepares a batch of 12
independent 3D quad instances. The geometry batch is submitted first, the
animation task is queued behind it, and the Master computes pose and transform
checksums before waiting. The first prepared geometry batch is also compared
with a synchronous execution of the same preparation function.

| Policy | Frames | Master SH-2 instructions | Slave SH-2 instructions | Frames with Slave instructions | Mean fixed system cycles |
|---|---:|---:|---:|---:|---:|
| AUTO (Slave selected) | 120 | 25,928,441 | 37,887,307 | 115 | 449,204 |
| MASTER | 120 | 25,603,171 | 0 | 0 | 449,204 |

The Slave run therefore demonstrates actual geometry/animation task execution
on both processors (the on-screen task counter reached the Slave path and the
Slave instruction counter was non-zero in 115 steady-state frames), but the
measured total instruction count is higher and the fixed Ymir cycle budget is
effectively unchanged. This is not evidence of a frame-rate improvement.
The current data supports keeping AUTO conservative until a larger geometry
workload is measured with runtime FRT timing and complete-frame timing that
separates initialization from steady state.

The recorded raw files are generated artifacts and are intentionally not
committed. The commands above reproduce them as:

```text
build/parallel_runtime.final_slave.instructions.csv
build/parallel_runtime.final_slave.cycles.csv
build/parallel_runtime.master_only.instructions.csv
build/parallel_runtime.master_only.cycles.csv
```

## Correctness and limitations

Host tests compare synchronous batch preparation and canonical merge behavior,
including scene-wide depth/pass ordering and equal-depth stability. The Ymir
runs booted the injected image (`pc_after_run` remained inside the injected
program range) and completed all 120 requested frames. The current harness
run did not produce a formal pixel-diff assertion; framebuffer screenshots
remain an inspection aid rather than an equivalence proof. Instruction
profiling proves useful Slave execution, not speedup.

No parallel physics, asset processing, or automatic adaptive scheduler was
implemented. The next justified benchmark milestone is a workload sweep with
small, medium, and large geometry batches, per-mode FRT-tick telemetry, and
frame captures/equivalence checks for Skybridge and Pac-Man after their normal
example builds.
