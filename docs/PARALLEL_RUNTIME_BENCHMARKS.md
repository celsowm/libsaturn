# Parallel runtime benchmark report

This report records the reproducible Master-only, explicit-Slave, and
conservative-AUTO comparison for the high-level runtime and its
geometry-enabled example.

## Reproduction

Build the example and run the Ymir harness with the repository's US BIOS:

```powershell
.\build-example.ps1 parallel_runtime -ForceRebuild
.\harness\run-harness.ps1 parallel_runtime `
  -Bios .\bios\saturn_bios_us.bin -Frames 120 -BootFrames 90 `
  -ProfileInstructions .\build\parallel_runtime.auto_final.instructions.csv `
  -ProfileCycles .\build\parallel_runtime.auto_final.cycles.csv
```

The AUTO run is the checked-in default. The explicit-SLAVE comparison uses
the identical assets, camera, object count, animation state, and 120-frame
window, with only the initial example policy temporarily changed to
`SAT_PARALLEL_SLAVE`; the source was restored to AUTO and rebuilt afterward.
The Master-only comparison similarly uses `SAT_PARALLEL_MASTER` temporarily.
The temporary policy edits are not part of the committed example.

The harness uses Ymir's direct-injection path, 90 BIOS boot frames, and 120
program frames. The instruction CSV has 210 rows; the first 90 are excluded
from the comparison. Ymir's fixed cycle profile is reported separately and is
not treated as CPU-work time. For `parallel_runtime`, the harness supplies the
Slave reset-vector handoff resolved from `build/parallel_runtime.map`; without
that handoff Ymir would execute bootstrap instructions but the high-level task
counter would correctly fall back to Master.

## Animation and geometry-enabled workload

The example renders 32 animated procedural objects and prepares a batch of 12
independent 3D quad instances. The geometry batch is submitted first, the
animation task is queued behind it, and the Master computes pose and transform
checksums before waiting. The first prepared geometry batch is also compared
with a synchronous execution of the same preparation function.

| Policy | Frames | Master SH-2 instructions | Slave SH-2 instructions | Frames with Slave instructions | Mean fixed system cycles |
|---|---:|---:|---:|---:|---:|
| AUTO (geometry Master, animation Slave) | 120 | 25,805,400 | 37,852,135 | 115 | 449,204 |
| SLAVE (geometry and animation Slave) | 120 | 25,955,641 | 37,887,306 | 115 | 449,204 |
| MASTER | 120 | 25,603,171 | 0 | 0 | 449,204 |

The explicit-Slave run proves actual two-processor task execution, while AUTO
also shows both processors active because animation remains eligible for the
Slave. The measured instruction totals are higher than the Master-only run and
the fixed Ymir cycle budget is effectively unchanged. These measurements do
not claim a frame-rate improvement; they justify keeping this geometry policy
conservative until a broader workload sweep is available.

The recorded raw files are generated artifacts and are intentionally not
committed. The comparison files are:

```text
build/parallel_runtime.auto_final.instructions.csv
build/parallel_runtime.auto_final.cycles.csv
build/parallel_runtime.explicit_slave.instructions.csv
build/parallel_runtime.explicit_slave.cycles.csv
build/parallel_runtime.master_only.instructions.csv
build/parallel_runtime.master_only.cycles.csv
```

## Correctness and limitations

Host tests compare synchronous batch preparation and canonical merge behavior,
including scene-wide depth/pass ordering and equal-depth stability. The Ymir
runs booted the injected image (`pc_after_run` remained inside the injected
program range) and completed all requested frames. The current harness run did
not produce a formal pixel-diff assertion; framebuffer screenshots remain an
inspection aid rather than an equivalence proof. Instruction profiling proves
useful Slave execution, not speedup.

No parallel physics, asset processing, or automatic adaptive scheduler was
implemented. The next justified benchmark milestone is a workload sweep with
small, medium, and large geometry batches, per-mode FRT-tick telemetry, and
frame captures/equivalence checks for Skybridge and Pac-Man after their normal
example builds.
