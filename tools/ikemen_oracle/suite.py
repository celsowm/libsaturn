#!/usr/bin/env python3
from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

DEFAULT_SCENARIOS = [
    "kfm_idle_120.json",
    "kfm_walk_120.json",
    "kfm_jump_120.json",
    "kfm_punch_120.json",
    "kfm_guard_160.json",
    "kfm_throw_160.json",
]

def run(cmd: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        cmd,
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "scenarios",
        nargs="*",
        help="scenario file names or paths; defaults to the compatibility set",
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
    requested = args.scenarios or DEFAULT_SCENARIOS

    failures = 0
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
            ],
        ]

        scenario_failed = False
        for step in steps:
            result = run(step)
            if result.stdout:
                print(result.stdout.rstrip())
            if result.returncode != 0:
                scenario_failed = True
                failures += 1
                print(f"FAIL {name}: exit {result.returncode}")
                break

        if not scenario_failed:
            print(f"PASS {name}")

        if scenario_failed and not args.keep_going:
            print("stopping at first divergent scenario")
            return 1

    if failures:
        print(f"{failures} scenario(s) failed")
        return 1
    print(f"oracle suite passed: {len(requested)} scenario(s)")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
