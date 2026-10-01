#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

HOST_BIN = ROOT / "build" / "ikemen_oracle" / "saturn_trace"

def load_scenario(path: Path) -> dict:
    data = json.loads(path.read_text(encoding="utf-8"))
    if int(data.get("frames", 0)) <= 0:
        raise SystemExit("scenario.frames must be > 0")
    return data

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("scenario", type=Path)
    parser.add_argument(
        "--trace",
        type=Path,
        default=Path("build/ikemen_oracle/libsaturn.jsonl"),
    )
    parser.add_argument(
        "--no-generate",
        action="store_true",
        help="reuse existing generated KFM assets",
    )
    args = parser.parse_args()

    scenario = load_scenario(args.scenario)
    frames = int(scenario["frames"])
    seed = int(scenario.get("seed", 1))
    trace = args.trace.resolve()
    trace.parent.mkdir(parents=True, exist_ok=True)
    HOST_BIN.parent.mkdir(parents=True, exist_ok=True)

    if not args.no_generate:
        subprocess.run(
            [
                "make",
                "EXAMPLE=ikemen_saturn",
                "build/generated/ikemen_saturn/kfm_frames.c",
                "build/generated/ikemen_saturn/kfm_cns.c",
            ],
            cwd=ROOT,
            check=True,
        )

    sources = [
        "tools/ikemen_oracle/saturn_trace.cpp",
        "examples/ikemen_saturn/ikemen_fight.c",
        "examples/ikemen_saturn/ikemen_anim.c",
        "examples/ikemen_saturn/ikemen_cns.c",
        "examples/ikemen_saturn/ikemen_entity.c",
        "examples/ikemen_saturn/ikemen_entity_runtime.c",
        "build/generated/ikemen_saturn/kfm_frames.c",
        "build/generated/ikemen_saturn/kfm_cns.c",
    ]
    compile_cmd = [
        "g++",
        "-std=c++20",
        "-Wall",
        "-Wextra",
        "-O1",
        "-Iinclude",
        "-I.",
        "-Ibuild/generated",
        *sources,
        "-o",
        str(HOST_BIN),
    ]
    subprocess.run(compile_cmd, cwd=ROOT, check=True)
    subprocess.run(
        [str(HOST_BIN), str(trace), str(frames), str(seed)],
        cwd=ROOT,
        check=True,
    )

    count = 0
    with trace.open("r", encoding="utf-8") as f:
        for line in f:
            if line.strip():
                count += 1
    if count != frames:
        raise SystemExit(
            f"Saturn trace has {count} frames, expected {frames}"
        )
    print(f"saturn trace: {trace} ({count} frames)")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
