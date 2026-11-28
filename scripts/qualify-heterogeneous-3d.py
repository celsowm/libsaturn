#!/usr/bin/env python3
"""Build and qualify the SCU DSP experiment on macOS, then archive the evidence."""

import argparse
import csv
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from harness.scripts import renderer_visual as visual
from harness.scripts.sh2_kernel_verify_mednafen import elf_text, normalize, sha, work_ram
from harness.scripts.telemetry import WRAM_HIGH, find_symbol
from harness.tests import test_heterogeneous_3d_bench as schema

EXAMPLE = "heterogeneous_3d_bench"
BUILD = ROOT / "build"


def capture(command):
    return subprocess.check_output(command, cwd=ROOT, text=True).strip()


def run(command, log, timeout=480, env=None):
    print(f"{' '.join(map(str, command))} -> {log.name}", flush=True)
    started = time.monotonic()
    with log.open("w") as output:
        subprocess.run(command, cwd=ROOT, stdout=output, stderr=subprocess.STDOUT,
                       check=True, timeout=timeout, env=env)
    return round(time.monotonic() - started, 3)


def write_json(path, data):
    path.write_text(json.dumps(data, indent=2) + "\n")


def ymir(out, bios, probe_dir, fields):
    map_path = BUILD / f"{EXAMPLE}.map"
    mark = WRAM_HIGH + find_symbol(map_path, "_g_heterogeneous_scene_capture")
    command = ["./harness/run-harness.sh", EXAMPLE, "--bios", str(bios),
               "--build-dir", str(probe_dir), "--frames", str(fields),
               "--out", str(out / "probe.json"), "--",
               "--dump-wram-high", str(out / "ymir-wram.bin"),
               "--video-observations", str(out / "ymir-fields.csv"),
               "--video-crop", "0:76:320:148"]
    for i in range(1, schema.SCENE_COUNT + 1):
        command += ["--screenshot-when", f"{mark:#x}:{i}:{out}/ymir-scene-{i}.png"]
    elapsed = run(command, out / "ymir.log")
    report = schema.read_report((out / "ymir-wram.bin").read_bytes(), map_path)
    write_json(out / "ymir-metrics.json", report)
    schema.validate(report, require_stress=True)

    images = [visual.load_rgb(out / f"ymir-scene-{i}.png")
              for i in range(1, schema.SCENE_COUNT + 1)]
    box = (0, 76, 320, 224)
    crops = [visual.crop(rgb, w, h, box) for w, h, rgb in images]
    if len({crops[0][i:i+3] for i in range(0, len(crops[0]), 3)}) < 16:
        raise ValueError("Ymir world crop has too few colors")
    results = [visual.diff(crops[0], pixels, 320, 148)[0] for pixels in crops]
    log = (out / "ymir.log").read_text()
    marks = {int(scene): int(frame) for frame, scene in
             re.findall(r"frame (\d+) -> .*ymir-scene-(\d+)\.png", log)}
    with (out / "ymir-fields.csv").open(newline="") as stream:
        observations = {int(row["frame"]): int(row["changed_pixels"])
                        for row in csv.DictReader(stream)}
    # Aligned around the same pose, away from mode boundaries and HUD.
    cadence = {scene: sum(observations[i] > 0 for i in range(frame-40, frame+40))
               for scene, frame in marks.items()}
    write_json(out / "ymir-visual.json", {"crop": box, "diff": results,
                                        "changed_world_fields_out_of_80": cadence})
    if len(cadence) != schema.SCENE_COUNT or not all(cadence.values()):
        raise ValueError("Missing Ymir world cadence observations")
    if any(result["different_pixels"] for result in results):
        raise ValueError("Same-pose Ymir world captures differ")

    source = probe_dir / "_deps/ymir-src"
    version = re.search(r"project\(Ymir VERSION ([^)]*)\)",
                        (source / "CMakeLists.txt").read_text())
    return {"version": version[1], "revision": capture(["git", "-C", source, "rev-parse", "HEAD"]),
            "probe_sha256": sha(probe_dir / "probe"), "host_seconds": elapsed, "fields": fields}


