#!/usr/bin/env python3
"""Inspect libsaturn emulator captures without ad-hoc PIL snippets.

Subcommands:
  pixels     sample exact pixels, print a horizontal span, or list common colors
  bbox       count an exact color and print its bounding box
  registers  print selected VDP2 registers from a probe.json
  struct     decode a big-endian telemetry struct from a Work RAM dump

Examples:
  python tools/inspect_capture.py pixels capture/char.png --at 10,100 --at 11,100
  python tools/inspect_capture.py pixels capture/char.png --span 100:0:40:2
  python tools/inspect_capture.py pixels capture/char.png --top 8
  python tools/inspect_capture.py bbox capture/char.png --color 248,0,248
  python tools/inspect_capture.py registers capture/probe.json --keys sfprmd,sfccmd,sfcode,sfsel
  python tools/inspect_capture.py struct --dump capture/wram.bin \
      --map build/vdp2_special_priority.map --symbol _g_special_priority --count 13

Pillow is only needed by the `pixels` and `bbox` subcommands; `registers` and
`struct` use the standard library.
"""

import argparse
import json
import re
import struct
import sys
from pathlib import Path

WRAM_HIGH = 0x06000000
WRAM_SIZE = 0x100000

DEFAULT_REGISTERS = (
    "ccctl,ccrna,ccrnb,lnclen,sfprmd,sfccmd,sfcode,sfsel,prir,prina,prinb"
)


def parse_at(text):
    parts = text.split(",")
    if len(parts) != 2:
        raise argparse.ArgumentTypeError("expected X,Y")
    return int(parts[0], 0), int(parts[1], 0)


def parse_color(text):
    value = text.strip()
    if value.startswith("#"):
        value = value[1:]
    if "," in value:
        parts = [part.strip() for part in value.split(",")]
        if len(parts) != 3:
            raise argparse.ArgumentTypeError("expected R,G,B or #RRGGBB")
        channels = tuple(int(part, 0) for part in parts)
    else:
        if value.lower().startswith("0x"):
            value = value[2:]
        if len(value) != 6:
            raise argparse.ArgumentTypeError("expected R,G,B or #RRGGBB")
        channels = tuple(int(value[i:i + 2], 16) for i in (0, 2, 4))
    if any(channel < 0 or channel > 255 for channel in channels):
        raise argparse.ArgumentTypeError("color channels must be 0-255")
    return channels


def parse_span(text):
    parts = [int(part, 0) for part in text.split(":")]
    if len(parts) not in (3, 4):
        raise argparse.ArgumentTypeError("expected Y:X0:X1[:STEP]")
    y, x0, x1 = parts[:3]
    step = parts[3] if len(parts) == 4 else 1
    if step <= 0:
        raise argparse.ArgumentTypeError("step must be positive")
    return y, x0, x1, step


def load_image(path):
    try:
        from PIL import Image
    except ImportError:  # pragma: no cover - environment dependent
        raise SystemExit("Pillow is required for pixels/bbox: pip install pillow")
    return Image.open(path).convert("RGB")


def color_hex(color):
    return "#%02X%02X%02X" % color


def find_color_bounds(image, color, tolerance=0):
    """Returns (count, (x0, y0, x1, y1)) for an exact/tolerant RGB match."""
    pixels = image.load()
    width, height = image.size
    count = 0
    x0 = y0 = 1 << 30
    x1 = y1 = -1
    for y in range(height):
        for x in range(width):
            pixel = pixels[x, y]
            if all(abs(pixel[i] - color[i]) <= tolerance for i in range(3)):
                count += 1
                x0 = min(x0, x)
                y0 = min(y0, y)
                x1 = max(x1, x)
                y1 = max(y1, y)
    if count == 0:
        return 0, None
    return count, (x0, y0, x1, y1)


def unpack_struct(dump_path, map_path, symbol, count):
    """Reads `count` big-endian uint32s at `symbol` from a High WRAM dump."""
    map_text = Path(map_path).read_text(encoding="utf-8")
    pattern = re.compile(r"^\s*0x([0-9A-Fa-f]+)\s+" + re.escape(symbol) + r"\s*$",
                         re.MULTILINE)
    match = pattern.search(map_text)
    if not match:
        raise SystemExit("%s is absent from %s" % (symbol, map_path))
    address = int(match.group(1), 16)
    if address < WRAM_HIGH or address + 4 * count > WRAM_HIGH + WRAM_SIZE:
        raise SystemExit("0x%08X is outside High Work RAM" % address)
    data = Path(dump_path).read_bytes()
    offset = address - WRAM_HIGH
    if offset + 4 * count > len(data):
        raise SystemExit("dump is shorter than the requested struct")
    return address, struct.unpack_from(">%dI" % count, data, offset)


