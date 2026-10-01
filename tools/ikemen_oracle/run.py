#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

SCHEMA = 1

def load_scenario(path: Path) -> dict:
    data = json.loads(path.read_text(encoding="utf-8"))
    required = ("p1", "p2", "frames", "seed")
    missing = [k for k in required if k not in data]
    if missing:
        raise SystemExit(f"scenario missing keys: {', '.join(missing)}")
    if int(data["frames"]) <= 0:
        raise SystemExit("scenario.frames must be > 0")
    return data

def build_command(scenario: dict) -> list[str]:
    cmd = [
        "go", "run", "./src",
        "-p1", str(scenario["p1"]),
        "-p2", str(scenario["p2"]),
        "-rounds", str(scenario.get("rounds", 1)),
        "-windowed",
        "-nomusic",
        "-nosound",
    ]
    stage = scenario.get("stage")
    if stage:
        cmd += ["-s", str(stage)]
    p1_ai = scenario.get("p1_ai")
    p2_ai = scenario.get("p2_ai")
    if p1_ai is not None:
        cmd += ["-p1.ai", str(p1_ai)]
    if p2_ai is not None:
        cmd += ["-p2.ai", str(p2_ai)]
    cmd += [str(v) for v in scenario.get("extra_args", [])]
    return cmd

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("scenario", type=Path)
    parser.add_argument(
        "--ikemen-dir",
        type=Path,
        default=Path(".external/Ikemen-GO"),
    )
    parser.add_argument(
        "--trace",
        type=Path,
        default=Path("build/ikemen_oracle/ikemen.jsonl"),
    )
    parser.add_argument(
        "--install",
        action="store_true",
        help="install/update the source hook before running",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
    )
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[2]
    tool_dir = Path(__file__).resolve().parent
    ikemen_dir = args.ikemen_dir.resolve()
    trace = args.trace.resolve()
    scenario = load_scenario(args.scenario)

    if args.install:
        subprocess.run(
            [
                sys.executable,
                str(tool_dir / "install.py"),
                "--ikemen-dir",
                str(ikemen_dir),
            ],
            check=True,
            cwd=repo_root,
        )

    hook = ikemen_dir / "src" / "libsaturn_oracle.go"
    if not hook.is_file():
        raise SystemExit(
            "oracle hook is not installed; run with --install first"
        )

    trace.parent.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["LIBSATURN_IKEMEN_ORACLE_TRACE"] = str(trace)
    env["LIBSATURN_IKEMEN_ORACLE_MAX_FRAMES"] = str(
        int(scenario["frames"])
    )
    env["LIBSATURN_IKEMEN_ORACLE_SEED"] = str(int(scenario["seed"]))

    cmd = build_command(scenario)
    if args.dry_run:
        print(json.dumps({
            "schema": SCHEMA,
            "cwd": str(ikemen_dir),
            "command": cmd,
            "trace": str(trace),
            "seed": int(scenario["seed"]),
            "frames": int(scenario["frames"]),
        }, indent=2))
        return 0

    completed = subprocess.run(cmd, cwd=ikemen_dir, env=env)
    if completed.returncode != 0:
        return completed.returncode
    if not trace.is_file():
        raise SystemExit("Ikemen GO finished without producing oracle trace")

    frame_count = 0
    with trace.open("r", encoding="utf-8") as f:
        for line in f:
            if line.strip():
                frame_count += 1
    expected = int(scenario["frames"])
    if frame_count != expected:
        raise SystemExit(
            f"oracle trace has {frame_count} frames, expected {expected}"
        )
    print(f"oracle trace: {trace} ({frame_count} frames)")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
