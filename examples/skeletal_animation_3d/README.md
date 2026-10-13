# Skeletal animation viewer

An original textured 24-joint humanoid with nine named clips, two-anchor elbow
and knee seams, contact shadow and three geometry LODs. Runtime joint sampling,
crossfades and CPU skinning update a mutable mesh in the canonical scene.
Texture pixels upload once. All storage is bounded and stock Saturn RAM is the
baseline. This example shares application code with the arena and stress demo.

```powershell
.\build-example.ps1 skeletal_animation_3d
.\run-example.ps1 skeletal_animation_3d -MednafenCart none
.\harness\run-character-plan.ps1 -Bios bios/saturn_bios_us.bin -Scenario viewer
```

Mednafen is the default emulator; Ymir provides automatic screenshots and RAM
inspection. The automated viewer checks pause/resume pixels, clips, camera,
lighting, skeleton overlay, execution policy and serial/worker agreement.

| Input | Action |
| --- | --- |
| A / B | Next / previous clip |
| L / R | Decrease / increase playback speed |
| D-pad | Orbit camera |
| X + up/down | Zoom camera |
| X | Toggle detailed diagnostics |
| C | Toggle indexed baked light / RGB Gouraud light |
| Y | Toggle skeleton overlay |
| Z | Cycle AUTO / MASTER / SLAVE |
| Start | Pause / resume simulation |

Clips are idle, walk, run, jump, punch, kick, guard, hit and knockdown. Simulation
uses elapsed fixed 60 Hz ticks; the HUD's SUB/S is submission throughput, not
distinct displayed images per second. Full diagnostics show clip/frame,
joints/vertices/faces, commands, skin/preparation/sort/emission timing and owned
memory. Physical hardware acceptance has not been performed.

## Optional robot study

The earlier Robot Police Unit viewer remains available:

```powershell
$env:SKELETAL_ROBOT_DEMO='1'
.\build-example.ps1 skeletal_animation_3d
.\run-example.ps1 skeletal_animation_3d -MednafenCart none
Remove-Item Env:SKELETAL_ROBOT_DEMO
```

This separate CC BY 4.0 asset by Jungle Jim has 102 joints, 933 shared
deformation vertices, 1,799 faces and one gesture clip. D-pad orbits, L/R zoom,
B resets camera, C toggles automatic orbit and Start pauses. See
[asset attribution](assets/ROBOT_POLICE_ATTRIBUTION.md) and
[the optimization study](../../docs/SKELETAL_ANIMATION_PERFORMANCE_STUDY.md).
Rebuild without the environment setting to restore the original humanoid.

See [original content and resource ownership](../characters/README.md),
[CPU skinning](../../docs/CPU_SKINNING_3D.md),
[character rendering](../../docs/CHARACTER_RENDERING_3D.md) and
[readiness](../../docs/FIGHTING_GAME_READINESS.md).
