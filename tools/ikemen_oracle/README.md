# Ikemen GO oracle harness

This harness turns a local Ikemen GO checkout into an executable behavioral
oracle for the LibSaturn Ikemen runtime.

It is intentionally external to the Saturn runtime. The only source mutation is
an idempotent hook installed into the ignored `.external/Ikemen-GO` checkout.

## What it records

One JSON object is emitted per game frame after Ikemen GO finishes
`System.action()`. Schema version 1 records:

- round/tick/RNG state;
- every root fighter and Helper;
- state number/time/type/movetype;
- ctrl, animation, animation element/time;
- position, velocity, facing, life, power and juggle;
- hitpause and move-contact state;
- current GetHitVar chain ID;
- target IDs and HitDef target IDs;
- every projectile with owner, ID, animation, transform, hit/miss timers and
  removal/contact state.

The JSONL format is deliberately simple so the Saturn host runtime can emit the
same rows and be compared frame by frame.

## Install the hook

```sh
python tools/ikemen_oracle/install.py
```

The installer:

1. copies `oracle_hook.go` to
   `.external/Ikemen-GO/src/libsaturn_oracle.go`;
2. injects one hook before `SetupCharRoundStart()` to set the deterministic
   seed;
3. injects one hook immediately after `System.action()` to capture the frame.

It refuses to patch if the expected upstream anchors changed.

To remove it:

```sh
python tools/ikemen_oracle/install.py --uninstall
```

## Run a scenario

```sh
python tools/ikemen_oracle/run.py \
  tools/ikemen_oracle/scenarios/kfm_idle_120.json \
  --install
```

The default trace is written to:

```text
build/ikemen_oracle/ikemen.jsonl
```

A scenario currently controls the native Ikemen GO quick-match CLI:

```json
{
  "p1": "../Ikemen-GO-Screenpack/chars/kfm/kfm.def",
  "p2": "../Ikemen-GO-Screenpack/chars/kfm/kfm.def",
  "stage": "../Ikemen-GO-Screenpack/stages/stage0.def",
  "frames": 120,
  "seed": 1,
  "rounds": 1,
  "round_state": 2,
  "p1_ai": 0,
  "p2_ai": 0,
  "extra_args": []
}
```

The bundled scenario addresses the sibling ignored
`.external/Ikemen-GO-Screenpack` checkout directly, so it does not require
copying KFM or Training Room into the engine checkout.

By default capture starts only when Ikemen reports `RoundState=2`, so motif
and intro frames do not shift the gameplay trace. Set `round_state` to another
value (or `-1` to capture every round state) when a scenario specifically
targets intro/post-round behavior.

The RNG seed is applied before round character setup.

## Deterministic authored inputs

Scenarios may contain a shared logical input timeline. The same timeline is
injected into Ikemen GO before command parsing and converted to Saturn pad
states before `ik_command_update()`, so both sides exercise their real command
engines rather than forcing state numbers.

```json
{
  "inputs": [
    {"from": 10, "to": 25, "p1": ["forward"], "p2": []},
    {"frame": 30, "p1": ["x"], "p2": ["back"]}
  ]
}
```

Supported logical buttons are `forward`, `back`, `up`, `down`, `a`,
`b`, `c`, `x`, `y`, `z` and `start`. Forward/back are resolved
relative to each fighter's current facing on both engines.

The bundled authored scenarios currently cover idle, walk, jump, punch, guard
setup and throw setup.

## Compatibility suite

```sh
make ikemen-oracle-suite
```

The suite runs the upstream oracle, the native LibSaturn host runtime, then the
JSONL comparator for each scenario. It stops at the first divergent scenario
and asks the comparator for exactly one mismatch, giving the first
frame/field that should be investigated. Use `--keep-going` directly with
`suite.py` to collect all failing scenarios.

## Run the LibSaturn host trace

The Saturn fighting runtime is also executed natively on the host with the
generated KFM CNS/AIR assets:

```sh
make ikemen-oracle-saturn
```

This generates:

```text
build/ikemen_oracle/libsaturn.jsonl
```

Coordinates are normalized from the Saturn example's 320x224 screen space to
Ikemen stage-local space: X is relative to screen center and Y is relative to
the fight floor.

## Compare traces

```sh
make ikemen-oracle-diff
```

Or run the whole oracle -> Saturn -> diff pipeline:

```sh
make ikemen-oracle-check
```

The comparator ignores engine-global `tick` and mutable `rand_seed` by
default because LibSaturn does not yet expose equivalent global clocks/RNG.
They remain in both trace schemas for later determinism work. Pass
`--strict-metadata` directly to `diff.py` when those fields become
comparable.

The comparator is strict for integer/boolean/string/state fields and uses a
small absolute tolerance for floating-point coordinates and velocities.

## Important scope

This is a behavioral oracle, not a source-code dependency. Ikemen GO remains in
`.external/Ikemen-GO`; nothing from its Go runtime is linked into LibSaturn.

The current hook is render-independent at capture time but Ikemen GO still
performs its normal platform/window initialization. A later fully headless
backend can remove that startup dependency without changing the trace schema.

The first Saturn runner intentionally uses idle controls. It establishes the
trace contract and exposes baseline divergences. Replay/input-stream injection
is the next layer for authored attacks, throws and controller edge cases.
