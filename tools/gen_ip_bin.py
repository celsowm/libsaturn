#!/usr/bin/env python3
"""Generate a Saturn-compatible 32KB IP.BIN for emulator workflows."""

from __future__ import annotations

import argparse
import struct
from pathlib import Path

IP_TOTAL_SIZE = 0x8000
IP_HEADER_SIZE = 0x100
SECURITY_BLOCK_SIZE = 0x500
IP_CODE_OFFSET = IP_HEADER_SIZE + SECURITY_BLOCK_SIZE
IP_CODE_LIMIT = IP_TOTAL_SIZE - IP_CODE_OFFSET

DEFAULT_IP_LOAD_ADDRESS = 0x06004000
DEFAULT_FIRST_READ_ADDRESS = 0x06010000
DEFAULT_MASTER_STACK = 0x060FFFFC
DEFAULT_SLAVE_STACK = 0x06001000
DEFAULT_AREA_SYMBOLS = "JT  UB E  "


def write_field(blob: bytearray, offset: int, size: int, text: str) -> None:
    data = text.encode("ascii", errors="ignore")[:size]
    blob[offset : offset + size] = b" " * size
    blob[offset : offset + len(data)] = data


def parse_hex_u32(raw: str) -> int:
    value = raw.strip().lower()
    if value.startswith("0x"):
        value = value[2:]
    parsed = int(value, 16)
    if parsed < 0 or parsed > 0xFFFFFFFF:
        raise ValueError(f"valor fora de uint32: {raw}")
    return parsed


def parse_u32(raw: str) -> int:
    parsed = int(raw, 0)
    if parsed < 0 or parsed > 0xFFFFFFFF:
        raise ValueError(f"valor fora de uint32: {raw}")
    return parsed


def align_2048(value: int) -> int:
    return (value + 0x7FF) & ~0x7FF


def build_ip_bin(
    maker: str,
    product: str,
    version: str,
    date_yyyymmdd: str,
    game_name: str,
    ip_load_address: int,
    first_read_address: int,
    first_read_size: int,
    master_stack: int,
    slave_stack: int,
    stub_path: str | None = None,
) -> bytes:
    blob = bytearray(IP_TOTAL_SIZE)

    # System ID header (0x000 - 0x0FF)
    write_field(blob, 0x000, 16, "SEGA SEGASATURN ")
    write_field(blob, 0x010, 16, maker)
    write_field(blob, 0x020, 10, product)
    write_field(blob, 0x02A, 6, version)
    write_field(blob, 0x030, 8, date_yyyymmdd)
    blob[0x038] = 0x00  # CD-ROM
    blob[0x039] = 0x01  # 1 disc
    blob[0x03A] = 0x00
    blob[0x03B] = 0x00
    write_field(blob, 0x03C, 10, DEFAULT_AREA_SYMBOLS)
    write_field(blob, 0x046, 6, "J     ")
    blob[0x04C:0x050] = b" " * 4
    write_field(blob, 0x050, 112, game_name)
    blob[0x0C0:0x0D0] = b"\x00" * 0x10

    stub_data = b""
    if stub_path:
        stub_data = Path(stub_path).read_bytes()
    aligned_stub_size = align_2048(len(stub_data)) if stub_data else 0
    if aligned_stub_size > IP_CODE_LIMIT:
        raise ValueError(
            f"stub muito grande ({len(stub_data)} bytes, alinhado {aligned_stub_size}); limite {IP_CODE_LIMIT}"
        )

    struct.pack_into(">I", blob, 0x0D0, ip_load_address)
    struct.pack_into(">I", blob, 0x0D4, aligned_stub_size)
    struct.pack_into(">I", blob, 0x0D8, master_stack)
    struct.pack_into(">I", blob, 0x0DC, slave_stack)
    struct.pack_into(">I", blob, 0x0E0, first_read_address)
    struct.pack_into(">I", blob, 0x0E4, first_read_size)
    blob[0x0E8:0x100] = b"\x00" * 0x18

    # Security block area (0x100 - 0x5FF) is left zero-filled for emulator workflows.
    if stub_data:
        blob[IP_CODE_OFFSET : IP_CODE_OFFSET + len(stub_data)] = stub_data

    return bytes(blob)


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate Saturn-compatible ip.bin")
    parser.add_argument("--output", required=True, help="Output path for ip.bin")
    parser.add_argument("--maker", default="SEGA ENTERPRISES", help="Maker string")
    parser.add_argument("--product", default="T-00000G  ", help="Product code (10 chars)")
    parser.add_argument("--version", default="V1.000", help="Version field (6 chars)")
    parser.add_argument("--date", default="20260310", help="Release date YYYYMMDD")
    parser.add_argument("--name", default="LIBSATURN MVP", help="Game name (max 112 chars)")
    parser.add_argument("--ip-load-address", default=f"{DEFAULT_IP_LOAD_ADDRESS:08X}", help="IP load address (hex)")
    parser.add_argument(
        "--first-read-address",
        default=None,
        help="First read address (hex). Defaults to --entry when provided, otherwise 06010000",
    )
    parser.add_argument(
        "--entry",
        default=None,
        help="Backward-compatible alias for first read address (hex, deprecated)",
    )
    parser.add_argument("--first-read-size", default=None, help="First read size in bytes (dec or 0xhex)")
    parser.add_argument("--first-read-file", default=None, help="Path to binary used to infer first read size")
    parser.add_argument("--master-stack", default=f"{DEFAULT_MASTER_STACK:08X}", help="Master stack address (hex)")
    parser.add_argument("--slave-stack", default=f"{DEFAULT_SLAVE_STACK:08X}", help="Slave stack address (hex)")
    parser.add_argument("--stub", default=None, help="Path to IP boot stub binary (embedded at offset 0x600)")
    args = parser.parse_args()

    if args.first_read_file and args.first_read_size is not None:
        raise ValueError("use apenas um entre --first-read-size e --first-read-file")

    if args.first_read_address is not None:
        first_read_address = parse_hex_u32(args.first_read_address)
    elif args.entry is not None:
        first_read_address = parse_hex_u32(args.entry)
    else:
        first_read_address = DEFAULT_FIRST_READ_ADDRESS

    if args.first_read_file:
        first_read_size = Path(args.first_read_file).stat().st_size
    elif args.first_read_size is not None:
        first_read_size = parse_u32(args.first_read_size)
    else:
        first_read_size = 0

    data = build_ip_bin(
        maker=args.maker,
        product=args.product,
        version=args.version,
        date_yyyymmdd=args.date,
        game_name=args.name,
        ip_load_address=parse_hex_u32(args.ip_load_address),
        first_read_address=first_read_address,
        first_read_size=first_read_size,
        master_stack=parse_hex_u32(args.master_stack),
        slave_stack=parse_hex_u32(args.slave_stack),
        stub_path=args.stub,
    )

    out = Path(args.output)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(data)
    print(f"Wrote {len(data)} bytes to {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
