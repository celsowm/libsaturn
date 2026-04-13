"""Gera ip.bin a partir de um template e tamanho do app."""
import argparse
import struct
import sys
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description="Generate ip.bin for Sega Saturn")
    parser.add_argument("--template", required=True, help="Path to ip template bin")
    parser.add_argument("--output", required=True, help="Output ip.bin path")
    parser.add_argument("--app-size", type=int, required=True, help="App binary size in bytes")
    parser.add_argument("--profile", default="current", choices=["current", "safe"], help="IP profile")
    parser.add_argument("--load-addr", default="0x06004000", help="Load address hex")
    args = parser.parse_args()

    load_addr = int(args.load_addr, 16)
    first_size = 0 if args.profile == "safe" else args.app_size

    tmpl = bytearray(Path(args.template).read_bytes())
    if len(tmpl) > 0x8000:
        print(f"[gen] ERROR: template too large ({len(tmpl)} > 0x8000)", file=sys.stderr)
        sys.exit(1)

    tmpl.extend(b"\x00" * (0x8000 - len(tmpl)))
    src = bytearray(tmpl)

    struct.pack_into(">I", src, 0x0F0, load_addr)
    struct.pack_into(">I", src, 0x0F4, first_size)

    Path(args.output).write_bytes(src)
    print(f"[gen] ip.bin profile={args.profile} size={len(src)} "
          f"first_read=0x{load_addr:08X} first_size=0x{first_size:08X}")


if __name__ == "__main__":
    main()
