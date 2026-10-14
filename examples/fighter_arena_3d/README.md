# Vector Arena

A playable two-fighter integration example with original blue/red humanoids,
independent clips and crossfades, a central training mat, VDP2 backdrop, contact shadows,
health bars, round timer, hit flashes and a camera that follows both fighters.
All character faces enter one global painter queue.

```powershell
.\build-example.ps1 fighter_arena_3d
.\run-example.ps1 fighter_arena_3d -MednafenCart none
.\harness\run-character-plan.ps1 -Bios bios/saturn_bios_us.bin -Scenario arena
```

Controls:

| Input | Action |
| --- | --- |
| Left/right | Move on the fighting axis; facing follows the opponent |
| Up/down | Sidestep |
| A / C | Punch / kick |
| B | Guard |
| X | Jump |
| Y | Cycle mesh detail: MEDIUM (default), LOW, HIGH |
| Z | Skeleton and collision-volume visualization |
| Start | Restart the round and release/reload the selected resources |

The opponent uses deterministic approach/guard/attack/recovery logic.
Body volumes separate every fixed simulation tick, including at walls; sustained
forward movement cannot pass through the opponent. Sidestepping around the
body remains allowed. Hit and hurt volumes use a bounded proximity approximation; attacks have
fixed active frames, damage and recovery. Three unblocked hits cause knockdown;
zero health or the round timer ends the round. These rules are application
code in `examples/characters/combat.c`, not a fighting-game subsystem in the
library. This demo does not implement a commercial game's moves or assets.

The high-detail character has 338 deformation vertices and 401 faces;
157 vertices blend two anchors. Two costumes share topology, motion and texture
patterns while owning independent poses, vertex buffers, texture handles and
palettes. Native character texture pixels total 16,896 bytes for two owners.
Font/background allocations are separate. The normal application runs on
stock Work RAM and enforces the linker's stack reserve.

The arena starts at MEDIUM (260 vertices and 301 faces per fighter), with
brighter gloves/pants, fuller legs, a lower camera and a quieter stage. The
viewer and stress benchmark still start at HIGH. Health bars have visible
tracks and the opponent's bar drains toward its outside edge. Submission
timing and detail labels appear with Z diagnostics; they are not presented as
measured display FPS. Mesh detail changes preserve simulation and body volumes.

PAL emulator checks use the European BIOS and separate artifacts:

```powershell
.\harness\run-character-plan.ps1 -Bios bios/saturn_bios_eu.bin -Pal -Scenario all
.\harness\run-character-plan.ps1 -Bios bios/saturn_bios_us.bin -Scenario rate
```

LOW reaches 30 submissions/s in the NTSC arena comparison (25/s PAL); MEDIUM
retains about 20/s NTSC. The `rate` scenario checks LOW over 1,200 fields with
combat and reload, with a two-field target for the selected video standard.
See [measurements and acceptance](../../docs/CHARACTER_PRESENTATION_ACCEPTANCE.md).

For CD-backed character loading:

```powershell
$env:CHARACTER_DEMO_DISC_ASSETS='1'
.\build-example.ps1 fighter_arena_3d
.\run-example.ps1 fighter_arena_3d -MednafenCart none
Remove-Item Env:CHARACTER_DEMO_DISC_ASSETS
```

This configuration uses CDFS and the same bounded asset cache/prefetch/loading
APIs as the embedded test configuration. Mednafen displayed the disc-loaded
arena and a completed knockdown round without expansion RAM. Real Saturn
hardware has not been tested. Ymir runs inject the application and use the
explicit embedded source configuration.

The automated scenario covers guard, attacks, jump, sidestep, debug volumes
and round restart. The separate `-Scenario collision` holds forward movement
for 500 fields and verifies zero overlaps and unchanged fighter order. RAM diagnostics verify hits, blocks, two accepted actors,
texture/asset reloads, serial/worker output equivalence, command capacity and
zero panic. See [readiness and measured limits](../../docs/FIGHTING_GAME_READINESS.md)
and [original asset licensing](../characters/README.md).
