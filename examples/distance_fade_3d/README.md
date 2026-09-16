# distance_fade_3d

Interactive regression/showcase for LibSaturn's Saturn-native stepped 3D distance fade.

The example is self-contained: both the VDP1 object texture and the tiled NBG0 background are generated in code, so there are no external assets to prepare.

## What it tests

- VDP2 sprite color-calculation ratios `CC0..CC7`;
- selective color calculation using Sprite Type 0 priority/ratio selector bits;
- ordinary VDP1 sprites remaining unaffected while faded sprites blend with NBG0;
- `sat_fade3d_eval()` mapping view depth to 4 or 8 discrete levels;
- hard cutoff versus stepped distance fade at the same final draw distance;
- several different fade slots visible in the same frame;
- final culling after `fade_end`;
- painter-order submission of the 3D quads.

The default ratio table is:

```text
slot:   0   1   2   3   4   5   6   7
ratio:  0   4   8  12  16  20  24  28
```

On Saturn VDP2 sprite color calculation, ratio `0` is almost entirely the sprite/top image and ratio `31` is entirely the second/background image. The example deliberately stops at `28` before the object is culled so the last visible step is still inspectable.

## Controls

```text
A          cycle NO FADE / HARD CUT / FADE 4 / FADE 8
B          toggle automatic camera motion
C          reset camera
UP/DOWN    move camera forward/back
START      toggle HUD
```

`FADE 8` is the main visual target. As the camera moves toward the row of panels, distant panels should emerge progressively from the VDP2 background instead of popping into existence.

## Build

```powershell
.\build-example.ps1 distance_fade_3d -ForceRebuild
```

## Run

For example with Mednafen:

```powershell
.\run-example.ps1 distance_fade_3d -BuildFirst -Emulator mednafen -BiosProfile auto
```

The same example can also be selected through the Makefile as `EXAMPLE=distance_fade_3d`.

## Expected comparison

`HARD CUT` should keep a panel fully visible until the final distance and then remove it abruptly.

`FADE 8` should use the same final cutoff but pass through the eight VDP2 blend slots first. The checker-pattern NBG0 layer moves slightly with camera depth so a correct result visibly reveals the background through the panel; simple palette darkening should not look equivalent.

The HUD remains opaque throughout the test. If the HUD fades together with the panels, the priority-selector/color-calculation condition setup is wrong.
