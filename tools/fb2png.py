#!/usr/bin/env python3
"""Convert a raw VDP1 framebuffer dump into a PNG you can actually look at.

The dump comes from the harness probe's --dump-fb (see harness/README.md):
512x256 16-bit big-endian words, 262144 bytes.

Saturn colour words are BGR555 -- red in bits 0-4, green 5-9, blue 10-14 --
which is the reverse of what "RGB555" suggests; getting it backwards produces
a plausible picture in the wrong colours rather than an obvious error. Bit 15
is the RGB code. A word of 0 is the framebuffer's transparent "no data" code,
where the VDP2 layers below show through; it is rendered here as black, since
this dump has no VDP2 composite to show.

Usage:
    python tools/fb2png.py <dump.fb> <out.png> [width height] [--scale N]

Width and height crop the 512x256 framebuffer to the visible display area
(default 320x224). --scale enlarges with nearest-neighbour so individual
pixels stay square and countable (default 2).

Requires Pillow. On this project's Windows setup the interpreter that has it
is C:/Python/python.exe, not the MSYS2 one.
"""
import sys

FB_W, FB_H = 512, 256
FB_BYTES = FB_W * FB_H * 2


def parse_args(argv):
    args = [a for a in argv[1:] if not a.startswith("--")]
    flags = [a for a in argv[1:] if a.startswith("--")]
    if len(args) < 2:
        sys.exit(__doc__)
    raw, out = args[0], args[1]
    crop_w = int(args[2]) if len(args) > 2 else 320
    crop_h = int(args[3]) if len(args) > 3 else 224
    scale = 2
    for f in flags:
        if f.startswith("--scale"):
            scale = int(f.split("=", 1)[1]) if "=" in f else 2
    return raw, out, crop_w, crop_h, scale


def main(argv):
    from PIL import Image

    raw, out, crop_w, crop_h, scale = parse_args(argv)

    data = open(raw, "rb").read()
    if len(data) < FB_BYTES:
        sys.exit(f"{raw}: {len(data)} bytes, expected {FB_BYTES} "
                 f"({FB_W}x{FB_H} 16-bit words)")

    img = Image.new("RGB", (FB_W, FB_H))
    px = img.load()
    nonzero = 0
    for y in range(FB_H):
        row = y * FB_W * 2
        for x in range(FB_W):
            w = (data[row + x * 2] << 8) | data[row + x * 2 + 1]
            if w:
                nonzero += 1
            px[x, y] = ((w & 0x1F) << 3,
                        ((w >> 5) & 0x1F) << 3,
                        ((w >> 10) & 0x1F) << 3)

    crop = img.crop((0, 0, crop_w, crop_h))
    crop.resize((crop_w * scale, crop_h * scale), Image.NEAREST).save(out)

    # Report how much of the visible area is actually lit, so a blank or
    # near-blank capture is obvious from the log without opening the file.
    cpx = crop.load()
    lit = sum(1 for y in range(crop_h) for x in range(crop_w)
              if cpx[x, y] != (0, 0, 0))
    total = crop_w * crop_h
    print(f"{out}: {nonzero}/{FB_W * FB_H} non-transparent framebuffer words; "
          f"{lit}/{total} ({100.0 * lit / total:.1f}%) visible pixels lit")


if __name__ == "__main__":
    main(sys.argv)
