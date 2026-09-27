#!/usr/bin/env python3
"""Encode a 2048-byte/sector ISO9660 image as raw CD-ROM Mode 1 sectors."""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path


ISO_SECTOR_BYTES = 2048
RAW_SECTOR_BYTES = 2352
USER_DATA_OFFSET = 16
USER_DATA_END = USER_DATA_OFFSET + ISO_SECTOR_BYTES
EDC_OFFSET = USER_DATA_END
ECC_OFFSET = 2076
SYNC = b"\x00" + (b"\xff" * 10) + b"\x00"


def _build_edc_lut() -> tuple[int, ...]:
    values: list[int] = []
    for value in range(256):
        edc = value
        for _ in range(8):
            edc = (edc >> 1) ^ (0xD8018001 if edc & 1 else 0)
        values.append(edc)
    return tuple(values)


def _build_ecc_luts() -> tuple[tuple[int, ...], tuple[int, ...]]:
    forward = [0] * 256
    backward = [0] * 256
    for index in range(256):
        value = (index << 1) ^ (0x11D if index & 0x80 else 0)
        forward[index] = value
        backward[index ^ value] = index
    return tuple(forward), tuple(backward)


EDC_LUT = _build_edc_lut()
ECC_FORWARD, ECC_BACKWARD = _build_ecc_luts()


