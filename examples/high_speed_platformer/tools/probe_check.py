#!/usr/bin/env python3
"""Plays the high_speed_platformer stage in the Ymir harness probe and checks the result.

    python examples/high_speed_platformer/tools/probe_check.py [--shots DIR]

The pad script (harness/high_speed_platformer.pad) runs right with one short jump over the first pit.
The game publishes its counters in `g_hsp_telemetry` (main.c); after the run this reads them out of
work RAM and fails unless the stage was cleared without a death, the loop switched layers twice, and
the speed passed the dash-pad speed. With --shots it also writes screenshots of the run.

Build first with `.\\build-example.ps1 -Example high_speed_platformer`. The pad script's frame numbers
count emulated frames; the game starts its first tick about eight frames in (loading), which is why
the jump is at frame 118 for tick 110 of the host simulation.
"""

from __future__ import annotations

import argparse
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
EXAMPLE = HERE.parent
REPO = EXAMPLE.parents[1]
MAGIC = 0x48535031
FIELDS = ("magic frames ticks x y rings deaths layer_switches platform_ticks rail_grabs landings spawned "
          "despawned top_speed cleared").split()
FORMAT = ">IIIiiIIIIIIIIiI"
WORK_RAM_HIGH = 0x06000000


def find_symbol(nm: str, elf: Path, name: str) -> int:
    out = subprocess.run([nm, str(elf)], capture_output=True, text=True, check=True).stdout
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[2] in (name, "_" + name):
            return int(parts[0], 16)
    raise SystemExit(f"{name} is not in {elf}")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--probe", default=str(REPO / "harness/build/probe.exe"))
    ap.add_argument("--bios", default=str(REPO / "bios/saturn_bios_us.bin"))
    ap.add_argument("--build", default=str(REPO / "build/examples"), help="directory with high_speed_platformer.*")
    ap.add_argument("--nm", default=shutil.which("sh2eb-elf-nm") or "sh2eb-elf-nm")
    ap.add_argument("--pad", default=str(EXAMPLE / "harness/high_speed_platformer.pad"))
    ap.add_argument("--frames", type=int, default=420)
    ap.add_argument("--shots", help="directory for screenshots at a few frames")
    args = ap.parse_args()

    build = Path(args.build)
    elf = build / "high_speed_platformer.elf"
    address = find_symbol(args.nm, elf, "g_hsp_telemetry")
    with tempfile.TemporaryDirectory() as tmp:
        dump = Path(tmp) / "wram.bin"
        cmd = [args.probe, "--iso", str(build / "high_speed_platformer.iso"), "--bios", args.bios,
               "--bin", str(build / "high_speed_platformer.app.bin"), "--frames", str(args.frames),
               "--boot-frames", "90", "--pad-script", args.pad, "--dump-wram-high", str(dump)]
        if args.shots:
            Path(args.shots).mkdir(parents=True, exist_ok=True)
            for frame in (100, 250, args.frames - 20):
                cmd += ["--screenshot", f"{frame}:{Path(args.shots).resolve() / f'frame_{frame}.png'}"]
        subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        data = dump.read_bytes()
    offset = address - WORK_RAM_HIGH
    size = struct.calcsize(FORMAT)
    values = dict(zip(FIELDS, struct.unpack(FORMAT, data[offset:offset + size])))
    values["x"] /= 65536
    values["y"] /= 65536
    values["top_speed"] /= 65536
    print(" ".join(f"{k}={v}" for k, v in values.items() if k != "magic"))

    problems = []
    if values["magic"] != MAGIC:
        problems.append("telemetry magic missing: the game did not run")
    if not values["cleared"]:
        problems.append("the stage was not cleared")
    if values["deaths"] != 0:
        problems.append(f"{values['deaths']} deaths")
    if values["layer_switches"] < 2:
        problems.append("the loop did not switch layers twice")
    if values["top_speed"] < 14:
        problems.append("the hero never passed the dash-pad speed")
    if values["rings"] < 50:
        problems.append("too few rings collected")
    if problems:
        print("FAIL: " + "; ".join(problems), file=sys.stderr)
        return 1
    print("probe check OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
