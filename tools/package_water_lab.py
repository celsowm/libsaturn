#!/usr/bin/env python3
"""Package Water Lab apps or a validated pair of native disc images."""

import argparse
import json
from pathlib import Path
from tempfile import TemporaryDirectory
from zipfile import ZIP_DEFLATED, ZipFile

from boot_image import SYSTEM_AREA, digest, inspect
from check_disc_image import validate_cue, validate_disc
from check_native_ip import validate as validate_ip


ROOT = Path(__file__).resolve().parent.parent
EXAMPLES = ("water_lab", "water_lab_probe")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, default=ROOT / "build" / "examples")
    parser.add_argument("--out", type=Path, default=ROOT / "build" / "water_lab_evidence")
    parser.add_argument("--discs", choices=("local", "public"))
    parser.add_argument("--payloads", type=Path, default=ROOT / "build/native-boot/payloads.json")
    args = parser.parse_args()
    files = {}
    try:
        if args.discs:
            payloads = json.loads(args.payloads.read_text(encoding="utf-8"))
            inputs = [payloads["security"], *payloads["regions"]]
            if (payloads["schema_version"] != 1 or
                    not isinstance(payloads["redistributable"], bool) or
                    any(not item.get("origin") or not item.get("license") for item in inputs)):
                raise ValueError("boot payload manifest requires provenance and license information")
            if args.discs == "public" and not payloads["redistributable"]:
                raise ValueError("boot payload manifest has redistributable=false")

        for example in EXAMPLES:
            app = (args.build / f"{example}.app.bin").read_bytes()
            if not app:
                raise ValueError(f"{example} application is empty")
            files[f"{example}.app.bin"] = app
            if args.discs:
                iso, raw, cue = (args.build / f"{example}.{ext}" for ext in ("iso", "bin", "cue"))
                validate_cue(cue, raw)
                validate_disc(iso, raw)
                image = iso.read_bytes()
                inspect(image, app, True)
                validate_ip(image[:SYSTEM_AREA], app, args.payloads)
                files[raw.name] = raw.read_bytes()
                files[cue.name] = cue.read_bytes()

        files["LICENSE"] = (ROOT / "LICENSE").read_bytes()
        files["README.md"] = (ROOT / "examples/water_lab/README.md").read_bytes()
        if args.discs:
            files["FX_COMPOSITOR.md"] = (ROOT / "docs/FX_COMPOSITOR.md").read_bytes()
            files["BOOT_PAYLOADS.json"] = args.payloads.read_bytes()
            verifier = ROOT / "harness/scripts/verify_water_lab_capture.py"
            files[verifier.name] = verifier.read_bytes()
        files["SHA256SUMS"] = "".join(
            f"{digest(data)}  {name}\n" for name, data in sorted(files.items()) if name.endswith(".bin")
        ).encode("ascii")
        name = {
            None: "water_lab_app.zip",
            "local": "water_lab_hardware_probe.zip",
            "public": "water_lab_public_discs.zip",
        }[args.discs]
        args.out.mkdir(parents=True, exist_ok=True)
        archive_path = args.out / name
        with TemporaryDirectory(dir=args.out) as work:
            staged = Path(work) / name
            with ZipFile(staged, "w", compression=ZIP_DEFLATED) as archive:
                for filename, data in files.items():
                    archive.writestr(filename, data)
            staged.replace(archive_path)
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.exit(1, f"Water Lab package: {error}\n")
    print(archive_path)


if __name__ == "__main__":
    main()
