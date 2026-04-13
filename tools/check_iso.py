"""Valida ISO gerada verificando o primeiro setor (ip.bin)."""
import argparse
import struct
import sys
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description="Validate ISO first sector")
    parser.add_argument("--iso", required=True, help="Path to ISO file")
    parser.add_argument("--expected-size", type=int, required=True, help="Expected app size")
    parser.add_argument("--profile", default="current", help="IP profile")
    parser.add_argument("--load-addr", default="0x06004000", help="Expected load address")
    args = parser.parse_args()

    load_addr = int(args.load_addr, 16)
    expected_first_size = 0 if args.profile == "safe" else args.expected_size

    iso = Path(args.iso).read_bytes()
    h = iso[:2048]
    magic = h[0:16]
    first_read = struct.unpack(">I", h[0x0F0:0x0F4])[0]
    first_size = struct.unpack(">I", h[0x0F4:0x0F8])[0]

    ok = magic == b"SEGA SEGASATURN " and first_read == load_addr and first_size == expected_first_size

    print(f"[check] iso profile={args.profile} lba0 magic={magic!r} "
          f"first_read=0x{first_read:08X} first_size=0x{first_size:08X}")

    if not ok:
        sys.exit(1)


if __name__ == "__main__":
    main()
