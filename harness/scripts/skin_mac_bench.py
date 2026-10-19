#!/usr/bin/env python3
"""Build and run the procedural Q16 skinning C / SH-2 MAC.L control in Ymir."""
import argparse
import hashlib
import json
import os
import shutil
import struct
import subprocess
import sys
from pathlib import Path

from cube_pipeline import ROOT, symbols

TELEMETRY_HEADER_WORDS = 8
SAMPLE_WORDS = 9
SAMPLE_COUNT = 5
EXPECTED_CHECKS = 7544
EXPECTED_ROWS = 2
EXPECTED_REPETITIONS = 32
EXPECTED_VERTICES = 943
EXPECTED_MAGIC = 0x534B4D31
EXPECTED_VERSION = 1
KERNELS = ("libsaturn_c_api", "mac_l")


def decode(raw):
    expected_bytes = (TELEMETRY_HEADER_WORDS + EXPECTED_ROWS * SAMPLE_WORDS) * 4
    if len(raw) != expected_bytes:
        raise ValueError("invalid skinning telemetry size")
    words = struct.unpack(f">{expected_bytes // 4}I", raw)
    magic, version, frequency, done, errors, checks, ccr, rows = words[:TELEMETRY_HEADER_WORDS]
    if (magic, version, done, errors, checks, rows) != (
        EXPECTED_MAGIC, EXPECTED_VERSION, 1, 0, EXPECTED_CHECKS, EXPECTED_ROWS
    ) or frequency == 0:
        raise ValueError("failed or incomplete skinning qualification")
    samples = []
    for index in range(EXPECTED_ROWS):
        row = words[TELEMETRY_HEADER_WORDS + index * SAMPLE_WORDS:
                    TELEMETRY_HEADER_WORDS + (index + 1) * SAMPLE_WORDS]
        kernel, region, repetitions = row[:3]
        ticks, checksum = row[3:3 + SAMPLE_COUNT], row[-1]
        if (kernel, region, repetitions) != (index, 0, EXPECTED_REPETITIONS):
            raise ValueError("unexpected skinning case order or parameters")
        if any(tick == 0 or tick >= frequency for tick in ticks):
            raise ValueError("invalid skinning timer interval")
        samples.append({
            "kernel": KERNELS[index], "ticks": list(ticks), "checksum": checksum,
            "median_ticks_per_vertex": sorted(ticks)[SAMPLE_COUNT // 2]
                / EXPECTED_REPETITIONS / EXPECTED_VERTICES,
        })
    if samples[0]["checksum"] != samples[1]["checksum"]:
        raise ValueError("C and MAC.L skinning checksums differ")
    c_ticks = samples[0]["median_ticks_per_vertex"]
    mac_ticks = samples[1]["median_ticks_per_vertex"]
    return {
        "frequency_hz": frequency,
        "ccr": ccr,
        "checks": checks,
        "vertices": EXPECTED_VERTICES,
        "poses": 4,
        "samples": samples,
        "mac_l_change_percent": (mac_ticks / c_ticks - 1.0) * 100.0,
        "scope": "Procedural 220-rigid/723-blended Q16 skin records, including 1/2/3 influences. "
                 "C API includes argument checks; MAC.L range is prevalidated and saves/restores SR.S "
                 "once per batch. Timing includes transform, Q15 blending and output writes. "
                 "Ymir timing does not certify physical Saturn cycles.",
    }


def run(args):
    out = args.out.resolve()
    if out.exists() and any(out.iterdir()):
        raise FileExistsError("use a fresh empty output directory")
    out.mkdir(parents=True, exist_ok=True)
    compiler_dir = args.compiler_dir.resolve()
    env = dict(os.environ)
    env["PATH"] = str(compiler_dir) + os.pathsep + env.get("PATH", "")
    build_command = [args.make, "all", "EXAMPLE=skin_mac_bench",
                     "PYTHON=" + sys.executable,
                     "EXAMPLE_INCFLAGS=-" + args.optimization, "-j4"]
    with (out / "build.log").open("w") as log:
        subprocess.run(build_command, cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT,
                       check=True, timeout=args.timeout)
    for suffix in (".elf", ".map", ".app.bin", ".iso", ".bin", ".cue"):
        shutil.copyfile(ROOT / "build" / ("skin_mac_bench" + suffix),
                        out / ("skin_mac_bench" + suffix))
    elf = out / "skin_mac_bench.elf"
    found = symbols(elf, compiler_dir / "sh2eb-elf-nm")
    telemetry_address, telemetry_size = found["g_skin_mac_bench"]
    panic_address, panic_size = found["g_sat_last_panic"]
    command = [str(args.probe.resolve()), "--iso", str(out / "skin_mac_bench.iso"),
               "--bin", str(out / "skin_mac_bench.app.bin"), "--bios", str(args.bios.resolve()),
               "--boot-frames", "90", "--frames", str(args.fields),
               "--sh2-cache", args.cache, "--slave-reset-entry", hex(found["saturn_slave_entry"][0]),
               "--dump-mem", f"{telemetry_address:#x}:{telemetry_size}:{out}/telemetry.bin",
               "--dump-mem", f"{panic_address:#x}:{panic_size}:{out}/panic.bin",
               "--out", str(out / "ymir.json"), "--screenshot",
               f"{args.fields - 1}:{out}/screen.png"]
    (out / "command.json").write_text(json.dumps(command, indent=2) + "\n")
    with (out / "probe.log").open("w") as log:
        subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT,
                       check=True, timeout=args.timeout)
    panic = (out / "panic.bin").read_bytes()
    if len(panic) != panic_size or any(panic):
        raise ValueError("native skinning benchmark panic")
    report = decode((out / "telemetry.bin").read_bytes())
    metadata = {
        "build_command": build_command,
        "source_sha256": hashlib.sha256(
            (ROOT / "examples/skin_mac_bench/main.c").read_bytes()).hexdigest(),
        "compiler_version": subprocess.check_output(
            [str(compiler_dir / "sh2eb-elf-gcc"), "--version"], text=True).splitlines()[0],
        "fields": args.fields, "optimization": args.optimization, "cache": args.cache,
        "checkout": subprocess.check_output(["git", "describe", "--always", "--dirty"],
                                             cwd=ROOT, text=True).strip(),
        "bios_sha256": hashlib.sha256(args.bios.read_bytes()).hexdigest(),
        "probe_sha256": hashlib.sha256(args.probe.read_bytes()).hexdigest(),
        "elf_sha256": hashlib.sha256(elf.read_bytes()).hexdigest(),
    }
    (out / "capture.json").write_text(json.dumps(metadata, indent=2) + "\n")
    (out / "analysis.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"Validated {report['checks']} exact skinning comparisons; "
          f"MAC.L change {report['mac_l_change_percent']:.2f}%; {out}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bios", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--fields", type=int, default=600)
    parser.add_argument("--timeout", type=int, default=300)
    parser.add_argument("--compiler-dir", type=Path,
                        default=Path.home() / "dev/saturn-tools/bin")
    parser.add_argument("--probe", type=Path, default=ROOT / "harness/build-macos/probe")
    parser.add_argument("--optimization", choices=("O2", "O3"), default="O2")
    parser.add_argument("--cache", choices=("on", "off"), default="on")
    parser.add_argument("--make", default=shutil.which("gmake") or "make")
    options = parser.parse_args()
    if options.fields <= 0 or options.timeout <= 0:
        parser.error("fields and timeout must be positive")
    run(options)
