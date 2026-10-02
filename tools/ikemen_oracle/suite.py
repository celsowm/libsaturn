#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# Every scenario file is part of the default set; gen_scenarios.py writes the
# generated matrix next to the hand-written ones.
DEFAULT_SCENARIOS = None

# Saturn keeps positions and velocities in Q8.8 (1/256 px); upstream uses
# float32. Over the scenarios measured so far the drift peaks at 1.1 px / 0.28
# px per tick (jump gravity, guard knockback friction). Remove this profile
# when the fight state moves to Q16.16 (see docs/IKEMEN_COMPAT.md).
Q8_TOLERANCE = ["--pos-eps", "1.5", "--vel-eps", "0.35"]


def run(cmd: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        cmd,
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )

def check_expect(scenario: Path, oracle: Path) -> list[str]:
    """Non-vacuity: the upstream trace must show what the scenario claims."""
    expect = json.loads(scenario.read_text(encoding="utf-8")).get("expect")
    if not expect:
        return []
    rows = [json.loads(l) for l in oracle.read_text(encoding="utf-8")
            .splitlines() if l.strip()]
    chars = [row["chars"] for row in rows]
    problems = []
    for side, key in ((0, "p1_states"), (1, "p2_states")):
        seen = {c[side]["state_no"] for c in chars}
        for state in expect.get(key, []):
            if state not in seen:
                problems.append(f"{key}: state {state} never visited")
    if expect.get("p2_life_drops") is True:
        if min(c[1]["life"] for c in chars) >= chars[0][1]["life"]:
            problems.append("p2 life never dropped (move did not connect)")
    if expect.get("p1_life_drops") is True:
        if min(c[0]["life"] for c in chars) >= chars[0][0]["life"]:
            problems.append("p1 life never dropped")
    return problems


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "scenarios",
        nargs="*",
        help="scenario file names or paths; defaults to every scenario",
    )
    parser.add_argument(
        "--ikemen-dir",
        type=Path,
        default=Path(".external/Ikemen-GO"),
    )
    parser.add_argument(
        "--keep-going",
        action="store_true",
        help="continue after mismatches instead of stopping at the first one",
    )
    args = parser.parse_args()

    tool = ROOT / "tools" / "ikemen_oracle"
    scenario_dir = tool / "scenarios"
    requested = args.scenarios or sorted(
        p.name for p in scenario_dir.glob("*.json"))

    failures = 0
    pending = 0
    for raw in requested:
        scenario = Path(raw)
        if not scenario.is_absolute() and not scenario.exists():
            scenario = scenario_dir / scenario
        scenario = scenario.resolve()
        name = scenario.stem
        out_dir = ROOT / "build" / "ikemen_oracle" / "suite" / name
        out_dir.mkdir(parents=True, exist_ok=True)
        oracle = out_dir / "ikemen.jsonl"
        saturn = out_dir / "libsaturn.jsonl"

        print(f"== {name} ==")

        steps = [
            [
                sys.executable,
                str(tool / "run.py"),
                str(scenario),
                "--ikemen-dir",
                str(args.ikemen_dir.resolve()),
                "--trace",
                str(oracle),
                "--install",
            ],
            [
                sys.executable,
                str(tool / "run_saturn.py"),
                str(scenario),
                "--trace",
                str(saturn),
            ],
            [
                sys.executable,
                str(tool / "diff.py"),
                str(oracle),
                str(saturn),
                "--limit",
                "1",
                *Q8_TOLERANCE,
            ],
        ]

        # A scenario that needs a feature the port does not have yet says so
        # in "pending": its divergence is reported, but does not fail the
        # suite. It must start failing the suite as soon as it matches.
        reason = json.loads(
            scenario.read_text(encoding="utf-8")).get("pending")

        scenario_failed = False
        for step in steps:
            result = run(step)
            if result.stdout:
                print(result.stdout.rstrip())
            if result.returncode != 0:
                scenario_failed = True
                if reason:
                    pending += 1
                    print(f"PENDING {name}: {reason}")
                else:
                    failures += 1
                    print(f"FAIL {name}: exit {result.returncode}")
                break

        if not scenario_failed:
            for problem in check_expect(scenario, oracle):
                scenario_failed = True
                print(f"VACUOUS {name}: {problem}")
            if scenario_failed:
                failures += 1
                print(f"FAIL {name}: expectation not met")
            elif reason:
                scenario_failed = True
                failures += 1
                print(f"FAIL {name}: matches now; drop its \"pending\" entry")
        if not scenario_failed:
            print(f"PASS {name}")

        if scenario_failed and not reason and not args.keep_going:
            print("stopping at first divergent scenario")
            return 1

    if failures:
        print(f"{failures} scenario(s) failed")
        return 1
    note = f" ({pending} pending)" if pending else ""
    print(f"oracle suite passed: {len(requested)} scenario(s){note}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
