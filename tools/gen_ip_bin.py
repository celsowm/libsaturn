#!/usr/bin/env python3
"""Generate a minimal 32KB IP.BIN boot block for Saturn ISO workflows.

Saturn System ID layout (first 0x100 bytes of sector 0):
  0x000-0x00F : Hardware ID       "SEGA SEGASATURN "
  0x010-0x01F : Maker ID          (16 bytes)
  0x020-0x029 : Product Number    (10 bytes)
  0x02A-0x02F : Version           (6 bytes)
  0x030-0x037 : Release Date      YYYYMMDD (8 bytes)
  0x038-0x03F : Device Info       (8 bytes)
  0x040-0x049 : Area Symbols      "JUE" etc. (10 bytes)
  0x04A-0x04F : (padding)         (6 bytes)
  0x050-0x05F : Peripherals       (16 bytes)
  0x060-0x0CF : Game Title        (112 bytes)
  0x0D0-0x0DF : (reserved)
  0x0E0-0x0E3 : IP Size           (4 bytes, binary BE)
  0x0E4-0x0E7 : (reserved)
  0x0E8-0x0EB : Master Stack      (4 bytes, binary BE)
  0x0EC-0x0EF : Slave Stack       (4 bytes, binary BE)
  0x0F0-0x0F3 : 1st Read Address  (4 bytes, binary BE)
  0x0F4-0x0F7 : 1st Read Size     (4 bytes, binary BE)
  0x0F8-0x0FF : (reserved)
  0x100-0x7FFF: Security/boot code area
"""

from __future__ import annotations

import argparse
import struct
from pathlib import Path

IP_SIZE = 0x8000
DEFAULT_ENTRY = 0x06004000


def write_field(blob: bytearray, offset: int, size: int, text: str) -> None:
    data = text.encode("ascii", errors="ignore")[:size]
    blob[offset : offset + size] = b" " * size
    blob[offset : offset + len(data)] = data


def build_ip_bin(maker: str, product: str, version: str, date_yyyymmdd: str, game_name: str, entry: int, stub_path: str | None = None) -> bytes:
    blob = bytearray(IP_SIZE)

    # System ID fields
    write_field(blob, 0x000, 16, "SEGA SEGASATURN ")
    write_field(blob, 0x010, 16, maker)
    write_field(blob, 0x020, 10, product)
    write_field(blob, 0x02A, 6, version)
    write_field(blob, 0x030, 8, date_yyyymmdd)
    write_field(blob, 0x038, 8, "CD-1/1  ")
    write_field(blob, 0x040, 10, "JTUE      ")
    write_field(blob, 0x050, 16, "J             R ")
    write_field(blob, 0x060, 112, game_name)

    # Binary control fields
    struct.pack_into(">I", blob, 0x0E0, IP_SIZE)
    struct.pack_into(">I", blob, 0x0E8, 0x060FFFFC)   # Master stack
    struct.pack_into(">I", blob, 0x0EC, 0x06001000)    # Slave stack
    struct.pack_into(">I", blob, 0x0F0, entry)         # 1st read address
    struct.pack_into(">I", blob, 0x0F4, 0x00000000)    # 1st read size (0 = auto)

    # Embed boot stub at offset 0x100
    if stub_path:
        stub_data = Path(stub_path).read_bytes()
        max_stub = IP_SIZE - 0x100
        if len(stub_data) > max_stub:
            stub_data = stub_data[:max_stub]
        blob[0x100 : 0x100 + len(stub_data)] = stub_data

    return bytes(blob)


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate minimal Saturn ip.bin")
    parser.add_argument("--output", required=True, help="Output path for ip.bin")
    parser.add_argument("--maker", default="LIBSATURN", help="Maker string")
    parser.add_argument("--product", default="T-00000G  ", help="Product code (10 chars)")
    parser.add_argument("--version", default="V1.000", help="Version field (6 chars)")
    parser.add_argument("--date", default="20260310", help="Release date YYYYMMDD")
    parser.add_argument("--name", default="LIBSATURN MVP", help="Game name (max 112 chars)")
    parser.add_argument("--entry", default=f"{DEFAULT_ENTRY:08X}", help="Entry point hex, ex: 06004000")
    parser.add_argument("--stub", default=None, help="Path to boot stub binary to embed at offset 0x100")
    args = parser.parse_args()

    entry = int(args.entry, 16)
    data = build_ip_bin(args.maker, args.product, args.version, args.date, args.name, entry, args.stub)

    out = Path(args.output)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(data)
    print(f"Wrote {len(data)} bytes to {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
