# SH-2 kernel verification (initial Phase A slice)

The diagnostic example calls the seven existing graphics modules (nine entry points)
and the ADX channel decoder on the actual target. There are 128 deterministic
cases per entry point per lane: serial Master, concurrent Master and Slave.
The parallel runtime must actually deliver the second lane to the Slave.
Projection uses a software 64-bit quotient oracle; the other graphics tests
use the contract references. ADX uses independent wide arithmetic.

Windows, from the repository root:

```powershell
git pull --ff-only origin main
if ($LASTEXITCODE -ne 0) { throw 'git pull failed' }
powershell -NoProfile -ExecutionPolicy Bypass -File .\harness\run-sh2-kernel-verify.ps1
```

Set LIBSATURN_BIOS if the runner cannot locate your own BIOS. The harness uses
the existing build-example and run-harness scripts. Return harness/build/sh2_kernel_verify-return.zip, which contains the
analysis, probe, metadata and transcript (also on test failure). Do not return BIOS or
commercial assets. If the capture is incomplete, rerun with -Frames 1200.

The report records per-lane CPU identity, SR, CCR, completion, case counts,
mismatches, first failing case and inclusive FRT sums. These sums include
timer and call overhead; they are not an isolated performance baseline.
Tests exercise count boundaries, unsupported projection cases, clips,
skipped keys, direct/indirect command emission, shading and ADX saturation.
This does not yet cover explicit ABI register sentinels, forced interrupt
injection, full stack high-water tests, all contract boundaries or physical
hardware. Phase A1 full workload baseline and the rest of A2 remain open.