def legacy(out, bios, probe):
    dump = out / "legacy-wram.bin"
    elapsed = run(["./harness/run-harness.sh", "scu_dsp_demo", "--bios", str(bios),
                   "--build-dir", str(probe), "--frames", "40",
                   "--out", str(out / "legacy-probe.json"), "--",
                   "--dump-wram-high", str(dump),
                   "--screenshot", f"39:{out}/legacy-screen.png"], out / "legacy-ymir.log")
    env = dict(os.environ, LIBSATURN_DSP_DUMP=str(dump),
               LIBSATURN_DSP_MAP=str(out / "scu_dsp_demo.map"))
    run([sys.executable, "-m", "unittest", "-v", "harness.tests.test_scu_dsp"],
        out / "legacy-tests.log", env=env)
    return elapsed


def mednafen(out, bios, seconds):
    elf = out / f"{EXAMPLE}.elf"
    map_path = out / f"{EXAMPLE}.map"
    executable = shutil.which(os.environ.get("MEDNAFEN", "mednafen"))
    if not executable:
        raise RuntimeError("Mednafen is required")
    started = time.monotonic()
    with tempfile.TemporaryDirectory(prefix="mednafen-", dir=out) as tmp:
        profile = Path(tmp)
        movie = profile / "capture.mov"
        env = dict(os.environ, MEDNAFEN=executable, LIBSATURN_MEDNAFEN_HOME=str(profile))
        command = ["./run-example.sh", EXAMPLE, "--bios", str(bios), "--",
                   "-video.driver", "softfb", "-sound", "0",
                   "-ss.region_default", "na", "-ss.bios_na_eu", str(bios),
                   "-autosave", "1", "-qtrecord", str(movie)]
        print(f"{' '.join(command)} -> mednafen.log ({seconds:g} seconds)", flush=True)
        with (out / "mednafen.log").open("w") as log:
            process = subprocess.Popen(command, cwd=ROOT, env=env, stdout=log,
                                       stderr=subprocess.STDOUT)
            try:
                process.wait(timeout=seconds)
            except subprocess.TimeoutExpired:
                process.send_signal(signal.SIGINT)
                try:
                    process.wait(timeout=15)
                finally:
                    if process.poll() is None:
                        process.kill()
                        process.wait()
        if process.returncode:
            raise RuntimeError(f"Mednafen exited {process.returncode}")
        states = list((profile / "mcs").glob("*.mca"))
        if len(states) != 1:
            raise ValueError(f"Expected one native CD autosave, got {len(states)}")
        order, ram = normalize(work_ram(states[0]), *elf_text(elf))
        at = find_symbol(map_path, schema.SYMBOL)
        (out / "mednafen-telemetry.bin").write_bytes(ram[at:at+schema.SIZE])
        report = schema.read_report(ram, map_path)
        write_json(out / "mednafen-metrics.json", report)
        run(["ffmpeg", "-v", "error", "-sseof", "-2", "-i", str(movie), "-frames:v", "1",
             "-vf", "scale=640:448", "-threads", "1", str(out / "mednafen-screen.png")],
            out / "mednafen-screenshot.log", timeout=30)
        schema.validate(report, require_stress=True)
    log = (out / "mednafen.log").read_text()
    version = re.search(r"(?m)^Starting Mednafen (.*)$", log)
    return {"version": version[1], "binary_sha256": sha(Path(executable)), "wram_order": order,
            "host_seconds": round(time.monotonic()-started, 3), "boot": "native CD"}


