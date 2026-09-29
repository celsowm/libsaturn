# Ikemen Saturn (training subset)

Kung Fu Man vs Kung Fu Man training demo proving LibSaturn 2D capacity
with real Ikemen GO screenpack assets.

## Controls

* D-pad: forward/back / crouch / jump, interpreted relative to facing
* X: KFM standing light punch (CMD `"x"`)
* A: KFM standing light kick (CMD `"a"`)
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
* A CNS compiler now emits KFM constants, selected Statedef metadata, HitDefs
  and PlaySnd controllers. Fractional velocity/physics values are stored Q8.8
  so the Saturn runtime can consume the original values without float math.
* The first compiled CNS slice is states 200, 210, 230 and 240. Controllers
  outside the currently executable subset are reported explicitly by the
  compiler rather than silently approximated.
* The currently playable state machine still executes idle/walk/jump/crouch,
  standing light punch 200, standing light kick 230, hit and KO. Wiring the
  generated CNS tables into the fight runtime is the next layer; there is no
  claim yet that the full CNS/common-state VM has been ported.
* Stage0 is the real Training Room image on VDP2 NBG0. P2 uses a draw-time
  palette override while both fighters share one VDP1 pixel copy.
* Audio uses samples extracted from KFM/common SND. Stage0 has no BGM entry.

## Runtime constraints

* Generated asset/command/CNS tables live in ROM.
* Command matcher state is fixed-capacity and allocation-free.
* CNS fractional fields use Q8.8; no floating point is required on Saturn.
* 4 MB expansion RAM is not required.
