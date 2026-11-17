#!/usr/bin/env python3
"""Boot the native diagnostic in Mednafen and extract its saved guest report."""
import gzip
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from harness.tests.test_sh2_kernel_verify import decode, validate

SIZE = 704
MAGIC = bytes.fromhex("534b5631")
# Mednafen serializes memory differently across versions/host byte orders.
ORDERS = ((0, 1, 2, 3), (1, 0, 3, 2), (3, 2, 1, 0), (2, 3, 0, 1))


def candidates(data):
    for order in ORDERS:
        marker = bytes(MAGIC[i] for i in order)
        start = 0
        while True:
            pos = data.find(marker, start)
            if pos < 0:
                break
            start = pos + 1
            chunk = data[pos:pos + SIZE]
            if len(chunk) != SIZE:
                continue
            raw = bytes(chunk[i + j] for i in range(0, SIZE, 4) for j in order)
            words = struct.unpack(">176I", raw)
            # Distinguish the report from a literal pool or unrelated RAM.
            # Still accept failed/incomplete Slave lanes to diagnose them.
            if (words[0:3] != (0x534B5631, 1, 1)
                    or not 10000 < words[6] < 2000000
                    or words[8] != 0 or words[11] != 1
                    or words[12] != 128):
                continue
            yield raw, {"offset": pos, "byte_order": list(order)}


def read_state(path):
    data = path.read_bytes()
    if data.startswith(b'\x1f\x8b'):
        data = gzip.decompress(data)
    return list(candidates(data))


def run():
    out = ROOT / "harness/build/sh2_kernel_verify_mednafen"
    out.mkdir(parents=True, exist_ok=True)
    archive = out.parent / "sh2_kernel_verify-mednafen-return.zip"
    archive.unlink(missing_ok=True)
    for name in ("analysis.json", "metadata.json", "run.log"):
        (out / name).unlink(missing_ok=True)
    metadata = {"emulator": "Mednafen", "boot": "native CD",
                "hardware_status": "PENDING", "result": "FAIL"}
    try:
        metadata["checkout"] = subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
        subprocess.run(["sh", "build-example.sh", "sh2_kernel_verify"],
                       cwd=ROOT, check=True)
        elf = ROOT / "build/sh2_kernel_verify.elf"
        metadata["elf_sha256"] = hashlib.sha256(elf.read_bytes()).hexdigest()
        with tempfile.TemporaryDirectory(prefix="mednafen-state-", dir=out) as tmp:
            # Fresh state directory: never load an earlier guest snapshot.
            cmd = ["sh", "run-example.sh", "sh2_kernel_verify", "--",
                   "-sound", "0", "-video.driver", "softfb",
                   "-autosave", "1", "-filesys.path_state", tmp]
            print("Wait for SH2 KERNEL DIFFERENTIAL / ERRORS, then close Mednafen normally (Esc).",
                  flush=True)
            with (out / "run.log").open("w") as log:
                subprocess.run(cmd, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
            found = {}
            for state in Path(tmp).rglob("*"):
                if not state.is_file():
                    continue
                for raw, source in read_state(state):
                    found[raw] = dict(source, state=state.name)
            if len(found) != 1:
                raise RuntimeError(f"Expected one distinct completed report; found {len(found)}. "
                                   "Check native boot, autosave, and wait for the result screen.")
            raw, source = next(iter(found.items()))
            result = decode(struct.unpack(">176I", raw))
            result["scope"] = ("Mednafen native CD boot, FRT sums from diagnostic, "
                               "not physical-hardware timings")
            metadata["state_report"] = source
            (out / "analysis.json").write_text(json.dumps(result, indent=2)+"\n")
            print(json.dumps(result, indent=2))
            validate(result)
            metadata["result"] = "PASS"
    except BaseException as exc:
        metadata["error"] = str(exc)
        raise
    finally:
        (out / "metadata.json").write_text(json.dumps(metadata, indent=2)+"\n")
        with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as z:
            for name in ("analysis.json", "metadata.json", "run.log"):
                path = out / name
                if path.is_file():
                    z.write(path, name)
        print(f"Return: {archive}", flush=True)


if __name__ == "__main__":
    run()
