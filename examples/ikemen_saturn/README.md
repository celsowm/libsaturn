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

## Offline compilation

The Saturn does not parse Ikemen formats at runtime:

* `tools/ikemen_sff`: SFF/AIR sprites, timings and Clsn1/Clsn2
* `tools/ikemen_snd.py`: SND/WAV samples
* `tools/ikemen_cmd.py`: KFM CMD command grammar and buffers
* `tools/ikemen_state_rules.py`: selected KFM `[State -1]` ChangeState
  predicates compiled to compact postfix bytecode
* `tools/ikemen_cns.py`: KFM constants, Statedefs, HitDefs, PlaySnd and
  compact supported state controllers

Generated tables live under `build/generated/ikemen_saturn/`.

## Current fidelity

The playable normal attacks now use the original KFM data for states:

* 200 / 210: standing light / strong punch
* 230 / 240: standing light / strong kick
* 400 / 410: crouching light / strong punch
* 430 / 440: crouching light / strong kick
* 600 / 610: jumping light / strong punch
* 630 / 640: jumping light / strong kick

For those states, damage, hit pause, hit time, knockback, AIR collision boxes,
animation timing and the supported controllers come from the source data
instead of duplicate gameplay constants.

The CNS runtime now executes these controller forms:

* `ChangeState` with Time / AnimElem / AnimTime=0 triggers
* `CtrlSet`
* `PosAdd`
* `SprPriority`
* KFM's `Width` AnimElem range form
* KFM's move-contact `ChangeAnim` window, including `ignorehitpause`

State 210 now expands its push width only during the original AnimElem window
and skips the contact-linger animation with the original move-contact
`ChangeAnim`. State 410's two HitDefs are tracked independently, so both hits
can connect.
State 440 preserves its fall flag and vertical launch instead of flattening the
sweep into horizontal knockback. The four jumping normals now retain Physics=A,
including gravity/velocity while attacking, state 600's Time=17 CtrlSet and the
original light-air-attack contact cancels into the two strong air attacks.

KFM `[Data]`, `[Size]`, `[Velocity]` and `[Movement]` values are compiled
to fixed-point Q8.8. Walking, jump launch, gravity, friction, body height and
default player push widths consume those original constants.

The CMD runtime still compiles the complete `kfm.cmd` list and implements
facing-relative B/F, signed input ages, `/`, `~`, `# Ikemen Saturn (training subset)

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

## Offline compilation

The Saturn does not parse Ikemen formats at runtime:

* `tools/ikemen_sff`: SFF/AIR sprites, timings and Clsn1/Clsn2
* `tools/ikemen_snd.py`: SND/WAV samples
* `tools/ikemen_cmd.py`: KFM CMD command grammar and buffers
* `tools/ikemen_state_rules.py`: selected KFM `[State -1]` ChangeState
  predicates compiled to compact postfix bytecode
* `tools/ikemen_cns.py`: KFM constants, Statedefs, HitDefs, PlaySnd and
  compact supported state controllers

Generated tables live under `build/generated/ikemen_saturn/`.

## Current fidelity

The playable normal attacks now use the original KFM data for states:

* 200 / 210: standing light / strong punch
* 230 / 240: standing light / strong kick
* 400 / 410: crouching light / strong punch
* 430 / 440: crouching light / strong kick
* 600 / 610: jumping light / strong punch
* 630 / 640: jumping light / strong kick

For those states, damage, hit pause, hit time, knockback, AIR collision boxes,
animation timing and the supported controllers come from the source data
instead of duplicate gameplay constants.

The CNS runtime now executes these controller forms:

* `ChangeState` with Time / AnimElem / AnimTime=0 triggers
* `CtrlSet`
* `PosAdd`
* `SprPriority`
* KFM's `Width` AnimElem range form
* KFM's move-contact `ChangeAnim` window, including `ignorehitpause`

State 210 now expands its push width only during the original AnimElem window
and skips the contact-linger animation with the original move-contact
`ChangeAnim`. State 410's two HitDefs are tracked independently, so both hits
can connect.
State 440 preserves its fall flag and vertical launch instead of flattening the
sweep into horizontal knockback. The four jumping normals now retain Physics=A,
including gravity/velocity while attacking, state 600's Time=17 CtrlSet and the
original light-air-attack contact cancels into the two strong air attacks.

KFM `[Data]`, `[Size]`, `[Velocity]` and `[Movement]` values are compiled
to fixed-point Q8.8. Walking, jump launch, gravity, friction, body height and
default player push widths consume those original constants.

, `+`, `|`, `>`,
timing/buffering and duplicate command variants. The normal-attack
`[State -1]` ChangeState gates are now compiled too: command equality,
state type/state number, ctrl, movecontact and time predicates execute from a
small postfix rule VM instead of being rewritten as KFM-specific C branches.

## Texture residency

The selected AIR subset now references more than 64 unique KFM sprites, while
LibSaturn intentionally exposes 64 logical texture slots. The example therefore
no longer pre-uploads the whole character. A bounded 32-entry LRU working set
uploads frame textures from ROM on demand and pins both fighters' current
textures before emitting VDP1 commands, preventing an eviction from
invalidating the current command list.

## Still deferred

This is not yet a full CNS/common-state VM. Important remaining pieces include:

* full common1 state flow (stand↔crouch transitions, jump start/landing,
  run/hop, guards and complete get-hit/knockdown/recovery states)
* throws, specials and supers
* guard semantics and the remaining HitDef fields
* fightfx sparks/effects, motif/lifebar flow and full round presentation
* generic PlaySnd dispatch from compiled CNS instead of the current small
  sound binding layer

Unsupported selected controllers are reported by the CNS compiler rather than
silently approximated.

## Assets and attribution

Sprites, palettes, sounds and Training Room content come from
[Ikemen-GO-Screenpack](https://github.com/ikemen-engine/Ikemen-GO-Screenpack)
(Kung Fu Man / Elecbyte / Ikemen GO authors; see that repository's licence).
The source checkout stays under `.external/`; generated tables are rebuilt
locally.
