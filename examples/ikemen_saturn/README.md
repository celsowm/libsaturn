# Ikemen Saturn (training subset)

Kung Fu Man vs the ZSS Kung Fu Man variant, using the original Ikemen/MUGEN
character data on Saturn hardware. P1 and P2 now own independent AIR/SFF
runtime data; CNS/CMD simulation is still intentionally shared at this stage.

## Hardware profile

ikemen_saturn targets a Saturn with a **4 MiB RAM expansion cartridge**.
The cartridge is mandatory by design: no-cart and 1 MiB configurations stop
on an explicit requirement screen instead of silently falling back to a
smaller content profile.

Packed P1 and P2 sprite payloads are generated as KFM_SPR.BIN and KFM_ZSS.BIN,
staged on the ISO, copied once at boot into separate resident cartridge slots,
and kept there for the fight. Legacy MUGEN fight effects are compiled from
fightfx.sff/fightfx.air into FIGHTFX.BIN and occupy the third cart slot.
Internal WRAM is reserved for hot simulation state plus bounded
I/O/decode/prefetch scratch; VDP1 VRAM remains a 32-entry texture working set.

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

* `.external/Ikemen-GO-Screenpack`: KFM, KFM ZSS, Training Room, common/fight assets
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
facing-relative B/F, hold/release, `, `+`, `|`, `>`, timing, buffering
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
* 1300 / 1310 / 1320 / 1330 / 1340 / 1350 / 1351: high, low and air
  Kung Fu Blocking entry, reversal contact, blocked reaction and air landing
* 1400 / 1410 / 1420: Light/Strong/Far Kung Fu Zankou from the original CMD,
  including combo gating, 330-power Fast entry, source friction/steps,
  negative AnimElemTime first hit, second hit and authored velocities
* 3000 / 3050 / 3051: Triple Kung Fu Palm and Smash Kung Fu Upper supers,
  including source State -1 cancel gates, 1000-power requirement, SuperPause
  power debit, attribute-filtered NotHitBy and MoveHit-only success state
* 170 / 180 / 181 / 191 / 195: pre-intro immunity, win selector/pose,
  source-driven intro and taunt. State 191 now keeps RoundState in intro,
  spawns the authored wood-piece Explods and releases the round timer only
  after AssertSpecial Intro is no longer asserted

Zankou also enters through the original State -1 programs (`QCF_a`, `QCF_b`
and `QCF_ab`) rather than a direct input branch. The generic trigger runtime
now supports three-way AnimElem OR masks, packed negative AnimElemTime hits and
AnimElemTime ranges. Fast Zankou's first HitDef therefore fires exactly two
ticks before animation element 4 using the AIR element start tick, while its
second HitDef activates on element 4. AfterImage/AfterImageTime now use a
generic per-fighter history ring with authored length, TimeGap and FrameGap;
the renderer draws the trail behind the fighter with additive blending.
The trail tint now comes from the authored PalBright/PalContrast/PalAdd/PalMul
parameters instead of a fixed colour. Saturn's indexed-sprite tint path cannot
reproduce the full per-palette Elecbyte transform exactly, so this remains an
approximation of the original palette math rather than a hard-coded KFM colour.
PalFX add/sinadd is compiled and executed per fighter with deterministic cycle
phase and rendered as RGB modulation, covering KFM's yellow blink effects.
AfterImage now also carries the authored PalBright/PalContrast/PalAdd/PalMul
payload through CNS compilation and derives its Saturn additive trail tint from
those values instead of using a fixed yellow constant.

The two KFM supers are also entered from the original State -1 rules. The
expression VM now exposes the currently active HitDef attribute, so
`hitdefattr = SC, NA, SA, HA` cancel gates remain source-driven instead of
being weakened to MoveContact alone. `SuperPause` freezes the fight and
applies its `poweradd` on the authored controller tick; `MoveHit` is tracked
separately from guarded contact, so Smash Upper enters 3051 only after a real
hit. Old-style `NotHitBy` now preserves both state-type and attack-attribute
masks, including the super invulnerability filters using NA/SA/AT. SuperPause now emits its authored fightfx animation/position and darkens the
stage layer through VDP2 colour offset while the freeze is active. HitDef
envshake and fall.envshake drive deterministic camera shake, and fall.damage
is carried into common fall states and applied once by HitFallDamage.
AfterImage/AfterImageTime now continue the authored trail through their real
AnimElemTime/AnimTime/velocity keepalive triggers. PlaySnd, SuperPause sound,
HitDef hitsound and guardsound all feed the same generic fight sound-event
path, including multi-branch PlaySnd controllers. SuperPause darken now covers the stage plus fighter/helper/projectile sprites
while fightfx stays undarkened, matching the intended visual hierarchy more
closely than a whole VDP1-layer offset. Remaining differences are exact
Elecbyte palette-transform arithmetic and HUD/lifebar darken parity.

KFM's Blocking commands are compiled from the original State -1 rules rather
than dispatched through a KFM-only input branch. ReversalDef windows are
compiled into bounded runtime records and resolved with authored CLSN1-vs-CLSN1
contact, state-type filtering, reversal spark placement and p1 state/priority
changes. The high, crouch and air variants therefore enter states 1300, 1320
and 1340 from the source CMD and transition to 1310, 1330 or 1350 on a
successful reversal. The air path also preserves PosFreeze defaults,
AnimElemTime windows and Const(movement.yaccel) gravity before landing in 1351.
The blocked states now execute the authored Pause and one-tick SCA NotHitBy
window, and ReversalDef hitsound 6,0 is queued through a generic fight sound
event and played from common.snd. Pause rewinds the runtime's post-entry
bookkeeping tick so authored Time=1 controllers remain reachable after the
freeze. HitOverride is also compiled as a typed contact window instead of being
collapsed into invulnerability: incoming NA/SA/HA/NP/SP/HP/throw attributes are
preserved on HitDefs and matched after ReversalDef. KFM's AP fallback therefore
has the correct runtime representation and contact semantics. Projectile
entities now participate in the dynamic runtime and contact pass, so that AP
fallback is exercised end-to-end against a real projectile-class HitDef.

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

## Generic entity and expression runtime

The first compatibility-runtime layer is now shared instead of KFM-specific.

State -1 predicates are emitted into a generic stack expression VM with
separate field loads, command loads, comparisons, boolean operators and
integer arithmetic. The live KFM command path evaluates those programs against
registered entities rather than assembling a bespoke P1/P2 predicate struct.

The bounded entity pool currently provides 16 generationally addressed slots
with the engine-level kinds Player, Helper, Projectile and Explod. Entity
identity owns parent/root/target relationships plus MUGEN-compatible integer
var, fixed-point fvar and sysvar storage. P1 and P2 are registered in that
pool today, so self/P2 redirection and target identity already go through the
same mechanism future helpers and projectiles will use.

The fight runtime now binds its two players to that pool directly. Constant
MUGEN `VarSet` and `VarAdd` controllers execute against the bound entity's
60-slot integer variable bank; controller payload values are 32-bit so this
does not introduce a Saturn-only 16-bit variable limit. The existing KFM
`var(2) = command = "holdfwd"` expression keeps its specialized lowering
until controller value expressions are moved onto the expression VM.

`Helper` and `DestroySelf` are now the first dynamic-entity controllers
executed by that pool. A compiled Helper record carries its id, initial state,
P1/P2-relative position, facing, keyctrl and ownpal flags. Normal helpers own
state/animation time, velocity and variables independently, retain parent/root
redirections, may create nested helpers, and invalidate their generational
handle when `DestroySelf` runs. Round reset retires every dynamic entity while
keeping the two root-player handles stable.

Helpers render from their owner's SFF/AIR through the same bounded VDP1
texture cache, and players/helpers are stably ordered together by
`SprPriority`. The compiler deliberately rejects Helper parameters not yet
implemented (for example scaling/remappal/player/projectile helper types)
instead of silently accepting incompatible semantics.

Projectile and Explod both have active runtimes. Projectiles execute through
the same dynamic state/physics step as Helpers, while Explods use a presentation
runtime with source anim/position/velocity/acceleration/removetime and stable
SprPriority ordering. Projectiles can be spawned from the generic
entity runtime API, render through the owner's SFF/AIR path, expose typed
projectile HitDefs, participate in CLSN1-vs-CLSN2 contact and HitOverride AP,
and are consumed on contact. Explod remains presentation-only infrastructure.
Helpers also own persistent/re-armable HitDef state, CLSN1 contact against the
opposing fighter, anti-repeat hit masks, hitpause, movecontact, juggle cost,
damage/guard application and p1stateno transitions.
Helper throws remain deferred until target/bind ownership is generalized, and
helper-vs-player priority/trade arbitration is still resolved in separate
passes rather than one global Ikemen contact queue.

## Runtime controller coverage

The generic CNS runtime currently executes:

* `ChangeState`
* `CtrlSet`
* `PosAdd` / `PosSet`
* `VelSet` / `VelMul`, including conditional custom-state friction
* target/bind/target-state/target-life controllers used by throws
* `ChangeAnim2`, `SelfState`, `HitVelSet`, `PosFreeze`
* constant `VarSet` / `VarAdd`
* bounded `Helper` creation and helper-side `DestroySelf`
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

P1 and P2 have independent generated AIR/SFF tables and independent resident
cart blobs. The VDP1 texture cache key is `(asset slot, sprite index)`, so the
same numeric sprite index from two characters can never alias.

A cache miss performs:

    RAM cart character slot -> aligned WRAM-L source scratch
                            -> SFF decode scratch -> VDP1 VRAM

The example keeps a bounded 32-entry global VDP1 working set and pins both
fighters' current textures before emitting commands. It also looks ahead up
to 32 animation ticks per fighter and stages the next distinct packed sprite
into a small per-player WRAM-L prefetch buffer. A predicted cache hit avoids
the cart read on the transition frame.

The cart store defines P1, P2 and FIGHTFX slots. The fightfx compiler keeps
the SFF palette selected by every referenced sprite instead of forcing one
global palette, and AIR additive drawtypes are emitted as Saturn additive
blend flags. HitDef sparkno/sparkxy data drives actions 0..3 at the authored
impact position; guard contacts use fightfx action 40. Up to two contact
events may be emitted by simulation in one tick so trade sparks are preserved.

The shared texture LRU keys effects with the FIGHTFX asset slot and bounds
resident fightfx textures to three entries, preventing effect palette churn
from consuming every CRAM bank needed by fighters, text and tint variants.

## Still deferred

This is not yet a complete Ikemen common-state VM. The next important pieces are:

* exact remaining downed/defeated presentation semantics such as
  ground effects, get-up/defeated NotHitBy and MatchOver animation variants
* exact remaining guard semantics such as conditional air-guard landing,
  complete inGuardDist behavior
* remaining throw edge cases across different character state/CNS owners;
  AIR ownership is now per fighter
* remaining Blocking edge cases are now mostly advanced compatibility:
  Helper ReversalDef is exercised end-to-end and projectile HitOverride AP is
  exercised end-to-end. Projectile attack attributes no longer alias AA;
  broader Pause movetime/endcmdbuftime compatibility remains beyond KFM's
  authored usage
* remaining HitDef semantics such as reversal, hitonce/chain IDs,
  corner-push and advanced attr interactions
* remaining super presentation semantics: exact per-palette Elecbyte colour
  arithmetic and HUD/lifebar participation in SuperPause darken
* remaining fightfx families (blood, shockwaves and dust),
  motif/lifebar flow and full round presentation
* broader sound-bank/channel semantics beyond the KFM/common sounds currently
  extracted and dispatched
* stage DEF execution instead of the current simplified stage runtime

## Assets and attribution

Sprites, palettes, sounds and Training Room content come from
[Ikemen-GO-Screenpack](https://github.com/ikemen-engine/Ikemen-GO-Screenpack).
Common-state source data comes from
[Ikemen-GO](https://github.com/ikemen-engine/Ikemen-GO).
Kung Fu Man originates from Elecbyte; see the upstream repositories for their
licensing and attribution details. Source checkouts remain under
`.external/`; generated tables are rebuilt locally.
