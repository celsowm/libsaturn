"""Gera ip.bin a partir de um template e tamanho do app."""
import argparse
import struct
import sys
from pathlib import Path

try:
    from memory_layout import APPLICATION_LOAD_ADDRESS, MASTER_STACK_TOP, SLAVE_STACK_TOP, validate
except ModuleNotFoundError:  # Imported by the repository's host test loader.
    from tools.memory_layout import APPLICATION_LOAD_ADDRESS, MASTER_STACK_TOP, SLAVE_STACK_TOP, validate

validate()


def _field(value: str, size: int) -> bytes:
    """Encode an IP.BIN fixed-width ASCII field, padding with spaces."""
    raw = value.encode("ascii")
    if len(raw) > size:
        raise ValueError(f"IP field too large ({len(raw)} > {size})")
    return raw.ljust(size, b" ")


def build_ip_bin(*, maker: str, product: str, version: str,
                 date_yyyymmdd: str, game_name: str, ip_load_address: int,
                 first_read_address: int, first_read_size: int,
                 master_stack: int, slave_stack: int,
                 stub_path: str | None = None) -> bytes:
    """Build the fixed 32 KiB Saturn IP header.

    The function is intentionally pure so importers and host tests can verify
    the boot contract without touching the filesystem.  The optional stub is
    placed at the documented 0x600 entry point and cannot overlap the area
    code table at 0xE00.
    """
    if first_read_size < 0 or first_read_address < 0 or ip_load_address < 0:
        raise ValueError("addresses and first read size must be non-negative")
    if slave_stack != SLAVE_STACK_TOP:
        raise ValueError(f"slave stack must be 0x{SLAVE_STACK_TOP:08X}")
    src = bytearray(0x8000)
    src[0x000:0x010] = b"SEGA SEGASATURN "
    src[0x020:0x030] = _field(maker, 16)
    # The legacy product field occupies eight bytes in the Saturn header;
    # callers commonly pass the longer human-readable product string.
    src[0x030:0x038] = _field(product[:8], 8)
    src[0x038:0x040] = b"CD-1/1  "
    # Region/version and release-date slots are fixed-format fields.  Keep
    # the conventional U/J markers used by the Saturn IP contract; the
    # supplied metadata remains useful to callers of the pure builder even
    # though the legacy boot ROM fields are not free-form text.
    src[0x040:0x04A] = _field("U", 10)
    src[0x050:0x060] = _field("J", 16)
    src[0x060:0x080] = _field(game_name, 32)
    struct.pack_into(">I", src, 0x0E0, 0x1000)
    struct.pack_into(">I", src, 0x0E8, master_stack)
    struct.pack_into(">I", src, 0x0EC, slave_stack)
    struct.pack_into(">I", src, 0x0F0, first_read_address)
    struct.pack_into(">I", src, 0x0F4, first_read_size)
    if stub_path is not None:
        stub = Path(stub_path).read_bytes()
        if len(stub) > 0x0E00 - 0x0600:
            raise ValueError("stub sobrepoe area code")
        src[0x0600:0x0600 + len(stub)] = stub
    src[0x0E00:0x0E20] = _field("For USA and CANADA.", 32)
    src[0x0E20:0x0E40] = b" " * 32
    return bytes(src)


def main():
    parser = argparse.ArgumentParser(description="Generate ip.bin for Sega Saturn")
    parser.add_argument("--template", help="Path to ip template bin")
    parser.add_argument("--output", required=True, help="Output ip.bin path")
    parser.add_argument("--app-size", type=int, help="App binary size in bytes")
    parser.add_argument("--profile", default="current", choices=["current", "safe"], help="IP profile")
    parser.add_argument("--load-addr", default="0x06004000", help="Load address hex")
    parser.add_argument("--entry", help="Compatibility alias for first-read address")
    parser.add_argument("--first-read-file", help="Compatibility input used to derive first-read size")
    args = parser.parse_args()

    load_addr = int(args.load_addr, 16)
    if args.entry:
        load_addr = int(args.entry, 16)
    if args.first_read_file:
        first_size = Path(args.first_read_file).stat().st_size
    elif args.app_size is not None:
        first_size = 0 if args.profile == "safe" else args.app_size
    else:
        first_size = 0
    if args.template:
        tmpl = Path(args.template).read_bytes()
        if len(tmpl) > 0x8000:
            print(f"[gen] ERROR: template too large ({len(tmpl)} > 0x8000)", file=sys.stderr)
            sys.exit(1)
        src = bytearray(tmpl.ljust(0x8000, b"\x00"))
        template_master_stack = struct.unpack_from(">I", src, 0x0E8)[0]
        template_slave_stack = struct.unpack_from(">I", src, 0x0EC)[0]
        if template_master_stack not in (0, MASTER_STACK_TOP):
            raise ValueError("template master stack conflicts with memory layout")
        if template_slave_stack not in (0, SLAVE_STACK_TOP):
            raise ValueError("template slave stack conflicts with memory layout")
        struct.pack_into(">I", src, 0x0F0, load_addr)
        struct.pack_into(">I", src, 0x0F4, first_size)
    else:
        src = bytearray(build_ip_bin(
            maker="SEGA ENTERPRISES", product="T-00000G", version="V1.000",
            date_yyyymmdd="20260311", game_name="LIBSATURN MVP",
            ip_load_address=APPLICATION_LOAD_ADDRESS, first_read_address=load_addr,
            first_read_size=first_size, master_stack=0x060FFFFC,
            slave_stack=SLAVE_STACK_TOP))

    Path(args.output).write_bytes(src)
    print(f"[gen] ip.bin profile={args.profile} size={len(src)} "
          f"first_read=0x{load_addr:08X} first_size=0x{first_size:08X}")


if __name__ == "__main__":
    main()
