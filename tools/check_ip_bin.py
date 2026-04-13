"""Valida ip.bin gerado."""
import argparse
import struct
import sys
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description="Validate ip.bin")
    parser.add_argument("--ip-bin", required=True, help="Path to ip.bin")
    parser.add_argument("--template", required=True, help="Path to template bin")
    parser.add_argument("--expected-size", type=int, required=True, help="Expected app size")
    parser.add_argument("--profile", default="current", help="IP profile")
    parser.add_argument("--load-addr", default="0x06004000", help="Expected load address")
    args = parser.parse_args()

    load_addr = int(args.load_addr, 16)
    expected_first_size = 0 if args.profile == "safe" else args.expected_size

    tmpl = bytearray(Path(args.template).read_bytes())
    tmpl.extend(b"\x00" * (0x8000 - len(tmpl)))

    d = Path(args.ip_bin).read_bytes()
    magic = d[0:16]
    area_symbols = d[0x40:0x4A]
    first_read = struct.unpack(">I", d[0x0F0:0x0F4])[0]
    first_size = struct.unpack(">I", d[0x0F4:0x0F8])[0]

    security_ok = d[0x0100:0x0600] == bytes(tmpl[0x0100:0x0600])
    area_obj_ok = d[0x0E00:0x8000] == bytes(tmpl[0x0E00:0x8000])
    header_unchanged = d[0x0010:0x00F0] == bytes(tmpl[0x0010:0x00F0])

    errs = []
    if len(d) != 0x8000:
        errs.append("size")
    if magic != b"SEGA SEGASATURN ":
        errs.append("magic")
    if not header_unchanged:
        errs.append("header_modified")
    if first_read != load_addr:
        errs.append("first_read")
    if first_size != expected_first_size:
        errs.append("first_size")
    if not security_ok:
        errs.append("security_block_modified")
    if not area_obj_ok:
        errs.append("area_code_object_modified")

    print(f"[check] ip.bin profile={args.profile} len={len(d)} "
          f"magic={magic!r} area={area_symbols!r} "
          f"first_read=0x{first_read:08X} first_size=0x{first_size:08X} "
          f"header_unchanged={header_unchanged} security_ok={security_ok} "
          f"area_obj_ok={area_obj_ok}")

    if errs:
        print(f"[check] ip.bin FAIL: {', '.join(errs)}")
        sys.exit(1)


if __name__ == "__main__":
    main()
