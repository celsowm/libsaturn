# Ikemen GO compatibility (ikemen_saturn)

Goal: the Saturn port of Kung Fu Man (classic CNS/CMD) behaves 1:1 like upstream
Ikemen GO, measured frame by frame by the oracle in `tools/ikemen_oracle`. ZSS
characters are out of scope for now (the "P2 ZSS" is only a sprite/AIR label).

## Running it

From the MSYS2 **ucrt64** shell (Windows Go on PATH, e.g. `/e/Program Files/Go/bin`):

    make ikemen-oracle-suite            # all default scenarios, first divergence each
    python tools/ikemen_oracle/suite.py --keep-going kfm_walk_120

Upstream needs SDL2, libxmp and ffmpeg dev packages from `pacman` (ucrt64) and
the Screenpack checkout; `run.py` reports anything missing, sets
`GOEXPERIMENT=arenas`, forces the host C compiler and merges the Screenpack
into `.external/Ikemen-GO`. The Saturn side is the host build of the same code
the console runs: `main.c` and `saturn_trace.cpp` both call `ik_frame_step()`
(`examples/ikemen_saturn/ikemen_frame.c`).

## Trace contract (what is compared)

One JSON row per frame, both sides, captured after upstream's whole
`System.action()`. Normalised so the two agree:

- ids number from 1 (P1 = 1, P2 = 2, spawns in creation order);
- `state_type` S,C,A,L = 0..3 and `move_type` I,A,H = 0..2 (the **stored**
  type, which a hit changes at once and a StateDef changes only when it is
  initialised);
- `state_time` and `anim_time` are the post-tick values; `anim_time`/`anim_elem`
  follow upstream `Animation.curtime/curelem` (`ik_anim_trace_state`);
- `vel` is **local** (forward is positive), like upstream; Saturn keeps world
  velocity and the trace multiplies by `facing`;
- `juggle` is the juggle budget left against whoever hit this fighter (0 before
  the first hit or after leaving MoveType H), like `ghv.targetedBy`;
- `move_contact_type` (0 hit, 1 guarded) survives state changes,
  `move_contact_time` counts ticks since the contact;
- `hitdef_targets` lasts until the HitDef is reset, `targets` until the target
  leaves MoveType H;
- lists are `[]`, never `null`; `tick` and `rand_seed` are ignored until the
  Saturn side has a real global clock and RNG.

## Tick order (what the engine now does, same as upstream)

Per fighter, `ikemen_fight_step.c`:

1. start of tick: effect timers, pending power, StateDef of a state entered from
   outside (`statedef_pending`);
2. hard-coded keys (jump, crouch, walk/brake; walking back is cancelled while
   `inGuardDist`), then the State -1 request;
3. the current state's controllers (a `ChangeState` re-runs the new state in the
   same tick: controllers at Time 0, then physics);
4. guard entry (`inGuardDist` + holding back), physics, controllers of a state
   the physics entered (landing);
5. `state_time` advances, PalFX steps; **after contacts** `anim_time` advances
   (so contact boxes see the frame the controllers evaluated with).

Hits and throws change the target's state number at once but its StateDef
(anim, ctrl, velset, type) only when it next acts. `inGuardDist` is recomputed
after contacts each tick and read by the next tick. Guard shake uses
`hit_shake_time` (upstream `ghv.hitshaketime`), not a freeze.

## Fight runtime layout

`ikemen_fight.c` was a 4900-line translation unit; it is now one responsibility
per file (`ikemen_fight_internal.h` holds the shared inline lookups and the
`ikf_`-prefixed cross-file API; `ikemen_fight.h` stays the public API):

| File | Responsibility |
|---|---|
| `ikemen_fight.c` | lifecycle: spawn, init, reset, status text |
| `ikemen_fight_state.c` | `ChangeState` (number vs StateDef), hard-coded input |
| `ikemen_fight_step.c` | per-fighter tick order, push, clocks |
| `ikemen_fight_motion.c` | ground/air physics, body push |
| `ikemen_fight_ctrl.c` + `_ctrl_{motion,anim,state,fx,spawn,target}.c` | controller executor and its families |
| `ikemen_fight_geom.c` | body/Clsn geometry, animation position |
| `ikemen_fight_hitdef.c` / `_hitrules.c` | active HitDef/ReversalDef, hit eligibility, guard distance, power |
| `ikemen_fight_hit.c` / `_hit_entity.c` | applying a hit (root fighter / helper attacker) |
| `ikemen_fight_contacts.c` / `_contact_queue.c` | contact resolution and arbitration |
| `ikemen_fight_entities.c` | fighter <-> entity pool mirror and runtime bridge |
| `ikemen_fight_fx.c` | sound/effect events |
| `ikemen_fight_update.c` | frame orchestration (pause, timer, KO) |

