# Ikemen Saturn (training subset)

Kung Fu Man vs Kung Fu Man training demo using the original Ikemen/MUGEN
character data on Saturn hardware.

## Controls

* D-pad: movement / crouch / jump, relative to facing
* X/Y: light/strong punch
* A/B: light/strong kick
* Down + X/Y/A/B: the four crouching normals
* In the air, X/Y/A/B: light/strong punch and light/strong kick
* START: reset round (training convenience)
* P2 pad (optional): controls P2; unplugged = idle training dummy

## Source checkouts

The build consumes reference data from two ignored `.external/` checkouts:

* `.external/Ikemen-GO-Screenpack`: KFM character, Training Room, common/fight assets
* `.external/Ikemen-GO`: `data/common1.cns.zss`

Neither project is linked into the Saturn runtime. Their text/binary formats are
compiled offline into bounded C tables.

## Offline compilation

The Saturn does not parse Ikemen formats at runtime:

* `tools/ikemen_sff`: SFF/AIR sprites, timings and Clsn1/Clsn2
* `tools/ikemen_snd.py`: SND/WAV samples
* `tools/ikemen_cmd.py`: KFM CMD command grammar and buffers
* `tools/ikemen_state_rules.py`: selected KFM `[State -1]` ChangeState
  predicates compiled to compact postfix bytecode
* `tools/ikemen_cns.py`: KFM CNS plus the supported `common1.cns.zss`
  subset lowered into compact state/controller tables

Generated tables live under `build/generated/ikemen_saturn/`.

## Current fidelity

The playable normal attacks use the original KFM data for:

* 200 / 210: standing light / strong punch
* 230 / 240: standing light / strong kick
* 400 / 410: crouching light / strong punch
* 430 / 440: crouching light / strong kick
* 600 / 610: jumping light / strong punch
* 630 / 640: jumping light / strong kick

Damage, hit pause, hit time, ground/air knockback, AIR collision boxes,
animation timing and supported controllers come from the source data rather
than duplicate gameplay constants. State 210 keeps its original Width window
and contact ChangeAnim, state 410 tracks its two HitDefs independently, state
440 preserves fall/vertical launch, and aerial normals use the air branch of
HitDef against airborne victims.

The CMD runtime compiles the complete `kfm.cmd` command grammar, including
facing-relative B/F, hold/release, `$`, `+`, `|`, `>`, timing, buffering
and duplicate command variants. The normal attacks plus run-forward/back
`[State -1]` gates are compiled to a small postfix predicate VM instead of
being rewritten as KFM-specific C branches.

The first `common1` locomotion slice is also data-driven. The generated CNS
asset now contains states:

* 0: stand
* 10 / 11 / 12: stand-to-crouch, crouch, crouch-to-stand
* 20: walk
* 40 / 45: jump and air-jump startup
* 50 / 51: upward/downward jump flow
* 52: jump landing
* 100: run forward
* 105 / 106: hop backward and hop-back landing
* 120 / 130 / 131 / 132 / 140: guard start, stand/crouch/air guard and guard end
* 150-155: stand/crouch/air guard-hit flow
* 5000 / 5001 / 5010 / 5011: stand/crouch get-hit shake and knockback
* 5020 / 5030 / 5035 / 5040 / 5050: air get-hit/fall transition
* 5070 / 5071: trip shake and knock-away
* 5100 / 5101: fall ground impact and bounce
* 5110 / 5120: lying down and get-up
* 5200 / 5201 / 5210: ground and air fall recovery driven by the CMD recovery command

The generic runtime now supports contextual command/velocity triggers plus
`VelSet`, `VelMul`, `PosSet`, animation selection by local X velocity,
remembered jump direction, `prevStateNo` run-jump selection, compiled
air-jump limits and state-specific landing targets. HitDefs also preserve
guard flags, guard/air-guard velocities, guard timing and animation type.
Standing, crouching and air guard hits enter the common 150-155 graph, while
normal damage begins in the common 5000+ get-hit graph instead of the old
single synthetic hit state. Ground physics selects the compiled stand or
crouch friction values. Jump, air-jump, air attacks and hop-back land through
the compiled common flow instead of being forced directly to idle.

KFM `[Data]`, `[Size]`, `[Velocity]` and `[Movement]` values are compiled
to Q8.8, including walk/run/jump/run-jump velocities, gravity, friction,
thresholds, body height and push widths.

## Runtime controller coverage

The generic CNS runtime currently executes:

* `ChangeState`
* `CtrlSet`
* `PosAdd` / `PosSet`
* `VelSet` / `VelMul`
* `SprPriority`
* `Width`
* `ChangeAnim` plus the common locomotion animation selectors
* Time, AnimElem, AnimTime, movecontact, command-state and velocity/floor triggers

Unsupported selected behavior is reported or listed in the compiler JSON
rather than silently treated as fully compatible. The current common lowering
still records deferred presentation/engine behavior such as `AssertSpecial`
and `MakeDust`.

## Texture residency

The selected AIR subset references more than 64 unique KFM sprites, while
LibSaturn intentionally exposes 64 logical texture slots. The example uses a
bounded 32-entry LRU working set, uploads frame textures from ROM on demand and
pins both fighters' current textures before emitting VDP1 commands.

## Still deferred

This is not yet a complete Ikemen common-state VM. The next important pieces are:

* the remaining get-hit graph: downed re-hit states 5080/5081
  and defeated lying state 5150
* exact remaining guard semantics such as conditional air-guard landing,
  complete inGuardDist behavior and guard.kill
* throws, specials and supers
* remaining HitDef semantics such as hitflag, priority clashes,
  down.velocity/down.hittime, reversal and juggle behavior
* fightfx sparks/effects, motif/lifebar flow and full round presentation
* generic PlaySnd dispatch for all compiled states
* stage DEF execution instead of the current simplified stage runtime

## Assets and attribution

Sprites, palettes, sounds and Training Room content come from
[Ikemen-GO-Screenpack](https://github.com/ikemen-engine/Ikemen-GO-Screenpack).
Common-state source data comes from
[Ikemen-GO](https://github.com/ikemen-engine/Ikemen-GO).
Kung Fu Man originates from Elecbyte; see the upstream repositories for their
licensing and attribution details. Source checkouts remain under
`.external/`; generated tables are rebuilt locally.
