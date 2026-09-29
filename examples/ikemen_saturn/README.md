# Ikemen Saturn (training subset)

Kung Fu Man vs Kung Fu Man training demo proving LibSaturn 2D capacity
with real Ikemen GO screenpack assets.

## Controls

* D-pad: walk / crouch / jump (UP)
* A/X: punch, B/Y: kick
* START: reset round
* P2 pad (optional): controls dummy; unplugged = AI stand/guard dummy

## Assets and attribution

Sprites, palettes and the stage come from the
[Ikemen-GO-Screenpack](https://github.com/ikemen-engine/Ikemen-GO-Screenpack)
(Kung Fu Man and the stage0 "Training Room", Elecbyte/Ikemen GO authors;
see that repo's LICENCE.txt). `.external/Ikemen-GO-Screenpack` is a local
clone; nothing from it enters the repo tree -- only the generated C
tables under `build/generated/ikemen_saturn/` (rebuilt by the example's
Makefile rules).

Conversion happens offline in `tools/ikemen_sff` (SFF v2 container,
LZ5/RLE/PNG-indexed codecs, AIR frame times, stage composition with
palette merge) and `tools/ikemen_snd.py` (Elecbyte SND containers with
embedded WAV samples). The runtime consumes only generated tables through the
`examples/ikemen_saturn/ikemen_anim.h` frame-table contract -- it never parses
SFF or SND itself.

## Scope (v1 subset)

* States: idle 0, walk 20, jump 40, crouch 50, punch 200, kick 210,
  hit 500, KO 510, guard 550. Full CNS/Lua VM is NOT ported.
* Sim state -> KFM AIR action: 0/20/40, jump->42, punch->200,
  kick->230, hit->105, KO->120 (lying), guard->130.
* AIR interpolation, reflections, per-frame rotation: dropped.
* Stage: real stage0 image as a 320x224 indexed8 NBG0 plane on VDP2
  (palette bank 4, priority 2 under the VDP1 sprite layer at 7),
  transparent VDP1 erase, scroll follows the fighters' midpoint at
  delta 1:1 (stage camera bounds +/-125); the plane wraps seamlessly.
* P2 palette: draw-time override via `sat_render2d_set_palette` with the
  registered KFM palette (1,4) -- both fighters share one pixel copy in
  VDP1 VRAM and still get distinct colours (the lib's palette-variant
  tints compose on top for the hit flash).
* Audio: real KFM attack samples from `chars/kfm/kfm.snd` (0,0 punch,
  0,1 kick) plus the matching common hit samples from `data/common.snd`
  (5,0 light hit, 5,1 medium hit), converted offline to mono signed 8-bit PCM.
  Stage0 intentionally has an empty `bgmusic` entry, so there is no BGM to port.

## Budgets (measured)

* VDP1 cmds: 2 fighters + 2 mesh shadows + HUD << 2048.
* Textures: 50 unique KFM sprites (~264KB INDEX8, shared by both
  players) + HUD font atlas; 51/64 slots.
* CRAM: font bank 1 (external), stage bank 4 (external), KFM (1,1)
  and (1,4) logical banks, tint variants on demand.
* Audio: 4 resident S8 SFX; 28-voice pool otherwise untouched.
* RAM: generated tables in ROM; 4MB cart NOT required.
