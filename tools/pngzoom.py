#!/usr/bin/env python3
"""Crop and magnify a region of a PNG, for inspecting individual sprites.

A Saturn actor is a dozen pixels across in a 320x224 screenshot, which is too
small to judge by eye -- whether a mouth wedge reads as a mouth, or two eyes
have merged into one band, is invisible until the thing is blown up. This
crops a region and scales it with nearest-neighbour so the pixels stay square
and countable.

Reads and writes PNG with no third-party dependencies: it decodes the
IDAT/filter chain itself and writes back the same uncompressed-deflate form
harness/src/png_writer.hpp uses, so it round-trips the probe's screenshots
without needing Pillow installed.

    python tools/pngzoom.py shot.png out.png --at 37,172 --size 32,32 --scale 8
    python tools/pngzoom.py shot.png out.png --find ffdd00 --size 40,40

--find locates the first pixel matching a hex colour (within --tolerance) and
centres the crop on it, which saves hunting for coordinates by hand.
"""

import argparse
import struct
import sys
import zlib


def read_png(path):
    """Returns (width, height, rows) with rows as lists of (r, g, b)."""
    data = open(path, "rb").read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit(f"{path}: not a PNG")

    pos = 8
    idat = b""
    width = height = depth = color_type = 0
    while pos < len(data):
        length = struct.unpack(">I", data[pos : pos + 4])[0]
        ctype = data[pos + 4 : pos + 8]
        chunk = data[pos + 8 : pos + 8 + length]
        pos += 12 + length
        if ctype == b"IHDR":
            width, height, depth, color_type = struct.unpack(">IIBB", chunk[:10])
        elif ctype == b"IDAT":
            idat += chunk
        elif ctype == b"IEND":
            break

    if depth != 8 or color_type not in (2, 6):
        raise SystemExit(
            f"{path}: only 8-bit truecolour PNGs are supported "
            f"(got depth {depth}, colour type {color_type})"
        )

    channels = 3 if color_type == 2 else 4
    raw = zlib.decompress(idat)
    stride = width * channels
    rows = []
    previous = bytearray(stride)
    pos = 0
    for _ in range(height):
        filter_type = raw[pos]
        pos += 1
        line = bytearray(raw[pos : pos + stride])
        pos += stride
        # Undo the per-scanline filter (PNG spec section 9).
        for i in range(stride):
            a = line[i - channels] if i >= channels else 0
            b = previous[i]
            c = previous[i - channels] if i >= channels else 0
            if filter_type == 1:
                line[i] = (line[i] + a) & 0xFF
            elif filter_type == 2:
                line[i] = (line[i] + b) & 0xFF
            elif filter_type == 3:
                line[i] = (line[i] + ((a + b) >> 1)) & 0xFF
            elif filter_type == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pred = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pred) & 0xFF
            elif filter_type != 0:
                raise SystemExit(f"{path}: unknown filter type {filter_type}")
        previous = line
        rows.append(
            [tuple(line[x * channels : x * channels + 3]) for x in range(width)]
        )
    return width, height, rows


def write_png(path, rows):
    height = len(rows)
    width = len(rows[0])

    raw = bytearray()
    for row in rows:
        raw.append(0)  # filter: none
        for r, g, b in row:
            raw += bytes((r, g, b))

    # Stored (uncompressed) deflate blocks, same as the harness writer.
    z = bytearray(b"\x78\x01")
    block = 65535
    for off in range(0, len(raw), block):
        n = min(block, len(raw) - off)
        last = 1 if off + n >= len(raw) else 0
        z += bytes((last, n & 0xFF, (n >> 8) & 0xFF, ~n & 0xFF, (~n >> 8) & 0xFF))
        z += raw[off : off + n]
    a, b = 1, 0
    for byte in raw:
        a = (a + byte) % 65521
        b = (b + a) % 65521
    z += struct.pack(">I", (b << 16) | a)

    def chunk(tag, payload):
        return (
            struct.pack(">I", len(payload))
            + tag
            + payload
            + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)
        )

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)))
        f.write(chunk(b"IDAT", bytes(z)))
        f.write(chunk(b"IEND", b""))


def find_color(rows, target, tolerance, skip_top):
    for y, row in enumerate(rows):
        if y < skip_top:
            continue
        for x, px in enumerate(row):
            if all(abs(px[i] - target[i]) <= tolerance for i in range(3)):
                return x, y
    return None


def parse_pair(text, what):
    try:
        a, b = text.split(",")
        return int(a), int(b)
    except ValueError:
        raise SystemExit(f"--{what} wants two numbers like 40,32")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("input")
    ap.add_argument("output")
    ap.add_argument("--at", help="crop centre as X,Y")
    ap.add_argument("--find", help="hex colour (RRGGBB) to centre on instead")
    ap.add_argument("--tolerance", type=int, default=24,
                    help="per-channel slack for --find (default 24)")
    ap.add_argument("--skip-top", type=int, default=0,
                    help="ignore this many top rows when searching, to step "
                         "over a HUD that uses the same colour")
    ap.add_argument("--size", default="32,32", help="crop size as W,H (default 32,32)")
    ap.add_argument("--scale", type=int, default=8, help="magnification (default 8)")
    args = ap.parse_args()

    width, height, rows = read_png(args.input)
    crop_w, crop_h = parse_pair(args.size, "size")

    if args.find:
        target = tuple(int(args.find[i : i + 2], 16) for i in (0, 2, 4))
        hit = find_color(rows, target, args.tolerance, args.skip_top)
        if hit is None:
            raise SystemExit(f"no pixel within {args.tolerance} of #{args.find}")
        cx, cy = hit
        print(f"found #{args.find} at {cx},{cy}")
    elif args.at:
        cx, cy = parse_pair(args.at, "at")
    else:
        cx, cy = width // 2, height // 2

    x0 = max(0, min(width - crop_w, cx - crop_w // 2))
    y0 = max(0, min(height - crop_h, cy - crop_h // 2))

    out = []
    for y in range(y0, min(height, y0 + crop_h)):
        line = []
        for x in range(x0, min(width, x0 + crop_w)):
            line.extend([rows[y][x]] * args.scale)
        out.extend([line] * args.scale)

    write_png(args.output, out)
    print(f"{args.output}: {len(out[0])}x{len(out)} "
          f"(crop {x0},{y0} {crop_w}x{crop_h} at {args.scale}x)")


if __name__ == "__main__":
    sys.exit(main())
