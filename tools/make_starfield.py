#!/usr/bin/env python3
"""Shrink a photographic starfield down to a Saturn-sized tile.

Run once to regenerate examples/pacman_3d/assets/stars.png from the original
photo; the result is committed, so the build does not depend on the source
image being present.

    python tools/make_starfield.py ~/Downloads/seamless-stars.jpg \
        examples/pacman_3d/assets/stars.png

WHY THIS IS NOT A RESIZE
------------------------
The source is 7001x4001 and the target is 256x256, so every output pixel
covers roughly a 27x15 block of input. A star is a handful of bright pixels
on black, so averaging that block -- which is what every ordinary resampler,
Lanczos included, does -- divides the star's brightness by the block area and
leaves a uniform dark grey. The stars do not get smaller, they get erased.

So each block contributes its BRIGHTEST pixel instead. A block containing a
star becomes that star; a block of empty space stays black. Star count and
star brightness both survive, and each star lands on exactly one pixel, which
is what a starfield on this hardware should look like anyway.

This works here because the background really is black (median luminance 6,
and 95% of blocks come out under 8), so taking a maximum does not amplify
JPEG noise into a grey haze. On a source with a bright or gradient sky it
would, and this script would be the wrong tool.

Tiling is preserved: the whole image is consumed, so a seamless source stays
seamless. The 1.75:1 aspect of the source is squeezed into a square, which is
invisible on a field of points.
"""

import argparse
import sys

try:
    from PIL import Image
except ModuleNotFoundError:
    sys.exit("needs Pillow: pip install pillow")

try:
    import numpy as np
except ModuleNotFoundError:
    sys.exit("needs numpy: pip install numpy")


def max_pool(image, size, trim):
    """Downsample to size x size, keeping the brightest pixel of each block."""
    if trim > 0:
        # Crop before pooling, because a max filter is maximally sensitive to
        # exactly the kind of defect that lives on an image's edge. This
        # source has a one-pixel bright border (mean luminance 67-130 against
        # an interior of 6), left over from however it was encoded, and each
        # of those pixels would win its whole 27x15 block outright -- turning
        # the first row and column of the output into solid grey, which then
        # shows up on screen as a bright line at every tile boundary.
        image = image.crop((trim, trim, image.width - trim, image.height - trim))
    width, height = image.size
    block_w = width // size
    block_h = height // size
    if block_w < 1 or block_h < 1:
        sys.exit(f"source {width}x{height} is smaller than the {size}x{size} target")

    # Trim the remainder so every block is the same shape, then collapse the
    # two within-block axes. Taking the max per channel rather than picking
    # the brightest pixel whole is deliberate: it keeps a star's colour cast
    # without needing a luminance argmax.
    cropped = np.asarray(image.crop((0, 0, block_w * size, block_h * size)), dtype=np.uint8)
    return cropped.reshape(size, block_h, size, block_w, 3).max(axis=(1, 3))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("input")
    ap.add_argument("output")
    ap.add_argument("--size", type=int, default=256,
                    help="output edge in pixels, a multiple of 8 (default 256)")
    ap.add_argument("--trim", type=int, default=2,
                    help="pixels to crop from each edge before pooling, to "
                         "drop edge artefacts a max filter would amplify "
                         "(default 2)")
    ap.add_argument("--floor", type=int, default=10,
                    help="luminance at or below which a pixel is forced to "
                         "pure black, so the sky quantises to one palette "
                         "entry instead of a dozen near-blacks (default 10)")
    args = ap.parse_args()

    if args.size % 8 != 0:
        sys.exit("--size must be a multiple of 8: VDP2 cells are 8x8")

    pixels = max_pool(Image.open(args.input).convert("RGB"), args.size, args.trim)

    # Collapse the near-black background to exactly black. Without this the
    # adaptive palette spends most of its 256 entries separating shades of
    # empty space that nobody can tell apart, and has none left for the stars.
    luminance = pixels.astype(np.int32).sum(axis=2) // 3
    pixels[luminance <= args.floor] = 0

    Image.fromarray(pixels).save(args.output)
    lit = int((luminance > args.floor).sum())
    total = args.size * args.size
    print(f"{args.output}: {args.size}x{args.size}, "
          f"{lit} lit pixels ({100.0 * lit / total:.1f}%)")


if __name__ == "__main__":
    sys.exit(main())
