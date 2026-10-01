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
* START: KFM taunt through the original CMD/State -1 rule
* P2 pad (optional): controls P2; unplugged = idle training dummy

## Source checkouts

## Behavioral oracle

For frame-by-frame compatibility work, `tools/ikemen_oracle` can instrument
the ignored `.external/Ikemen-GO` checkout and emit a deterministic JSONL
trace directly from upstream Ikemen GO. The trace covers root fighters,
Helpers, projectiles, state/animation/physics/contact data, targets and RNG.

```sh
make ikemen-oracle-run
make ikemen-oracle-saturn
make ikemen-oracle-diff
```

Or execute the full upstream-oracle -> host-Saturn -> frame-diff flow:

```sh
make ikemen-oracle-check
```

The oracle is deliberately not linked into the Saturn runtime. See
`tools/ikemen_oracle/README.md` for the trace contract, coordinate
normalization and comparator behavior.

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
Add/sinadd channels use signed 9-bit storage (-255..255), so common-state
flashes such as add=128,128,128 no longer overflow through an int8 path.
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
freeze. Pause/SuperPause movetime now advances only the pause owner while the
opponent and round timer remain frozen; owner HitDefs and authorized
Helpers/Projectiles/Explods continue through their contact and movement paths.
Helper, Projectile and Explod pausemovetime/supermovetime budgets are consumed
independently from the owner's fighter movetime, including both owners'
dynamic entities during Pause/SuperPause. Pause/SuperPause endcmdbuftime now
reaches the command runtime:
patterns preserve command.buffer.pauseend, frozen players retain an existing
buffer during the authored final pause window, and a command completed there
receives Ikemen's extra completion tick. HitOverride is also compiled as a
typed contact window instead of being
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
single synthetic hit state. Common air recovery 5210 now also preserves its
entry PalFX, 15-tick SCA NotHitBy window and the source P2Dist<-20 conditional
Turn before applying the existing recovery velocity controls. HitDef target filtering now honors H/L/M/A/F/D
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

Projectile and Explod both have active runtimes. The earlier state-backed
Projectile path remains available, and the compiler now also lowers the
classic MUGEN Projectile controller to an independent projectile record with
projanim/projhitanim/projremanim/projcancelanim, embedded HitDef, offset,
velocity/velmul/accel, removetime, edge/stage bounds, projhits/projmisstime,
projpriority, sprpriority, ownpal and pause/super-move budgets. Classic
Projectiles also preserve bindtime, removeongethit/removeonchangestate and
P1/P2 postype anchors. They re-arm after projmisstime, can survive multiple
contacts, return from hit animation to the main animation, and resolve
projectile-vs-projectile CLSN1 trades before fighter contacts by decrementing
both priorities. A projectile reaching zero priority follows projcancelanim
when authored; timeout and edge/stage-bound removal now follow projremanim
when present. projstagebound uses the fight's authored stage limits instead of
the screen's 0..320 range. Root projectile query state exposes NumProj,
ProjContact/ProjHit/ProjGuarded and their *Time forms to both State -1
expressions and CNS StateController triggers.

Explods use a separate presentation runtime with source
anim/position/velocity/acceleration/removetime and stable SprPriority ordering.
They also support bindtime, removeongethit, removeonchangestate, ownpal,
pausemovetime/supermovetime, P1/P2 postype anchors, facing/vfacing, scale and
none/alpha/add/sub blend modes; bound Explods follow the selected positional
anchor for the authored lifetime instead of being approximated as Helpers.
Helpers also own persistent/re-armable HitDef state, CLSN1 contact against the
opposing fighter, anti-repeat hit masks, hitpause, movecontact/movehit, juggle
cost, damage/guard application and p1stateno transitions. Helper throws now
capture root fighters through generational target handles; subsequent
TargetBind, TargetFacing, TargetLifeAdd and TargetState controllers execute
from the Helper and the fighter remains bound until release or Helper
invalidation. Normal-frame player/helper/projectile contacts now share one bounded global
Ikemen contact queue for priority and Hit/Miss/Dodge arbitration. Projectile-
vs-projectile priority cancellation remains a deliberate pre-pass, while
Pause/SuperPause keeps its specialized authorized-movement contact path.