The split was verified byte-identical on all six scenario traces before any
behavior change.

## Status (2026-10-02)

| Scenario | Result |
|---|---|
| kfm_idle_120 | PASS |
| kfm_walk_120 | PASS |
| kfm_jump_120 | PASS |
| kfm_punch_120 | PASS (P1 punches, P2 out of reach: asserted by `expect`, no hit) |
| kfm_guard_160 | PASS (guard stance, shake, knockback, release, power, juggle) |
| kfm_throw_160 | PASS (camera bounds, wall, 5100/5101 bounce, get-up anim) |
| kfm_corner_300 | PASS (walk into P2 across the stage: push, camera scroll, P2 pinned at the right bound 270) |
| kfm_stand_x/y/a/b_160 | PASS (generated; approach, one standing normal that connects: shake, damage, 5001 slide, recovery) |
| kfm_crouch_x/a/b_160 | PASS (generated; hardcoded crouch/stand-up, crouch normals, 5070 trip shake and flight, landing in 5110) |
| kfm_fireball_x_160 | PASS (generated; QCF+x connects as the fall hit: 5030 launch, 5035/5050, landing) |
| kfm_qcf_y/a/b, qcb_x/y, upper_x/y, ff_a _200 | PASS (generated; every KFM special/super-less move reachable by motion: multi-hit upper with Up anim 5051/5061 and 5100/5101 landing, VelMul friction) |
| kfm_air_x/y/a/b_200 | PASS (generated; neutral jump from contact range, attack on the way down; hit timing depends on sub-pixel boxes) |
| kfm_run_200, kfm_hop_back_200 | PASS (generated; run, and hop-back whose landing is its own controller) |
| kfm_super_* _200 (palm_xy, upper_xy, blow_xy, knee_ab, zankou_ab, triple_palm, smash_upper) | PASS (generated; P1 starts with 1000 power via `setup`; power cost and `getpower = 0`, wall bounce 1026/1027, multi-hit supers, `mindist/maxdist` of 3050) |
| kfm_combo_* _200 (chain_x_y, chain_x_a, rapid_x, cancel_x_qcf_y, cancel_x_upper_x, cancel_y_blow_y) | PASS (generated; chains and move-contact cancels start right after the attacker's hit pause) |
| kfm_p2_stand_x, p2_crouch_a, p2_qcf_y, p2_upper_y _200, kfm_p2_throw_160 | PASS (generated; P2 as the attacker: run order, bind countdown) |
| kfm_throw_back_160 | PASS (back + y throw) |
| kfm_guard_stand_y/b, guard_crouch_a/x, guard_air _160 | PASS (generated; both players script their inputs; crouch guard 131/152, air guard 132/154/155) |
| kfm_blocking_high_120, blocking_low_120, blocking_hit_160, blocking_low_hit_160, blocking_air_hit_160, blocking_air_air_200 | PASS (the `F,x` blocking command, ReversalDef of 1300/1320/1340, reversed attacker bookkeeping) |
| kfm_upper_getup_320 | PASS (lie down 5110 and get up 5120) |
| kfm_recovery_200 | PASS (air recovery 5210 with `x+y` taps, landing in 52) |
| kfm_ko_stand_x_200, kfm_ko_upper_y_240 | PENDING (a KO starts roundState 3, KO slow motion and the win pose; see below) |

`tools/ikemen_oracle/gen_scenarios.py` writes the generated rows; every
scenario may carry an `expect` block (`p1_states`, `p2_states`,
`p2_life_drops`, `p1_life_drops`) that `suite.py` checks on the **upstream**
trace, so a PASS where the move never connected fails as `VACUOUS` (a state
only counts when it is visible at the end of a tick: a state entered and left
in one tick, like 1340 reversed at once, is not).

Scenario inputs are per side: an event that names only `p1` (or only `p2`)
leaves the other player's script alone, so two players' timelines can overlap.
`setup` (`p1_life`, `p1_power`, `p2_life`, `p2_power`) overrides the starting
values on both sides (the hook applies it before the first scripted tick).
A scenario with `pending` documents a feature the port does not have yet:
`suite.py` reports `PENDING <reason>` instead of failing, and fails once the
scenario matches, so the entry is dropped. With no arguments `suite.py` runs
every file in `scenarios/` (63 now: 61 pass, 2 pending).

### What the matrix taught the engine (all measured against upstream)

- Defender shake: the victim is not in hit pause. It runs the shake state
  (5000/5010/5020/5070) with `Time` advancing, the shaking animation restarted
  every tick, and leaves on `HitShakeOver`. Saturn used `hit_pause` for it and
  froze the state; now only `hit_shake_time` is used, and the damage lands one
  tick after the hit (`pending_damage`, applied when the victim next acts).
- HitOver is `hittime < 0`, so `hitstun` starts at `hit_time + 1`.
- Hardcoded keys: entering state 10 zeroes `vel x` (except from run, 100);
  any crouching state with ctrl stands up through 12 when down is released;
  any standing state with ctrl starts walking (`noWalk` states opt out).
- Stand physics zeroes `|vx| < 1` after friction; crouch does not.
- Get-hit flight states (5030/5035/5040/5050/5071/5200) run `VelAdd` from
  `Time 1`, so gravity lands before the move, not on the launch tick, and a
  `ChangeState` between them keeps the old state's `VelAdd`; their landing is a
  controller test on the position from before the move.
- Run order is upstream's `updateRunOrder`: attackers (MoveType A) first, then
  idle players, then the rest, ties by player number. A TargetBind lasts one tick
  *of the bound fighter*, so a victim that runs before its binder still sees the
  bind. Guard entry (`inGuardDist` + back) happens right after state -1 and
  before the state's own controllers, and `inGuardState()` is 120, 130-132,
  140 and 150-155. The hardcoded crouch only starts from a stand-type state.
- Contacts bookkeeping: a freshly executed HitDef clears the HitDef targets;
  attackers drop targets that left MoveType H; a ReversalDef marks the attacker
  `MC_Reversed` (MoveContact 0, HitDef target = reverser, juggle budget kept
  until it is hit again) and the reverser scores a hit.
- Width has separate player and edge widths (edge 0 by default, reset every
  tick); `BackEdgeBodyDist`/`FrontEdgeBodyDist` are Q8.8 distances to the
  screen edge minus the edge width and 0.5 px (air) / 1 px (lying); push uses
  Q8.8 size boxes, the widths of the *current* state type and also needs the
  hurt boxes (Clsn2) to touch. `ScreenBound` flags (value, movecamera) drive
  the clamp and which fighters the camera tracks (KFM 1026/1027).
- Pre-move gravity: states whose VelAdd is a controller (air guard 132/155,
  air recovery 5210 from its fourth tick) add it before the move; a ChangeState
  carries the old state's VelAdd into a non-flight successor.
- `HitDef` defaults: `yaccel` 0.35, `fall.animtype` (explicit, else
  `air.animtype` when Up/DiagUp, else Back); anim type resolves fall > air >
  ground, and a non-launching Back/Up/DiagUp ground hit plays as Hard. Shake
  animation: 5051/5052 for Up/DiagUp when present, 5030 for Back, else
  5000/5010/5020 + type; knocked-back anim 5005/5015/5025 + type.

Host tests: all `test_ikemen_*` pass with no tracked gaps. `tests/tools/test_ikemen_cns.py` passes again (explod 191 now compiles: explod fields are int32, `q8(wide=True)`; AfterImage with default palette keys compiles).

## Known shims and gaps

- **Round flow (partial).** The `--align-to` shim is gone: fighters spawn at
  `stage.p1_start_x/p2_start_x` (+-70) and RoundState 1 holds
  `IK_ROUND_FIGHT_WAIT_TICKS` (111 here, measured so the first traced row has
  state_time 113) after the intro state ends. The constant comes from the stock
  screenpack's "Round 1 / Fight" timings, not from a parsed `fight.def`.
- **Precision (decided: targeted Q8.16, not a full Q16.16 migration).** The
  fight state stays Q8.8 (1/256 px; walk speed 2.0390625 vs upstream 2.04), but
  the places where Q8.8 rounding accumulates every tick carry more bits:
  gravity (`yaccel`, HitDef `yaccel`, air-recover `yaccel`) and stand/crouch
  friction are Q16.16 constants (`*_q16`, 0 means "use Q8.8" for hand-built
  fixtures) and `ikf_apply_ground_velocity` / `ikf_step_air` integrate in
  Q8.16 using a fraction byte beside x/y/vx/vy (`ik_fine_t`, valid only while
  the Q8.8 value is unchanged, so any other writer drops it). Clsn boxes are
  compared in Q8.8 world units (`ik_frame_clsn_world_q8`), as upstream compares
  floats: a pixel-rounded position moved hits by a tick. Without these, hop-back,
  air normals and the long upper fall diverged by whole ticks. VelSet/VelAdd
  controllers carry their authored value as Q16.16 too (`value4/5`). Drift left
  in the scenarios peaks around 1 px (long walks: 2.4 vs 2.3984 per tick), so
  `suite.py` still compares with `--pos-eps 1.5 --vel-eps 0.35`; walk/run/jump
  constants as Q16.16 would let that drop to ~0.3 px.
- **Camera / stage bounds (done, stage0 defaults).** `ikemen_fight_camera.c`
  ports upstream `Camera.action` (tension 50, +-125 bounds, 1/4 px snap, no
  zoom, no vertical follow); players are clamped to `xmin/xmax` after the
  camera step, as `xScreenBound` does. `main.c` renders with the camera x but
  that path is only checked by probe screenshots at the centre. `ScreenBound`
  and `Width` edge widths feed the clamp and the tracked box. Missing: other
  stages' parameters, camera x in the traces.
- **Entity runtime tick order (done).** Helpers and state-driven projectiles now
  enter a state with `Time 0`, evaluate it on the entry tick, re-run on
  `ChangeState` in the same tick, advance `state_time` at the end and advance
  the animation after contacts (`ik_entity_runtime_finish_tick`). The 14 old
  `GAP_*` assertions are ordinary assertions: most were fixtures that did not
  follow the compiler's defaults (`p1/p2stateno -1`, target id -1, Time 0
  numbering, projectile animations -1, overlapping Clsn). Explods and
  Projectile-controller shots (state < 0) have no controllers and keep their
  own simple clock. Not yet measured by the oracle: no helper scenario exists.