def _bcd(value: int) -> int:
    if not 0 <= value <= 99:
        raise ValueError(f"BCD value outside CD range: {value}")
    return ((value // 10) << 4) | (value % 10)


def lba_to_msf_bcd(lba: int) -> bytes:
    """Return the on-disc MSF address for an LBA, including the 150-frame lead-in."""
    if lba < 0:
        raise ValueError("LBA must be non-negative")
    frame = lba + 150
    minute, frame = divmod(frame, 60 * 75)
    second, frame = divmod(frame, 75)
    return bytes((_bcd(minute), _bcd(second), _bcd(frame)))


def edc(data: bytes | bytearray | memoryview) -> int:
    """Calculate the CD-ROM EDC checksum for *data*."""
    value = 0
    for byte in data:
        value = (value >> 8) ^ EDC_LUT[(value ^ byte) & 0xFF]
    return value


def _ecc_block(source: bytes | bytearray, major_count: int, minor_count: int,
               major_mult: int, minor_inc: int) -> bytes:
    size = major_count * minor_count
    if len(source) < size:
        raise ValueError(f"ECC source has {len(source)} bytes, expected at least {size}")

    result = bytearray(major_count * 2)
    for major in range(major_count):
        index = ((major >> 1) * major_mult) + (major & 1)
        ecc_a = 0
        ecc_b = 0
        for _ in range(minor_count):
            value = source[index]
            index += minor_inc
            if index >= size:
                index -= size
            ecc_a ^= value
            ecc_b ^= value
            ecc_a = ECC_FORWARD[ecc_a]
        ecc_a = ECC_BACKWARD[ECC_FORWARD[ecc_a] ^ ecc_b]
        result[major] = ecc_a
        result[major + major_count] = ecc_a ^ ecc_b
    return bytes(result)


def encode_mode1_sector(lba: int, user_data: bytes) -> bytes:
    """Create one complete MODE1/2352 sector with valid EDC and ECC parity."""
    if len(user_data) != ISO_SECTOR_BYTES:
        raise ValueError(
            f"Mode 1 user data must be {ISO_SECTOR_BYTES} bytes, got {len(user_data)}"
        )

    sector = bytearray(RAW_SECTOR_BYTES)
    sector[:12] = SYNC
    sector[12:15] = lba_to_msf_bcd(lba)
    sector[15] = 1
    sector[USER_DATA_OFFSET:USER_DATA_END] = user_data
    struct.pack_into("<I", sector, EDC_OFFSET, edc(sector[:EDC_OFFSET]))
    # Bytes 2068..2075 are reserved zeroes in a Mode 1 sector.
    # Q parity includes the P parity bytes that have just been written.
    sector[ECC_OFFSET:ECC_OFFSET + 172] = _ecc_block(sector[12:], 86, 24, 2, 86)
    sector[ECC_OFFSET + 172:] = _ecc_block(sector[12:], 52, 43, 86, 88)
    return bytes(sector)


def validate_mode1_sector(lba: int, sector: bytes) -> bytes:
    """Validate a Mode 1 raw sector and return its 2048-byte user-data region."""
    if len(sector) != RAW_SECTOR_BYTES:
        raise ValueError(f"raw sector has {len(sector)} bytes, expected {RAW_SECTOR_BYTES}")
    if sector[:12] != SYNC:
        raise ValueError(f"LBA {lba}: invalid sync pattern")
    if sector[12:15] != lba_to_msf_bcd(lba):
        raise ValueError(f"LBA {lba}: invalid MSF address")
    if sector[15] != 1:
        raise ValueError(f"LBA {lba}: expected Mode 1, got {sector[15]}")
    if sector[2068:ECC_OFFSET] != b"\x00" * 8:
        raise ValueError(f"LBA {lba}: Mode 1 reserved bytes are not zero")
    stored_edc = struct.unpack_from("<I", sector, EDC_OFFSET)[0]
    if stored_edc != edc(sector[:EDC_OFFSET]):
        raise ValueError(f"LBA {lba}: EDC mismatch")
    ecc_source = sector[12:]
    expected_p = _ecc_block(ecc_source, 86, 24, 2, 86)
    expected_q = _ecc_block(ecc_source, 52, 43, 86, 88)
    if sector[ECC_OFFSET:ECC_OFFSET + 172] != expected_p:
        raise ValueError(f"LBA {lba}: ECC P mismatch")
    if sector[ECC_OFFSET + 172:] != expected_q:
        raise ValueError(f"LBA {lba}: ECC Q mismatch")
    return sector[USER_DATA_OFFSET:USER_DATA_END]


def convert_iso_to_raw(iso_path: Path, output_path: Path) -> int:
    """Stream-convert an ISO file and return its number of data sectors."""
    if iso_path.resolve() == output_path.resolve():
        raise ValueError("input ISO and output BIN must be different files")
    size = iso_path.stat().st_size
    if size % ISO_SECTOR_BYTES:
        raise ValueError(
            f"ISO size {size} is not aligned to {ISO_SECTOR_BYTES}-byte sectors"
        )

    output_path.parent.mkdir(parents=True, exist_ok=True)
    temp_path = output_path.with_name(output_path.name + ".tmp")
    try:
        with iso_path.open("rb") as source, temp_path.open("wb") as output:
            for lba in range(size // ISO_SECTOR_BYTES):
                user_data = source.read(ISO_SECTOR_BYTES)
                if len(user_data) != ISO_SECTOR_BYTES:
                    raise ValueError(f"unexpected EOF in ISO at LBA {lba}")
                output.write(encode_mode1_sector(lba, user_data))
        temp_path.replace(output_path)
    except BaseException:
        temp_path.unlink(missing_ok=True)
        raise
    return size // ISO_SECTOR_BYTES


def main() -> None:
    parser = argparse.ArgumentParser(description="Convert ISO9660 data to MODE1/2352 raw BIN")
    parser.add_argument("--iso", required=True, type=Path, help="2048-byte/sector ISO input")
    parser.add_argument("--output", required=True, type=Path, help="raw MODE1/2352 BIN output")
    args = parser.parse_args()

    try:
        sectors = convert_iso_to_raw(args.iso, args.output)
    except (OSError, ValueError) as error:
        print(f"[iso-to-raw] ERROR: {error}", file=sys.stderr)
        raise SystemExit(1) from error
    print(f"[iso-to-raw] {args.iso} -> {args.output}: {sectors} MODE1/2352 sectors")


if __name__ == "__main__":
    main()
