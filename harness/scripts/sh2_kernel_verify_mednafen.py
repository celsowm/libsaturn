#!/usr/bin/env python3
"""Unattended native CD verification using the Tekken 3 Saturn capture protocol.

Reference: celsowm/tekken3-saturn scripts/record_mednafen.py and
scripts/mednafen_state.py at f8cce4fc5c301ea4c17c9f22873d0821e16fcaed.
No keyboard, window focus, movie, or external input automation is needed.
"""
import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import shutil
import signal
import struct
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from harness.tests.test_sh2_kernel_verify import decode, validate
from harness.scripts.telemetry import find_symbol

SIZE = 704
HIGH = 0x06000000
RAM_SIZE = 0x100000


def sections(data):
    if data[:8] != b"MDFNSVST" or len(data) < 32:
        raise ValueError("Not a Mednafen save state")
    at = 32
    while at < len(data):
        if at+36 > len(data):
            raise ValueError("Truncated section header")
        name = data[at:at+32].split(b"\0")[0].decode(errors="replace")
        size = struct.unpack_from("<I", data, at+32)[0]
        end = at+36+size
        if end > len(data):
            raise ValueError("Truncated section: "+name)
        yield name, data[at+36:end]
        at = end


def entries(body):
    at = 0
    while at < len(body):
        n = body[at]
        end = at+1+n
        if end+4 > len(body):
            raise ValueError("Truncated state variable header")
        name = body[at+1:end].split(b"\0")[0].decode(errors="replace")
        size = struct.unpack_from("<I", body, end)[0]
        if end+4+size > len(body):
            raise ValueError("Truncated state variable: "+name)
        yield name, body[end+4:end+4+size]
        at = end+4+size


def swap16(raw):
    result = bytearray(raw)
    result[0::2], result[1::2] = raw[1::2], raw[0::2]
    return bytes(result)


def work_ram(state):
    data = state.read_bytes()
    if data[:2] == bytes.fromhex("1f8b"):
        data = gzip.decompress(data)
    for name, body in sections(data):
        if name == "MAIN":
            ram = dict(entries(body))
            high = ram["WorkRAMH"]
            if len(high) != RAM_SIZE:
                raise ValueError("Wrong WRAM-H size")
            return high
    raise ValueError("Missing MAIN section")


def elf_text(elf):
    output = subprocess.check_output(["sh2eb-elf-objdump", "-h", str(elf)], text=True)
    for line in output.splitlines():
        parts = line.split()
        if len(parts) >= 6 and parts[1] == ".text":
            size, address, offset = int(parts[2],16), int(parts[3],16), int(parts[5],16)
            with elf.open("rb") as f:
                f.seek(offset)
                expected = f.read(min(size,4096))
            if not expected:
                raise ValueError("Empty ELF text")
            return (address & 0x0fffffff)-HIGH, expected
    raise ValueError("No ELF .text section")


