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
  "p1_ai": 0,
  "p2_ai": 0,
  "extra_args": []
}
```

The bundled scenario addresses the sibling ignored
`.external/Ikemen-GO-Screenpack` checkout directly, so it does not require
copying KFM or Training Room into the engine checkout.

The RNG seed is applied before round character setup. For exact authored input
streams, the next layer should feed Ikemen replay/input data rather than rely on
AI decisions.

## Compare traces

Once the LibSaturn host-side runner emits the same schema:

```sh
python tools/ikemen_oracle/diff.py \
  build/ikemen_oracle/ikemen.jsonl \
  build/ikemen_oracle/libsaturn.jsonl
```

The comparator is strict for integer/boolean/string/state fields and uses a
small absolute tolerance for floating-point coordinates and velocities.

## Important scope

This is a behavioral oracle, not a source-code dependency. Ikemen GO remains in
`.external/Ikemen-GO`; nothing from its Go runtime is linked into LibSaturn.

The current hook is render-independent at capture time but Ikemen GO still
performs its normal platform/window initialization. A later fully headless
backend can remove that startup dependency without changing the trace schema.
