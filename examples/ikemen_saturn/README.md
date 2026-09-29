# Ikemen Saturn (training subset)

Kung Fu Man vs Kung Fu Man training demo proving LibSaturn 2D capacity
with real Ikemen GO screenpack assets.

## Controls

* D-pad: forward/back / crouch / jump, interpreted relative to facing
* X/Y: KFM standing light/strong punch
* A/B: KFM standing light/strong kick
* START: reset round (training-demo convenience; KFM's taunt state is not wired yet)
* P2 pad (optional): controls P2; unplugged = idle training dummy

## Assets and attribution

Sprites, palettes and the stage come from the
[Ikemen-GO-Screenpack](https://github.com/ikemen-engine/Ikemen-GO-Screenpack)
(Kung Fu Man and the stage0 "Training Room", Elecbyte/Ikemen GO authors;
see that repo's LICENCE.txt). `.external/Ikemen-GO-Screenpack` is a local
clone; nothing from it enters the repo tree -- only generated C tables under
`build/generated/ikemen_saturn/`.

Conversion happens offline in `tools/ikemen_sff` (SFF/AIR),
`tools/ikemen_snd.py` (SND), `tools/ikemen_cmd.py` (CMD), and
`tools/ikemen_cns.py` (the deterministic CNS subset). The Saturn runtime
consumes generated tables and never parses the source text/container formats.

## Scope

* Rendering and collision sample the real KFM AIR actions and generated
  per-frame Clsn1/Clsn2 boxes, including facing and AIR flips.
* The complete `kfm.cmd` command list is compiled into bounded tables.
  Runtime matching supports facing-relative B/F, signed input ages, `/`,
  `~`, `$`, `+`, `|`, `>`, command timing/buffering, duplicate-name
  variants and repeated-direction auto-greater expansion.
* The CNS compiler emits KFM constants, selected Statedef metadata, HitDefs
  and PlaySnd controllers. Fractional velocity/physics values are Q8.8 so
  values such as walk 2.4, gravity .44 and strong-punch knockback -5.5 are
  retained without floating point on SH-2.
* Standing normals 200, 210, 230 and 240 now execute from the generated CNS
  state/HitDef data: X = 200, Y = 210, A = 230 and B = 240. Damage, hit pause,
  hit time, velocity and HitDef activation no longer come from duplicate
  constants in `ikemen_fight.c`.
* Ground walking, jump launch and gravity now consume the original KFM
  `[Velocity]` / `[Movement]` values in Q8.8 rather than the previous
  integer approximations.
* KFM's documented 200/230 -> 210/240 normal cancels are wired at their CMD
  state-time gates. More complex CNS controllers such as Width, ChangeAnim,
  PosAdd and SprPriority are still deferred and are reported by the compiler.
* Stage0 is the real Training Room image on VDP2 NBG0. P2 uses a draw-time
  palette override while both fighters share one VDP1 pixel copy.
* Audio includes the original whiff/hit samples for all four standing normals.
  Stage0 has no BGM entry.

## Runtime constraints

* Generated asset/command/CNS tables live in ROM.
* Command matcher state is fixed-capacity and allocation-free.
* CNS fractional fields use Q8.8; no floating point is required on Saturn.
* 4 MB expansion RAM is not required.
