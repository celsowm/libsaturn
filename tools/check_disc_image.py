#!/usr/bin/env python3
"""Validate a single-track Saturn MODE1/2352 CUE/BIN image against its ISO."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from iso_to_raw import ISO_SECTOR_BYTES, RAW_SECTOR_BYTES, validate_mode1_sector


def validate_cue(cue_path: Path, bin_path: Path) -> None:
    lines = [line.strip() for line in cue_path.read_text(encoding="ascii").splitlines() if line.strip()]
    expected = [
        f'FILE "{bin_path.name}" BINARY',
        "TRACK 01 MODE1/2352",
        "INDEX 01 00:00:00",
    ]
    if lines != expected:
        raise ValueError(
            f"CUE must describe exactly one MODE1/2352 track for {bin_path.name}: {lines!r}"
        )


def validate_disc(iso_path: Path, bin_path: Path) -> int:
    iso_size = iso_path.stat().st_size
    bin_size = bin_path.stat().st_size
    if iso_size % ISO_SECTOR_BYTES:
        raise ValueError(f"ISO size {iso_size} is not sector aligned")
    sectors = iso_size // ISO_SECTOR_BYTES
    expected_bin_size = sectors * RAW_SECTOR_BYTES
    if bin_size != expected_bin_size:
        raise ValueError(
            f"BIN size {bin_size} does not match {sectors} raw sectors ({expected_bin_size})"
        )

    with iso_path.open("rb") as iso, bin_path.open("rb") as raw:
        for lba in range(sectors):
            expected_user_data = iso.read(ISO_SECTOR_BYTES)
            actual_user_data = validate_mode1_sector(lba, raw.read(RAW_SECTOR_BYTES))
            if actual_user_data != expected_user_data:
                raise ValueError(f"LBA {lba}: raw user data differs from ISO input")
    return sectors


def main() -> None:
    parser = argparse.ArgumentParser(description="Validate a Saturn MODE1/2352 CUE/BIN image")
    parser.add_argument("--iso", required=True, type=Path)
    parser.add_argument("--bin", required=True, type=Path)
    parser.add_argument("--cue", required=True, type=Path)
    args = parser.parse_args()
    try:
        validate_cue(args.cue, args.bin)
        sectors = validate_disc(args.iso, args.bin)
    except (OSError, UnicodeError, ValueError) as error:
        print(f"[check] disc image FAIL: {error}", file=sys.stderr)
        raise SystemExit(1) from error
    print(f"[check] disc image OK: {sectors} MODE1/2352 sectors")


if __name__ == "__main__":
    main()
