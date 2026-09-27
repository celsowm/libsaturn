#!/usr/bin/env python3
"""Regression tests for the Saturn raw MODE1/2352 distribution image."""

from __future__ import annotations

import hashlib
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from check_disc_image import validate_cue, validate_disc  # noqa: E402
from iso_to_raw import (  # noqa: E402
    ECC_OFFSET,
    EDC_OFFSET,
    RAW_SECTOR_BYTES,
    convert_iso_to_raw,
    encode_mode1_sector,
    validate_mode1_sector,
)


PAYLOAD = bytes(range(256)) * 8
# Golden MODE1/2352 sector for PAYLOAD at LBA 0, independently retained so an
# accidental change to the EDC/ECC tables or sector layout is visible in review.
GOLDEN_SHA256 = "f6a94f686e87d4dd2816703689017aa16078564724f033e97eb0faa23cb1adb3"
GOLDEN_EDC = bytes.fromhex("279739e6")


sector = encode_mode1_sector(0, PAYLOAD)
assert len(sector) == RAW_SECTOR_BYTES
assert sector[:16] == bytes.fromhex("00ffffffffffffffffffff0000020001")
assert sector[EDC_OFFSET:EDC_OFFSET + 4] == GOLDEN_EDC
assert hashlib.sha256(sector).hexdigest() == GOLDEN_SHA256
assert validate_mode1_sector(0, sector) == PAYLOAD

bad_sync = bytearray(sector)
bad_sync[1] = 0
try:
    validate_mode1_sector(0, bytes(bad_sync))
except ValueError as error:
    assert "sync" in str(error)
else:
    raise AssertionError("invalid sync was accepted")

bad_mode = bytearray(sector)
bad_mode[15] = 2
try:
    validate_mode1_sector(0, bytes(bad_mode))
except ValueError as error:
    assert "Mode 1" in str(error)
else:
    raise AssertionError("invalid sector mode was accepted")

try:
    encode_mode1_sector(0, b"short")
except ValueError:
    pass
else:
    raise AssertionError("short Mode 1 user data was accepted")

with tempfile.TemporaryDirectory() as temporary:
    directory = Path(temporary)
    iso = directory / "game.iso"
    raw = directory / "game.bin"
    cue = directory / "game.cue"
    second_payload = bytes(reversed(range(256))) * 8
    iso.write_bytes(PAYLOAD + second_payload)

    assert convert_iso_to_raw(iso, raw) == 2
    cue.write_text('FILE "game.bin" BINARY\n  TRACK 01 MODE1/2352\n    INDEX 01 00:00:00\n', encoding="ascii")
    validate_cue(cue, raw)
    assert validate_disc(iso, raw) == 2

    with raw.open("rb") as stream:
        stream.seek(RAW_SECTOR_BYTES)
        assert validate_mode1_sector(1, stream.read(RAW_SECTOR_BYTES)) == second_payload

    cue.write_text('FILE "game.iso" BINARY\n  TRACK 01 MODE1/2048\n    INDEX 01 00:00:00\n', encoding="ascii")
    try:
        validate_cue(cue, raw)
    except ValueError:
        pass
    else:
        raise AssertionError("MODE1/2048 CUE was accepted")

    damaged = bytearray(raw.read_bytes())
    damaged[ECC_OFFSET] ^= 0x80
    raw.write_bytes(damaged)
    try:
        validate_disc(iso, raw)
    except ValueError as error:
        assert "ECC" in str(error)
    else:
        raise AssertionError("damaged ECC was accepted")

    raw.write_bytes(raw.read_bytes()[:-1])
    try:
        validate_disc(iso, raw)
    except ValueError as error:
        assert "size" in str(error)
    else:
        raise AssertionError("truncated raw BIN was accepted")

    malformed_iso = directory / "malformed.iso"
    malformed_iso.write_bytes(b"x")
    try:
        convert_iso_to_raw(malformed_iso, directory / "malformed.bin")
    except ValueError as error:
        assert "aligned" in str(error)
    else:
        raise AssertionError("unaligned ISO was accepted")

print("disc image tooling: OK")
