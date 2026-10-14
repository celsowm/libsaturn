# Character presentation and PAL acceptance

The arena uses a central training mat, a quieter blue backdrop, framed health
tracks, a lower three-quarter camera, fuller legs and brighter gloves/pants.
Animation, texture ownership and body separation retain their existing paths.
MEDIUM is the arena's initial mesh detail; Y cycles MEDIUM, LOW and HIGH.
The viewer and stress benchmark still start at HIGH. Z exposes the arena's
submission counter, selected detail and bone/body diagnostics.

## Runtime work

Rigid joints skip nine fixed-point multiplies when their scale is exactly one.
Quaternion normalization skips division for zero numerators and resolves W-only
quaternions directly, preserving the existing positive/negative saturation and
zero fallback. The host gate exhausts every signed 16-bit W value with aliased
input/output, alongside existing scaled-joint, quaternion, skinning and clip
fixtures. No floating-point runtime work or new per-frame storage is added.

A general affine matrix multiplication trial passed numerical reference and
aliasing checks but produced no measurable improvement in the character run.
It was removed. The independent 1,000-product reference test remains.
A signed-axis world-transform trial also retained the same arena CPU medians
and was removed; the 48 cardinal-transform reference cases remain.

RGB faces can now use VDP1 system clipping beyond the texture-stretch guard.
Both raw command coordinates and coordinates after the screen-center offset
must remain in the hardware's signed -1024..1023 range. Near-plane crossings
and projection-clamped vertices still use the existing software fallback;
indexed faces retain the existing stretch bound. The host gate exercises both
signed limits and the center-offset boundary. The primary references are
[VDP1 vertex coordinates](sega_saturn_hardware/hard/vdp1/hon/p06_70.md) and
[system clipping](sega_saturn_hardware/hard/vdp1/hon/p07_10.md).

The same 1,400-field HIGH stress input measured the following Ymir medians
before the RGB clipping follow-up. That build includes the silhouette/palette adjustments and
Slave dispatch-table fix, so these are whole-build results rather than an
isolated attribution of every timing difference to quaternion work.

| Policy | Previous / final CPU | Previous / final submissions/s |
| --- | --- | --- |
| AUTO | 41 / 40 ms | 20 / 20 |
| MASTER | 54 / 53 ms | 15 / 15 |
| SLAVE | 45 / 44 ms | 20 / 20 |

Simulation remains approximately 60 steps/s. HIGH geometry preparation is still
13 ms in AUTO/SLAVE and 22 ms in MASTER. There is no claim that HIGH has reached
30/60 presented frames/s. Logs and dumps use `harness/build/character_affine_baseline*`
and `harness/build/character_pose_optimized*`; intermediate affine/scale trials
use `character_affine_optimized*` and `character_rigid_optimized*`.

## Cross-emulator dispatch fix

Mednafen exposed `SAT_ERR_UNSUPPORTED` while waiting for the first paired geometry
task after skinning. Slave could retain an older cached registration table when
the geometry adapter registered later. The executor now consumes the dispatch
array and its count before worker lookup, without consuming other Master state.
The real-worker host gate fails against the preceding executor and passes after
the fix. Mednafen then renders the arena normally with a stock cartridge setting.
See [the parallel cache contract](PARALLEL_RUNTIME_ARCHITECTURE.md).

## Arena rate and PAL checks

The runner accepts `-Pal`, forwards the hardware video selection, uses separate
`*_pal` artifacts and calculates rates with 50 fields/s. Simulation retains
its independent 60 Hz target; PAL display fields are not counted as simulation
ticks. The European BIOS is available locally and used by these gates.

```powershell
.\harness\run-character-plan.ps1 -Bios bios/saturn_bios_eu.bin -Pal -Scenario all
.\harness\run-character-plan.ps1 -Bios bios/saturn_bios_us.bin -Scenario all
.\harness\run-character-plan.ps1 -Bios bios/saturn_bios_us.bin -Scenario rate
.\harness\run-character-plan.ps1 -Bios bios/saturn_bios_eu.bin -Pal -Scenario rate
```

