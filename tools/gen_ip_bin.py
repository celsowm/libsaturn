#!/usr/bin/env python3
"""Generate a minimal 32KB IP.BIN-like boot block for Saturn ISO workflows."""

from __future__ import annotations

import argparse
from pathlib import Path

IP_SIZE = 0x8000
DEFAULT_ENTRY = 0x06004000


def write_field(blob: bytearray, offset: int, size: int, text: str) -> None:
    data = text.encode("ascii", errors="ignore")[:size]
    blob[offset : offset + size] = b" " * size
    blob[offset : offset + len(data)] = data


def build_ip_bin(maker: str, product: str, version: str, date_yyyymmdd: str, game_name: str, entry: int) -> bytes:
    blob = bytearray(IP_SIZE)

    write_field(blob, 0x000, 16, "SEGA SEGASATURN ")
    write_field(blob, 0x060, 96, "(C)2026 LIBSATURN")
    write_field(blob, 0x100, 16, maker)
    write_field(blob, 0x110, 8, date_yyyymmdd)
    write_field(blob, 0x120, 16, "JUE             ")
    write_field(blob, 0x130, 16, game_name)
    write_field(blob, 0x140, 16, version)
    write_field(blob, 0x150, 8, f"{entry:08X}")
    write_field(blob, 0x158, 8, product)

    return bytes(blob)


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate minimal Saturn ip.bin")
    parser.add_argument("--output", required=True, help="Output path for ip.bin")
    parser.add_argument("--maker", default="LIBSATURN", help="Maker string")
    parser.add_argument("--product", default="LIBSATURN01", help="Product code")
    parser.add_argument("--version", default="V1.000", help="Version field")
    parser.add_argument("--date", default="20260310", help="Release date YYYYMMDD")
    parser.add_argument("--name", default="LIBSATURN MVP", help="Game name (max 16 chars)")
    parser.add_argument("--entry", default=f"{DEFAULT_ENTRY:08X}", help="Entry point hex, ex: 06004000")
    args = parser.parse_args()

    entry = int(args.entry, 16)
    data = build_ip_bin(args.maker, args.product, args.version, args.date, args.name, entry)

    out = Path(args.output)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(data)
    print(f"Wrote {len(data)} bytes to {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