def normalize(high, text_offset, expected):
    if not 0 <= text_offset <= RAM_SIZE-len(expected):
        raise ValueError("ELF text outside WRAM-H")
    for order, ram in (("big", high), ("swapped16", swap16(high))):
        if ram[text_offset:text_offset+len(expected)] == expected:
            return order, ram
    raise ValueError("Save state does not contain the built ELF's .text")


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(args):
    example = args.example
    if example == "static_project_bench":
        from harness.tests import test_static_project_bench as schema
    elif example == "hierarchy_compose_bench":
        from harness.tests import test_hierarchy_compose_bench as schema
    elif example == "weighted_blend_bench":
        from harness.tests import test_weighted_blend_bench as schema
    elif example == "paint_distribution_bench":
        from harness.tests import test_paint_distribution_bench as schema
    elif example == "paint_order_bench":
        from harness.tests import test_paint_order_bench as schema
    elif example == "screen_clip_bench":
        from harness.tests import test_screen_clip_bench as schema
    else:
        from harness.tests import test_sh2_kernel_verify as schema
    size = getattr(schema, "SIZE", SIZE)
    out = ROOT / f"harness/build/{example}_mednafen"
    out.mkdir(parents=True, exist_ok=True)
    archive = out.parent / f"{example}-mednafen-return.zip"
    archive.unlink(missing_ok=True)
    names = ("analysis.json", "metadata.json", "run.log", "telemetry.bin")
    for name in names:
        (out/name).unlink(missing_ok=True)
    metadata = {"emulator": "Mednafen", "boot": "native CD",
                "hardware_status": "PENDING", "result": "FAIL",
                "seconds_requested": args.seconds}
    env = dict(os.environ)
    toolchain = Path(env.get("LIBSATURN_TOOLCHAIN_ROOT", str(Path.home()/"dev/saturn-tools")))
    env["LIBSATURN_TOOLCHAIN_ROOT"] = str(toolchain)
    env["PATH"] = str(toolchain/"bin")+os.pathsep+env.get("PATH","")
    os.environ["PATH"] = env["PATH"]  # objdump uses the same compiler tools
    try:
        metadata["checkout"] = subprocess.check_output(
            ["git","rev-parse","HEAD"], cwd=ROOT, text=True).strip()
        bios = args.bios or Path(env.get("LIBSATURN_BIOS",
                                       str(ROOT/"bios/firmware/mpr-17933.bin")))
        bios = bios.resolve()
        if bios.stat().st_size != 524288:
            raise ValueError("BIOS must be a caller-owned 512 KiB image")
        metadata["bios_sha256"] = sha(bios)
        with (out/"run.log").open("w") as log:
            subprocess.run(["sh","build-example.sh",example],
                           cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT, check=True)
            elf = ROOT/f"build/{example}.elf"
            metadata["elf_sha256"] = sha(elf)
            metadata["cue_sha256"] = sha(ROOT/f"build/{example}.cue")
            metadata["disc_sha256"] = sha(ROOT/f"build/{example}.bin")
            offset = find_symbol(ROOT/f"build/{example}.map", f"_g_{example}")
            if not 0 <= offset <= RAM_SIZE-size:
                raise ValueError("Telemetry outside WRAM-H")
            metadata["report_wram_offset"] = offset
            text_offset, expected = elf_text(elf)
            with tempfile.TemporaryDirectory(prefix="mednafen-profile-", dir=out) as tmp:
                profile = Path(tmp)
                env["LIBSATURN_MEDNAFEN_HOME"] = str(profile)
                env["MEDNAFEN_HOME"] = str(profile)
                exe = shutil.which(env.get("MEDNAFEN","mednafen"), path=env["PATH"])
                if not exe:
                    raise RuntimeError("Mednafen not on PATH; set MEDNAFEN")
                env["MEDNAFEN"] = exe
                metadata["mednafen_sha256"] = sha(Path(exe))
                # Use the existing LibSaturn launcher but give it a fresh
                # profile and explicit BIOS. mcs/*.mca is the default
                # autosave location; do not override filename/path settings.
                command = ["sh","run-example.sh",example,"--bios",str(bios),"--",
                           "-sound","0","-video.driver","softfb","-ss.region_default","na",
                           "-ss.bios_na_eu",str(bios),"-autosave","1","-ss.input.port2","none"]
                for button in ("up","down","left","right","a","b","c","x","y","z","ls","rs","start"):
                    command += ["-ss.input.port1.gamepad."+button,""]
                metadata["command"] = command
                print(f"Running Mednafen unattended for {args.seconds:g} seconds...", flush=True)
                process = subprocess.Popen(command, cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT)
                try:
                    try:
                        process.wait(timeout=args.seconds)
                    except subprocess.TimeoutExpired:
                        # Both shell launchers exec, so this is the emulator PID.
                        process.send_signal(signal.SIGINT)
                        process.wait(timeout=20)
                finally:
                    if process.poll() is None:
                        process.kill()
                        process.wait()
                metadata["returncode"] = process.returncode
                states = sorted((profile/"mcs").glob("*.mca"))
                metadata["state_files"] = [{"name":p.name,"size":p.stat().st_size} for p in states]
                if process.returncode:
                    raise RuntimeError(f"Mednafen exited {process.returncode}; inspect run.log")
                if len(states) != 1:
                    metadata["profile_files"] = [str(p.relative_to(profile)) for p in profile.rglob("*") if p.is_file()]
                    raise RuntimeError(f"Expected one autosave in mcs; found {len(states)}")
                metadata["state_sha256"] = sha(states[0])
                order, high = normalize(work_ram(states[0]), text_offset, expected)
                metadata["wram_order"] = order
                raw = high[offset:offset+size]
                (out/"telemetry.bin").write_bytes(raw)
                result = schema.decode(struct.unpack(f">{size//4}I",raw))
                result["scope"] = "Mednafen native CD boot; inclusive diagnostic FRT sums, not hardware timing"
                (out/"analysis.json").write_text(json.dumps(result,indent=2)+"\n")
                print(json.dumps(result,indent=2))
                schema.validate(result)
                metadata["result"] = "PASS"
    except BaseException as exc:
        metadata["error"] = str(exc)
        raise
    finally:
        (out/"metadata.json").write_text(json.dumps(metadata,indent=2)+"\n")
        with zipfile.ZipFile(archive,"w",zipfile.ZIP_DEFLATED) as z:
            for name in names:
                if (out/name).is_file():
                    z.write(out/name,name)
        print(f"Return: {archive}",flush=True)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--seconds", type=float, default=30)
    p.add_argument("--bios", type=Path)
    p.add_argument("--example", choices=("sh2_kernel_verify","static_project_bench","hierarchy_compose_bench","weighted_blend_bench","paint_distribution_bench","paint_order_bench","screen_clip_bench"), default="sh2_kernel_verify")
    args = p.parse_args()
    if not 0 < args.seconds <= 600:
        p.error("--seconds must be in (0, 600]")
    run(args)
