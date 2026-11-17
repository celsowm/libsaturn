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
from harness.scripts.telemetry import find_symbol

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


def read_state(path, report_offset):
    data = path.read_bytes()
    if data.startswith(bytes.fromhex("1f8b")):
        data = gzip.decompress(data)
    info = {"file": path.name, "decoded_bytes": len(data),
            "state_header": data[:8].hex(), "wram_fields": [], "magic_hits": {}}
    found = {}
    # Native Mednafen named state variables encode a one-byte name length,
    # name, little-endian uint32 byte length, and raw contents. Saturn
    # WRAM-H is 1 MiB of uint16. Read the actual linked report address.
    name = b"WorkRAMH"
    start = 0
    while True:
        pos = data.find(name, start)
        if pos < 0:
            break
        start = pos + 1
        end = pos + len(name)
        if pos == 0 or data[pos-1] != len(name) or end+4 > len(data):
            continue
        length = struct.unpack_from("<I", data, end)[0]
        base = end+4
        if length != 1048576 or base+length > len(data):
            continue
        chunk = data[base+report_offset:base+report_offset+SIZE]
        if len(chunk) != SIZE:
            continue
        field = {"offset": base, "report_offset": report_offset, "headers": []}
        info["wram_fields"].append(field)
        for order in ORDERS:
            raw = bytes(chunk[i+j] for i in range(0, SIZE, 4) for j in order)
            words = struct.unpack(">176I", raw)
            field["headers"].append({"order": list(order), "header": list(words[:12])})
            if words[:2] == (0x534B5631, 1):
                found[raw] = {"offset": base+report_offset, "byte_order": list(order),
                              "method": "linked WRAM-H", "state": path.name}
    for order in ORDERS:
        marker = bytes(MAGIC[i] for i in order)
        info["magic_hits"][str(order)] = data.count(marker)
    # Fallback for emulator forks with different field names.
    for raw, source in candidates(data):
        found.setdefault(raw, dict(source, state=path.name, method="signature scan"))
    return found, info


def run():
    out = ROOT / "harness/build/sh2_kernel_verify_mednafen"
    out.mkdir(parents=True, exist_ok=True)
    archive = out.parent / "sh2_kernel_verify-mednafen-return.zip"
    archive.unlink(missing_ok=True)
    for name in ("analysis.json", "metadata.json", "run.log", "state-inspection.json"):
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
        report_offset = find_symbol(ROOT / "build/sh2_kernel_verify.map", "_g_sh2_kernel_verify")
        metadata["report_wram_offset"] = report_offset
        if not 0 <= report_offset <= 1048576-SIZE:
            raise ValueError("Report outside WRAM-H")
        tmp = tempfile.mkdtemp(prefix="mednafen-state-", dir=out)
        if True:  # retain capture locally, even on extraction failure
            # Fresh state directory: never load an earlier guest snapshot.
            cmd = ["sh", "run-example.sh", "sh2_kernel_verify", "--",
                   "-sound", "0", "-video.driver", "softfb",
                   "-autosave", "1", "-filesys.path_state", tmp,
                   "-filesys.fname_state", "sh2-verify.%X", "-filesys.state_comp_level", "-1"]
            print("Wait for SH2 KERNEL DIFFERENTIAL / ERRORS. Press F5 (fn+F5 on Mac) to save, then Esc.",
                  flush=True)
            with (out / "run.log").open("w") as log:
                subprocess.run(cmd, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
            found = {}
            inspections = []
            for state in Path(tmp).rglob("*"):
                if not state.is_file():
                    continue
                try:
                    reports, info = read_state(state, report_offset)
                    found.update(reports)
                    inspections.append(info)
                except (OSError, ValueError, EOFError) as exc:
                    inspections.append({"file": state.name, "error": str(exc)})
            (out / "state-inspection.json").write_text(json.dumps(inspections, indent=2)+"\n")
            metadata["state_directory"] = tmp
            metadata["state_files"] = len(inspections)
            completed = {raw: source for raw, source in found.items()
                         if struct.unpack_from(">I", raw, 8)[0] == 1}
            if completed:
                found = completed
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
            for name in ("analysis.json", "metadata.json", "run.log", "state-inspection.json"):
                path = out / name
                if path.is_file():
                    z.write(path, name)
        print(f"Return: {archive}", flush=True)


if __name__ == "__main__":
    run()
