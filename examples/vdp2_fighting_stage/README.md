# VDP2 Fighting Stage (E11)

The existing two-fighter character demo with a different backdrop. The
fighters, combat simulation, skeleton evaluation, global painter and VDP1
command path are the same code as `fighter_arena_3d`; only the VDP2
background differs:

- NBG3 stands and a drifting crowd (priority 1)
- NBG2 hanging banners with an animated colour offset (priority 3)
- NBG1 moving spotlight beams, clipped to the upper hall by window W0
  (priority 2)
- NBG0 ring apron rails and corner lights (priority 4)

The training mat, fighters, shadows and HUD stay VDP1 draws at priority 6, so
the stage can never cover a fighter.

```powershell
.\build-example.ps1 vdp2_fighting_stage
.\run-example.ps1 vdp2_fighting_stage -MednafenCart none
.\harness\run-vdp2-fighting-stage.ps1 -Bios bios\saturn_bios_us.bin
```

| Input | Action |
| --- | --- |
| Left/right | Move on the fighting axis |
| Up/down | Sidestep |
| A / C | Punch / kick |
| B | Guard |
| X | Jump |
| Y | Cycle mesh detail |
| Z | Debug overlay |
| L | Show/hide the VDP2 stage |
| Start | Restart the round |

The example also hides and shows the stage once at demo frames 70 and 120 for
the harness, so the two states are measured without a pad script.

## Evidence and limits

`harness/run-vdp2-fighting-stage.ps1` dumps the stage telemetry and the normal
character metrics, captures on/off/on frames, and checks that the fighter
simulation advanced in both states, the stage adds no VDP1 command of its own
(the small count difference is the continuing animation), all four NBGs are
enabled in BGON, and the backdrop returns after the toggle. The same 5/5
checks pass in the PAL sweep at 50 Hz. On the pinned
Ymir revision the stage runs at the character demo's normal cadence; the
example does not claim a measured 60 fps.

The stage code lives in `examples/characters/stage.c` and is compiled only
into this example; `fighter_arena_3d` keeps the legacy NBG0 background. It
uses the public layer, window and colour-offset APIs, not raw register pokes.
Mednafen 1.32.1 window captures show the same fighter scene with the stage
off (flat backdrop) and on (stands, banners and moving spotlights). Physical
hardware remains **PENDING**.
