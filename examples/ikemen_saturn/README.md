# Ikemen Saturn (training subset)

Kung Fu Man vs Kung Fu Man training demo proving LibSaturn 2D capacity
with real Ikemen GO screenpack assets.

## Controls

* D-pad: forward/back / crouch / jump, interpreted relative to facing
* X: KFM standing light punch (CMD "x")
* A: KFM standing light kick (CMD "a")
* START: reset round (training-demo convenience; KFM's taunt state is not wired yet)
* P2 pad (optional): controls P2; unplugged = idle training dummy

## Assets and attribution

Sprites, palettes and the stage come from the
[Ikemen-GO-Screenpack](https://github.com/ikemen-engine/Ikemen-GO-Screenpack)
(Kung Fu Man and the stage0 "Training Room", Elecbyte/Ikemen GO authors;
see that repo's LICENCE.txt). `.external/Ikemen-GO-Screenpack` is a local
clone; nothing from it enters the repo tree -- only the generated C
tables under `build/generated/ikemen_saturn/` (rebuilt by the example's
Makefile rules).

Conversion happens offline in `tools/ikemen_sff` (SFF v2 container,
LZ5/RLE/PNG-indexed codecs, AIR frame times, collision boxes and stage
composition), `tools/ikemen_snd.py` (Elecbyte SND containers with embedded
WAV samples), and `tools/ikemen_cmd.py` (the real KFM CMD command grammar).
The runtime consumes generated tables; it never parses SFF, AIR, SND or CMD
text on the Saturn.

## Scope (v1 subset)

* States: idle, walk, vertical jump, crouch, standing light punch (200),
  standing light kick (230), hit, KO and guard. Full CNS/Lua VM is NOT ported.
* Input no longer hard-codes Saturn buttons. The build compiles the complete
  `kfm.cmd` command list into bounded tables and the Saturn runtime implements
  Ikemen-style signed input ages, B/F facing normalization, `/`, `~`, `# Ikemen Saturn (training subset)

Kung Fu Man vs Kung Fu Man training demo proving LibSaturn 2D capacity
with real Ikemen GO screenpack assets.

## Controls

* D-pad: forward/back / crouch / jump, interpreted relative to facing
* X: KFM standing light punch (CMD "x")
* A: KFM standing light kick (CMD "a")
* START: reset round (training-demo convenience; KFM's taunt state is not wired yet)
* P2 pad (optional): controls P2; unplugged = idle training dummy

## Assets and attribution

Sprites, palettes and the stage come from the
[Ikemen-GO-Screenpack](https://github.com/ikemen-engine/Ikemen-GO-Screenpack)
(Kung Fu Man and the stage0 "Training Room", Elecbyte/Ikemen GO authors;
see that repo's LICENCE.txt). `.external/Ikemen-GO-Screenpack` is a local
clone; nothing from it enters the repo tree -- only the generated C
tables under `build/generated/ikemen_saturn/` (rebuilt by the example's
Makefile rules).

Conversion happens offline in `tools/ikemen_sff` (SFF v2 container,
LZ5/RLE/PNG-indexed codecs, AIR frame times, collision boxes and stage
composition), `tools/ikemen_snd.py` (Elecbyte SND containers with embedded
WAV samples), and `tools/ikemen_cmd.py` (the real KFM CMD command grammar).
The runtime consumes generated tables; it never parses SFF, AIR, SND or CMD
text on the Saturn.

## Scope (v1 subset)

,
  `+`, `|`, `>`, command timing/buffering, duplicate-name variants and
  the documented F,F auto-greater expansion. All KFM commands are recognized;
  state -1 currently consumes only the commands needed by the subset above.
* Rendering and collision both sample the real KFM AIR actions. Crouch uses
  action 11, vertical jump 41, punch 200 and kick 230.
* Hit registration is AIR-native: generated per-frame Clsn1 attack boxes are
  tested against the opponent's generated per-frame Clsn2 hurt boxes, including
  facing/AIR flips. There are no synthetic punch/kick range rectangles.
* The supported punch/kick use the original KFM CNS damage, hit pause, hit time,
  animation duration and ground hit velocity (state 200: 23 damage, 8/8 pause,
  11 hit time; state 230: 26 damage, 12/12 pause, 14 hit time).
* AIR interpolation, reflections and per-frame rotation remain outside this subset.
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
* RAM: generated sprite/audio/command tables in ROM; command matcher state is
  fixed-capacity and allocation-free; 4MB cart NOT required.
