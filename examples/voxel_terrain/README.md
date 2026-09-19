# voxel_terrain

First CPU height-field (Voxel Space) demo for LibSaturn.

- Procedural 256x256 height and indexed-color maps; no CD assets or RAMCart.
- 160x112 INDEX8 rendering to caller-owned RAM, scaled to 320x224 by VDP1.
- Fixed-point camera with heading, altitude, pitch and a bounded view radius.
- HUD displays measured **CPU render**, **dynamic VDP1 texture update** in
  milliseconds, and elapsed display frames per game frame.
- Controls: arrows fly/turn; A/B altitude; X/Y look; C reset; START quit.

Build with `make EXAMPLE=voxel_terrain all`. This first example deliberately
uses the existing VDP1 dynamic texture backend and is **not proof** that VDP2
bitmap streaming works or that the demo reaches a specific FPS on a Saturn.
Use the host test `make test` for pixel and bounds regressions, then compare
emulator screenshots with a real machine before accepting visual correctness.

The sample's world wraps but heights never contain caves, arches or overlapping
strata. Objects drawn above the terrain do not gain depth-tested occlusion.
Read `docs/VOXEL_TERRAIN_PLAN.md` for phase gates and alternative presenters.