def format_register(value):
    if isinstance(value, int):
        pattern = "0x%04X (%d)" if 0 <= value <= 0xFFFF else "0x%X (%d)"
        return pattern % (value, value)
    return str(value)


def cmd_pixels(args):
    image = load_image(args.image)
    width, height = image.size
    print("%s: %dx%d" % (args.image, width, height))
    pixels = image.load()

    for x, y in args.at:
        if not (0 <= x < width and 0 <= y < height):
            raise SystemExit("pixel %d,%d is outside the image" % (x, y))
        color = pixels[x, y]
        print("  (%3d,%3d) = %-15s %s" % (x, y, str(color), color_hex(color)))

    if args.span:
        y, x0, x1, step = args.span
        if not (0 <= y < height) or x0 < 0 or x1 >= width:
            raise SystemExit("span is outside the image")
        values = []
        for x in range(x0, x1 + 1, step):
            color = pixels[x, y]
            values.append("%d:%s" % (x, color_hex(color)))
        print("  y=%d: %s" % (y, " ".join(values)))

    if not args.at and not args.span:
        counts = {}
        for color in image.getdata():
            counts[color] = counts.get(color, 0) + 1
        top = sorted(counts.items(), key=lambda item: (-item[1], item[0]))[:args.top]
        print("  top %d colors:" % len(top))
        for color, count in top:
            print("    %-15s %s  %d px" % (str(color), color_hex(color), count))


def cmd_bbox(args):
    image = load_image(args.image)
    count, bounds = find_color_bounds(image, args.color, args.tolerance)
    print("%s: %d px of %s" % (args.image, count, color_hex(args.color)))
    if bounds is not None:
        x0, y0, x1, y1 = bounds
        print("  bbox x=%d..%d y=%d..%d" % (x0, x1, y0, y1))
    if count == 0:
        raise SystemExit(1)


def cmd_registers(args):
    document = json.loads(Path(args.probe).read_text(encoding="utf-8"))
    registers = document.get("vdp2")
    if not isinstance(registers, dict):
        raise SystemExit("%s has no vdp2 object" % args.probe)
    keys = args.keys.split(",") if args.keys else DEFAULT_REGISTERS.split(",")
    print("%s:" % args.probe)
    for key in keys:
        key = key.strip()
        if key not in registers:
            print("  %-8s <missing>" % key)
            continue
        print("  %-8s %s" % (key, format_register(registers[key])))


def cmd_struct(args):
    address, words = unpack_struct(args.dump, args.map, args.symbol, args.count)
    names = args.names.split(",") if args.names else []
    print("%s @ 0x%08X:" % (args.symbol, address))
    for index, word in enumerate(words):
        name = names[index].strip() if index < len(names) else "[%d]" % index
        print("  %-32s 0x%08X  %d" % (name, word, word))


def build_parser():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    subparsers = parser.add_subparsers(dest="command", required=True)

    pixels = subparsers.add_parser("pixels", help="sample pixels, a span, or top colors")
    pixels.add_argument("image")
    pixels.add_argument("--at", type=parse_at, action="append", default=[],
                        metavar="X,Y", help="exact pixel; repeatable")
    pixels.add_argument("--span", type=parse_span, metavar="Y:X0:X1[:STEP]",
                        help="horizontal span, inclusive")
    pixels.add_argument("--top", type=int, default=8,
                        help="number of common colors when no --at/--span is given")
    pixels.set_defaults(func=cmd_pixels)

    bbox = subparsers.add_parser("bbox", help="count a color and print its bounding box")
    bbox.add_argument("image")
    bbox.add_argument("--color", type=parse_color, required=True, metavar="R,G,B")
    bbox.add_argument("--tolerance", type=int, default=0)
    bbox.set_defaults(func=cmd_bbox)

    registers = subparsers.add_parser("registers", help="read VDP2 registers from probe.json")
    registers.add_argument("probe")
    registers.add_argument("--keys", help="comma-separated register names")
    registers.set_defaults(func=cmd_registers)

    structure = subparsers.add_parser("struct", help="decode a telemetry struct from WRAM")
    structure.add_argument("--dump", required=True)
    structure.add_argument("--map", required=True)
    structure.add_argument("--symbol", required=True)
    structure.add_argument("--count", type=int, required=True)
    structure.add_argument("--names", help="comma-separated field names")
    structure.set_defaults(func=cmd_struct)
    return parser


def main(argv=None):
    args = build_parser().parse_args(argv)
    args.func(args)
    return 0


if __name__ == "__main__":
    sys.exit(main())