def qualify(args):
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    archive = out.with_suffix(".zip")
    if archive.exists():
        raise FileExistsError(archive)
    os.environ.setdefault("MAKEFLAGS", f"-j{min(8, os.cpu_count() or 2)}")
    if "LIBSATURN_TOOLCHAIN_ROOT" not in os.environ and not shutil.which("sh2eb-elf-gcc"):
        os.environ["LIBSATURN_TOOLCHAIN_ROOT"] = str(Path.home() / "dev/saturn-tools")
    toolchain = os.environ.get("LIBSATURN_TOOLCHAIN_ROOT")
    if toolchain:
        os.environ["PATH"] = str(Path(toolchain) / "bin") + os.pathsep + os.environ["PATH"]
    probe = Path(os.environ.get("LIBSATURN_YMIR_BUILD_DIR",
                                ROOT / "harness/build/ymir-baseline")).resolve()
    bios = args.bios.resolve()
    meta = {"platform": capture(["uname", "-sm"]), "result": "FAIL", "stages": {},
            "build": {"configuration": "optimized Makefile example",
                      "benchmark_c": "-O3", "shared_sdk": "Makefile defaults",
                      "ip_profile": "current", "ram_cart": "none in Ymir"},
            "scope": "Emulator timing and guest correctness. Physical Saturn, SAROO and CD-streamed audio unqualified."}

    def step(name, operation):
        try:
            meta["stages"][name] = {"result": "PASS", "detail": operation()}
        except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as exc:
            meta["stages"][name] = {"result": "FAIL", "error": str(exc)}
            print(f"{name} failed: {exc}", file=sys.stderr)

    try:
        # Fail closed on pull/build failures. Collect independent test reports
        # even if one emulator fails qualification.
        meta["pull_seconds"] = run(["git", "pull", "--ff-only"], out / "git-pull.log")
        meta["git_sha"] = capture(["git", "rev-parse", "HEAD"])
        meta["git_status_before_build"] = capture(["git", "status", "--short"])
        meta["compiler"] = capture(["sh2eb-elf-gcc", "--version"])
        meta["bios_sha256"] = sha(bios)
        visual.require_ffmpeg()
        for example in ("scu_dsp_demo", EXAMPLE):
            run(["./build-example.sh", example], out / f"{example}-build.log")
            meta.setdefault("binaries", {})[example] = {}
            for suffix in ("elf", "app.bin", "bin", "cue", "map"):
                path = BUILD / f"{example}.{suffix}"
                shutil.copy2(path, out)
                meta["binaries"][example][suffix] = sha(path)
            (out / f"{example}-size.txt").write_text(
                capture(["sh2eb-elf-size", "-A", BUILD / f"{example}.elf"]) + "\n")
        step("host_tests", lambda: run(["gmake", "test"], out / "host-tests.log"))
        step("legacy_dsp", lambda: legacy(out, bios, probe))
        step("ymir", lambda: ymir(out, bios, probe, args.fields))
        step("mednafen", lambda: mednafen(out, bios, args.seconds))
        if all(stage["result"] == "PASS" for stage in meta["stages"].values()):
            meta["result"] = "PASS"
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as exc:
        meta["error"] = str(exc)
        print(exc, file=sys.stderr)
    finally:
        write_json(out / "qualification.json", meta)
        with zipfile.ZipFile(archive, "x", zipfile.ZIP_DEFLATED) as bundle:
            for path in sorted(out.iterdir()):
                if path.is_file():
                    bundle.write(path, path.name)
        print(f"{meta['result']}: {archive}", flush=True)
    return meta["result"] == "PASS"


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    default = "heterogeneous-qualification-" + datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
    parser.add_argument("--out", type=Path, default=BUILD / default)
    parser.add_argument("--bios", type=Path, default=Path(os.environ.get(
        "LIBSATURN_BIOS", ROOT / "bios/firmware/mpr-17933.bin")))
    parser.add_argument("--fields", type=int, default=1100, help="Ymir field budget")
    parser.add_argument("--seconds", type=float, default=42, help="Mednafen host-time budget")
    arguments = parser.parse_args()
    if not (1 <= arguments.fields <= 10000 and 0 < arguments.seconds <= 600):
        parser.error("--fields must be 1–10000 and --seconds must be in (0, 600]")
    if sys.platform != "darwin":
        parser.error("This qualification runner targets macOS")
    sys.exit(0 if qualify(arguments) else 1)