- Hit-state gravity: 5030/5035/5040/5050/5071/5200 and 5100/5101/5110 follow
  upstream; other hit states (5080/5081, 5120, 5150, 5201...) still use the
  engine default and no matrix scenario reaches them (KO and lying down are the
  next ones).
- Hand-built common states (`tools/ikemen_cns.py`, states 0..5210) keep authored
  `Time` values from `common1.cns.zss` but are audited only for the states the
  scenarios visit; the rest should be checked against the zss when new scenarios
  reach them.
- **KO and the round flow (pending scenarios).** When a character's life
  reaches 0 upstream enters roundState 3 on that tick: the engine slows down
  (`fight.def` `round.slow.time/speed`, `tick` advances about every 4th frame,
  so the trace has runs of frames with unchanged state), the KO'd fighter is
  forced into the fall (`ghv.fallflag`, extra KO velocity from the
  `[Velocity]` `*.gethit.ko.*` constants) and the others go to the win pose.
  Saturn freezes at `round_over` and has none of that, so
  `kfm_ko_*` stay `pending` until stage F implements the round states, KO slow
  motion, win poses, timeout and the next round. A round-complete scenario and
  a timeout scenario need that same work (plus a way to shorten the round
  timer in the hook).
- Compiler gaps still open (`--strict` will list them): `Explod` 1027
  (`floor(screenpos y)`), `command="holdback"/"blocking"` ChangeState rows of
  1310/1330/1350/1351 (the oracle scenarios pass because the hit reversal path
  does not need them), 1400-1420 `VelMul x=0.5`, AfterImage 1420,
  `[Statedef -2/-3]`, common states 110/115/175/190/5500/5900.
  `SelfAnimExist(n)` is resolved while compiling against the character's
  `.air` (`--air`). `tests/tools/*ikemen*` pass.

## Other measured gaps (not yet scheduled in the oracle)

See `examples/ikemen_saturn/README.md` "Still deferred" and the plan: round flow
(win poses never run), defence/fall-defence scaling, `[Statedef -3]` sounds,
sound banks, explods/projectiles/helpers (entity runtime), HUD power bar.