The PAL viewer passed all nine clips, shading and paused-image equivalence.
The PAL arena passed guard, attacks and resource reload. The MEDIUM collision
gate checked 870 ticks with zero overlap and unchanged fighter order; its final
interval measured 25 submissions/s and approximately 60 simulation steps/s.
These are emulator observations, not physical Saturn certification.

Full NTSC and PAL viewer, arena, collision, twelve-workload stress and material
gates passed with zero guest panic. They include 69 NTSC and 85 PAL continuous
geometry comparisons. The final RGB clipping follow-up passed the host suite,
matched arena runs in both video standards and another PAL material gate.
The full matrix was obtained before the final RGB optimization; the following
table is from the matched arena follow-up, using the same combat/detail input
and unchanged silhouette, palette and camera.

| Detail | NTSC CPU before / after RGB clipping | NTSC submissions/s before / after | PAL submissions/s after |
| --- | --- | --- | --- |
| MEDIUM | 37 / 36 ms | 19.98 / 19.98 | 24.37 |
| LOW | 32 / 31 ms | 28.24 / 30.00 | 25.00 |
| HIGH | 48 / 47 ms | 17.94 / 18.50 | 15.81 |

MEDIUM emission drops from 6.847 to 5.963 ms. The LOW interval in this detail
comparison has 25 steady samples; the dedicated 1,200-field `rate` gate uses
LOW during guard, attacks, jump, sidestep and reload for a sustained check.
The HIGH interval includes diagnostics and resource reload, so it is not the
same workload as the HIGH stress table above. Submission rates are calculated
from guest submission/VBlank counters using 60 NTSC or 50 PAL fields/s.
The arena reuses the existing bounded benchmark buffers, and its analyzer
checks fixed simulation rate and zero body overlap at every detail.

The final 1,200-field sustained LOW captures confirm the two-field target:

| Video | Steady samples | CPU median | Submissions/s | Simulation steps/s | Frame median / p95 |
| --- | --- | --- | --- | --- | --- |
| NTSC | 569 | 26 ms | 29.66 | 60.00 | 33 / 34 ms |
| PAL | 571 | 27 ms | 24.78 | 60.00 | 40 / 43 ms |

The captures include resource reload, whose maximum frame durations are
229/235 ms; it is retained in the aggregate rates. Body checks cover 1,171/1,410
ticks with zero overlaps. Both captures record zero VDP1 draw timeouts and
late swaps. Draw-wait counters are 4 NTSC and 476 PAL; they are synchronization
events, not counts of presented frames. No unique-presentation FPS is inferred.

The initial analysis rejected a short startup detail interval; the analyzer
now requires 20 steady arena samples after three transition samples and the
rate scenario validates LOW. PAL uses its 25/s target rather than the NTSC
30/s threshold. Corrected analysis of the same captures passes through the
project runner's `-AnalyzeOnly` option. This option preserves the matching map,
RAM dumps, screenshots and input script and performs no new emulator run.
Artifacts use `character_rate*` and `character_rate_pal*`; successful final
validation logs are `character_sustained_ntsc_final_gate.log` and
`character_sustained_pal_final_gate.log` under `harness/build`.

All three delivered stock-RAM ISO builds pass. The stress map ends BSS at
`0x060f6b48` and WRAM-L at `0x00282658`, leaving 38,072 high-RAM bytes including
the required 16 KiB Master stack reserve. Geometry, texture and archive counts
retain their existing bounds. All host/tool checks pass with the delivered
renderer and executor. Mednafen runs the rebuilt default MEDIUM arena normally;
the fresh capture is `harness/build/character_delivered_mednafen.png`.

Physical Saturn timing, cache behavior and PAL output still require
the existing [hardware acceptance checklist](HARDWARE_ACCEPTANCE_CHECKLIST.md).
