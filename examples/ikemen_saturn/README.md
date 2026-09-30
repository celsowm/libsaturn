# Ikemen Saturn (training subset)

Kung Fu Man vs Kung Fu Man training demo using the original Ikemen/MUGEN
character data on Saturn hardware.

## Hardware profile

ikemen_saturn targets a Saturn with a **4 MiB RAM expansion cartridge**.
The cartridge is mandatory by design: no-cart and 1 MiB configurations stop
on an explicit requirement screen instead of silently falling back to a
smaller content profile.

Packed character sprite payloads are generated as KFM_SPR.BIN, staged on the
ISO, copied once at boot into the RAM cartridge, and kept resident there for
the fight. Internal WRAM is reserved for hot simulation state plus bounded
I/O/decode scratch; VDP1 VRAM remains a 32-entry texture working set.

Mednafen's normal LibSaturn launcher already defaults to the 4 MiB cart:

    .\run-example.ps1 ikemen_saturn -BuildFirst -MednafenCart extram4

The Ymir harness must be given the matching profile explicitly:

    .\harness\run-harness.ps1 -Example ikemen_saturn -BuildFirst -RamCart 4m

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
facing-relative B/F, hold/release, `# Ikemen Saturn (training subset)

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

, `+`, `|`, `>`, timing, buffering
and duplicate command variants. The postfix State -1 VM now also receives P2
body/state/move context and the power meter. KFM's derived `var(1)` combo gate
is inlined from the original VarSet rule, so throws, Palm and Knee entries are
compiled from the original CMD instead of KFM-specific C branches.

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
* 5080 / 5081: downed re-hit shake and knockback
* 5110 / 5120 / 5150: lying down, get-up and defeated lying state
* 5200 / 5201 / 5210: ground and air fall recovery driven by the CMD recovery command
* 800 / 810 / 820 / 821: Kung Fu Throw capture, target binding, throw damage
  and custom thrown-air state
* 1000 / 1010: Light/Strong Kung Fu Palm with source-selected near/far
  HitDefs
* 1020 / 1025-1028: Fast Kung Fu Palm, custom victim state and wall bounce
* 1050-1056 / 1060-1061: Light/Strong Kung Fu Knee, persistent knee HitDef,
  optional air kick and landing
* 1070 / 1071 / 1075: Fast Kung Fu Knee multistage attack
* 1100 / 1110 / 1120: Light/Strong/Fast Kung Fu Upper, including Fast
  Upper's re-triggered first HitDef, force-stand and per-hit fall acceleration
* 1200 / 1210 / 1220: Light/Strong/Fast Kung Fu Blow with source corner-push
  recoil, Width windows and Fast Blow fall acceleration

The generic runtime now supports contextual command/velocity triggers plus
`VelSet`, `VelMul`, `PosSet`, animation selection by local X velocity,
remembered jump direction, `prevStateNo` run-jump selection, compiled
air-jump limits and state-specific landing targets. HitDefs also preserve
guard flags, guard/air-guard velocities, guard timing, animation type,
`guard.kill` chip-KO semantics, and the dedicated `down.velocity` /
`down.hittime` branch used when striking a liedown opponent.
Standing, crouching and air guard hits enter the common 150-155 graph, while
normal damage begins in the common 5000+ get-hit graph instead of the old
single synthetic hit state. HitDef target filtering now honors H/L/M/A/F/D
plus the +/- get-hit modifiers. Simultaneous contacts are gathered before
state changes so numeric priority and Hit/Miss/Dodge clashes can trade or
suppress hits correctly. Conditional HitDefs are activated on their exact
source tick and persist until replaced/state exit; p2bodydist-selected Palm
HitDefs therefore keep the near/far definition chosen at activation time.
A HitDef may also carry a source `trigger2`: Fast Upper rearms only that
controller's local hit bit on AnimElem 4, allowing its intentional second
30-damage contact without turning persistent HitDefs into accidental multihits.
KFM's [Data] airjuggle budget, StateDef juggle cost and HitDef air.juggle cost
are tracked across falling/downed targets. StateDef poweradd drives a bounded
0-3000 power meter, and hitdefpersist carries Knee HitDefs across the ground
to air state transition without granting a duplicate hit. HitDefs now also
preserve `forcestand`, per-hit `yaccel`, `air.fall`, conditional damage
from `prevstateno`, and `ground.cornerpush.veloff`. Ground physics selects the compiled stand or
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
* `VelSet` / `VelMul`, including conditional custom-state friction
* target/bind/target-state/target-life controllers used by throws
* `ChangeAnim2`, `SelfState`, `HitVelSet`, `PosFreeze`
* edge-aware wall-bounce movement used by Fast Palm
* HitDef re-trigger/rearm, force-stand, per-hit Y acceleration and ground
  corner-push recoil used by Upper/Blow
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
LibSaturn intentionally exposes 64 logical texture slots. Heavy sprite bytes
do not live in the executable: the offline compiler emits 4-byte-aligned
packed SFF payloads into KFM_SPR.BIN, and boot copies that file from CD into
the mandatory 4 MiB RAM cartridge.

During a fight, a cache miss performs:

    RAM cart -> aligned WRAM-L source scratch -> SFF decode scratch -> VDP1 VRAM

The example keeps a bounded 32-entry LRU VDP1 working set and pins both
fighters' current textures before emitting commands. The cart is therefore a
persistent asset-residency tier, not a substitute for hot WRAM.

## Still deferred

This is not yet a complete Ikemen common-state VM. The next important pieces are:

* exact remaining downed/defeated presentation semantics such as
  HitFallDamage, ground effects, NotHitBy and MatchOver animation variants
* exact remaining guard semantics such as conditional air-guard landing,
  complete inGuardDist behavior
* remaining throw edge cases across different character AIR/state owners,
  remaining specials (Blocking/Zankou) and supers
* remaining HitDef semantics such as reversal, hitonce/chain IDs,
  corner-push and advanced attr interactions
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
