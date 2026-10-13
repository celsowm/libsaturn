# Skinning stress benchmark

Reproducible stock-RAM workloads with 1, 2, 4 or 8 independently animated
humanoids, three geometry LODs, simple/varied clips and Master/Slave/AUTO
dispatch. All actors share immutable source data and texture residency.

```powershell
.\build-example.ps1 skinning_stress_3d
.\run-example.ps1 skinning_stress_3d -MednafenCart none
.\harness\run-character-plan.ps1 -Bios bios/saturn_bios_us.bin -Scenario stress
```

| Input | Action |
| --- | --- |
| A | Cycle 1/2/4/8 requested actors |
| B | Cycle HIGH/MEDIUM/LOW requested detail |
| X | Toggle one shared walk clip versus independent clip changes |
| Z | Cycle AUTO/MASTER/SLAVE |
| C | Toggle baked textured presentation versus RGB Gouraud lighting |
| Y | Toggle skeleton overlay |
| D-pad / L,R | Orbit / zoom |
| Start | Pause/resume simulation |

The renderer reserves HUD commands and chooses the first LOD within each
actor's conservative face quota. At requested HIGH, four actors use MEDIUM
and eight use LOW. Requested, accepted, downgraded and rejected counts remain
explicit. A canonical scene capacity failure rejects that frame's character
work rather than silently omitting individual faces.

The HUD shows submission/simulation rates, clip/frame, bones, vertices,
visible faces, native command use, measured skin/preparation/sort/emission
costs, complete frame maximum and caller-owned storage. `OWN RAM KB` is the
family's actor/archive/scene buffers, not all library/code/stack memory.
Link-map sizes and stack headroom are reported separately in the readiness
report. Texture bytes likewise count character patterns, excluding font and
VDP2 assets.

`g_character_demo` publishes completed-frame diagnostics. The bounded
`g_character_benchmark` log resides in WRAM-L. Analysis groups contiguous
configuration intervals and uses VBlank counts rather than the rounded HUD.
The runner validates all twelve count/CPU combinations and records median
CPU, skin, preparation, sort/emission, command use and complete-frame maxima.
The final varied-clip interval is reported separately.

The host gate `test_skin_parallel` compares two independent outputs with the
serial reference and times serial work versus two host threads. Host thread
creation overhead and host timings do not predict SH-2 speedup. Saturn policy
decisions use Ymir measurements and actual worker counters.

For a shorter matched two-HIGH-actor comparison of all three CPU policies,
run `harness/run-character-plan.ps1 -Bios bios/saturn_bios_us.bin -Scenario prepare`.
It discards transition samples and reports complete-frame p95 alongside CPU
and preparation medians. See the measured
[preparation study](../../docs/CHARACTER_PREPARATION_PERFORMANCE.md).

The separate bounded `g_character_handoff` log records calibrated FRT timings
for the VBlank wait and frame submission, plus cumulative VDP1 wait/timeout
and display-field counters. Its rows match benchmark iteration numbers.
The analyzer reports median/p95 timings and counter differences for each
steady interval; display fields do not count distinct presented images.
Use `harness/run-character-handoff.ps1 -Bios bios/saturn_bios_us.bin -Transfer cpu`
and repeat with `-Transfer dma` to compare the existing command-upload paths.
The study copies the stock stress configuration into a temporary example and
removes that source directory afterward. Production transfer defaults stay intact.
See the [handoff study](../../docs/CHARACTER_HANDOFF_PERFORMANCE.md).

Assets are [original MIT project content](../characters/README.md).
See [CPU skinning](../../docs/CPU_SKINNING_3D.md) and
[fighting-game readiness](../../docs/FIGHTING_GAME_READINESS.md).
