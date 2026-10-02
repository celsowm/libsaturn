#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

try:
    from .inputs import setup_string, write_timeline
except ImportError:
    from inputs import setup_string, write_timeline

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

def apply_upstream_build_env(env: dict) -> None:
    """Upstream imports the Go `arena` package (rollback state cloning), which
    only exists under GOEXPERIMENT=arenas; `go run` fails without it."""
    experiments = [e for e in env.get("GOEXPERIMENT", "").split(",") if e]
    if "arenas" not in experiments:
        experiments.append("arenas")
    env["GOEXPERIMENT"] = ",".join(experiments)
    env.setdefault("CGO_ENABLED", "1")
    # Running under this repo's Makefile leaks the SH-2 cross toolchain into
    # the environment (CC=sh2eb-elf-gcc); cgo must build for the host.
    env["CC"] = "gcc"
    env["CXX"] = "g++"
    for var in ("AR", "LD", "CFLAGS", "CXXFLAGS", "CPPFLAGS", "LDFLAGS"):
        env.pop(var, None)

SCREENPACK_DIRS = ("data", "font", "sound", "stages", "chars", "video")

def stage_screenpack(ikemen_dir: Path, screenpack_dir: Path) -> int:
    """The engine repo ships no motif: a runnable Ikemen GO is the engine
    plus the Screenpack's data/font/sound/stages/chars. Merge the Screenpack
    into the engine checkout without overwriting anything already there.
    Both are ignored .external checkouts, so this never touches tracked
    files. Returns the number of files copied."""
    copied = 0
    for name in SCREENPACK_DIRS:
        src_root = screenpack_dir / name
        if not src_root.is_dir():
            continue
        for src in src_root.rglob("*"):
            if not src.is_file():
                continue
            dst = ikemen_dir / name / src.relative_to(src_root)
            if dst.exists():
                continue
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(src, dst)
            copied += 1
    return copied

def missing_prerequisites(env: dict) -> list[str]:
    """Human-readable list of what upstream's cgo build still needs."""
    problems = []
    if shutil.which("go", path=env.get("PATH")) is None:
        problems.append("go toolchain not on PATH")
    if shutil.which("gcc", path=env.get("PATH")) is None:
        problems.append("gcc (cgo) not on PATH")
    pkg_config = shutil.which("pkg-config", path=env.get("PATH")) or \
        shutil.which("pkgconf", path=env.get("PATH"))
    if pkg_config is None:
        problems.append("pkg-config/pkgconf not on PATH")
        return problems
    for module in ("sdl2", "libxmp", "libavcodec", "libavformat",
                   "libavutil", "libswscale", "libswresample"):
        found = subprocess.run(
            [pkg_config, "--exists", module], env=env,
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        ).returncode == 0
        if not found:
            problems.append(f"pkg-config module '{module}' not found")
    return problems

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
    timeline = trace.parent / "inputs.txt"
    write_timeline(timeline, scenario)
    env = os.environ.copy()
    env["LIBSATURN_IKEMEN_ORACLE_TRACE"] = str(trace)
    env["LIBSATURN_IKEMEN_ORACLE_MAX_FRAMES"] = str(
        int(scenario["frames"])
    )
    env["LIBSATURN_IKEMEN_ORACLE_SEED"] = str(int(scenario["seed"]))
    env["LIBSATURN_IKEMEN_ORACLE_ROUND_STATE"] = str(
        int(scenario.get("round_state", 2))
    )
    env["LIBSATURN_IKEMEN_ORACLE_INPUTS"] = str(timeline)
    env["LIBSATURN_IKEMEN_ORACLE_SETUP"] = setup_string(scenario)
    apply_upstream_build_env(env)

    cmd = build_command(scenario)
    if args.dry_run:
        print(json.dumps({
            "schema": SCHEMA,
            "cwd": str(ikemen_dir),
            "command": cmd,
            "trace": str(trace),
            "seed": int(scenario["seed"]),
            "frames": int(scenario["frames"]),
            "inputs": str(timeline),
        }, indent=2))
        return 0

    screenpack = ikemen_dir.parent / "Ikemen-GO-Screenpack"
    if screenpack.is_dir():
        staged = stage_screenpack(ikemen_dir, screenpack)
        if staged:
            print(f"staged {staged} Screenpack files into {ikemen_dir}")

    problems = missing_prerequisites(env)
    if problems:
        raise SystemExit(
            "Ikemen GO cannot be built here:\n  - " + "\n  - ".join(problems)
        )

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