## Runtime controller coverage

The generic CNS runtime currently executes:

* `ChangeState`
* `CtrlSet`
* `PosAdd` / `PosSet`
* `VelSet` / `VelMul`, including conditional custom-state friction
* target/bind/target-state/target-life controllers used by throws
* `ChangeAnim2`, `SelfState`, `HitVelSet`, `PosFreeze`
* constant `VarSet` / `VarAdd`
* bounded `Helper`, classic `Projectile` and presentation `Explod`
  creation, plus helper-side `DestroySelf`
* edge-aware wall-bounce movement used by Fast Palm
* HitDef re-trigger/rearm, force-stand, per-hit Y acceleration and ground
  corner-push recoil used by Upper/Blow
* `SprPriority`
* `Width`
* `ChangeAnim` plus the common locomotion animation selectors
* Time, AnimElem, AnimTime, movecontact, command-state and velocity/floor triggers

Unsupported selected behavior is reported or listed in the compiler JSON
rather than silently treated as fully compatible. Common state 100 now
preserves noWalk/noAutoTurn as state-level assertions so noAutoTurn is visible
before the automatic facing step, and common state 106 emits the authored
MakeDust landing effect through fightfx action 120.

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

* downed/defeated common-state presentation now includes the authored
  fightfx ground impact, continuous get-up/defeated SCA NotHitBy windows,
  5140 defeated base animation and the 5150 MatchOver variant when present.
  The narrower post-get-up NT/ST/HT attribute windows remain deferred
* guard start/hold/end now uses HitDef guard.dist (falling back to attack.dist),
  includes Helper/Projectile threats in inGuardDist and chooses air-guard
  landing state 130 only while holdback and inGuardDist remain true; otherwise
  it lands through common state 52
* Throw/custom-state ownership is explicit per player/entity. TargetState
  switches the victim into the attacker's CNS namespace, ChangeState keeps the
  current state owner, and SelfState restores the victim's own CNS namespace.
  Root fighters and Helpers carry state_owner independently, so identical state
  numbers may resolve to different character CNS assets without collision.
* Animation ownership is independent from state ownership. ChangeAnim resolves
  through the character's native AIR, while ChangeAnim2 resolves through the
  current custom-state owner's AIR. ChangeAnim2 still resolves each AIR
  (group,image) against the victim's native SFF, matching Ikemen's separate
  animation-player and sprite-player model. Raw AIR offsets and SFF sprite axes
  are retained in generated assets so remapped animations keep their authored
  placement.
* Character constants are native-character data, not custom-state data. Life,
  size, jump/recovery movement and air-juggle defaults therefore stay with the
  victim while it executes another character's state. ik_fight_init_players()
  initializes P1/P2 from independent CNS assets and preserves independent life
  limits/common-state availability.
* HitDef targets now carry IDs. Dynamic entities keep a fixed four-slot
  generational target registry; TargetBind, TargetFacing, TargetLifeAdd and
  TargetState honor optional id/index selectors and may operate on multiple
  simultaneous targets without allocation. Stale target handles are compacted
  before capture, and TargetState releases only the targets it transitions.
* TargetDrop is lowered with excludeID/keepone semantics (keepone defaults to
  one), NumTarget and NumTarget(ID) are available to CNS triggers and generated
  state-rule expressions, and target(ID,index) redirects carry the full 32-bit
  target ID plus an explicit bounded index through the generic expression VM.
* HitDef chainID/nochainID gates use the last HitDef ID received from the same
  attacking player. The compatibility layer supports the classic two
  NoChainID values while preserving zero-initialized legacy C HitDefs.
* remaining Blocking/engine edge cases are now mostly advanced compatibility:
  Helper ReversalDef and projectile HitOverride AP are exercised end-to-end.
  Pause/SuperPause movetime, paused-owner contact resolution and
  endcmdbuftime command retention are implemented
* remaining classic Projectile/Explod compatibility includes front/back/
  left/right/none postypes, camera-relative edge semantics, depth/height
  bounds, angle/window/remappal and ModifyProjectile. P1/P2 postypes,
  projremanim lifecycle, scale/blend for Explod and Projectile query triggers
  are implemented
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
