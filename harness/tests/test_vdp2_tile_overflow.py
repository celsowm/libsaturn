r"""Acceptance for 8bpp scroll screens with more than 512 tiles (examples/vdp2_tile_overflow).

Run through the wrapper, which builds the demo, runs Ymir, and dumps Work RAM and a screenshot:

    .\harness\run-vdp2-tile-overflow.ps1 -Bios .\bios\saturn_bios_us.bin

NBG0 shows tiles 0-510 of a 40x24 map and NBG1 (the next NBG up, reading the next 32 KiB character
window) shows tiles 511-959, with a third 256-colour layer (NBG2, a checkerboard) behind both. Every
tile is a solid colour that ramps with its number, so a tile past 511 that wrapped onto tile 0-448
would be a visibly wrong colour. The struct read here is g_overflow_demo.
"""

import os
import re
import struct
import unittest

MAGIC = 0x4F564631  # "OVF1"
WRAM_HIGH = 0x06000000
MAP_W, MAP_H = 40, 24
TILES = MAP_W * MAP_H
OV_FIRST = 511
STRUCT_BYTES = 72
SHIFT_X, SHIFT_Y = 3, 4   # every layer is scrolled by (-3, -4)


def _load():
    dump = os.environ.get("LIBSATURN_OVERFLOW_DUMP")
    map_path = os.environ.get("LIBSATURN_OVERFLOW_MAP")
    if not dump or not map_path:
        raise unittest.SkipTest("run harness/run-vdp2-tile-overflow.ps1 first")
    if not os.path.exists(dump) or not os.path.exists(map_path):
        raise AssertionError("missing input: %s / %s" % (dump, map_path))
    with open(map_path, "r", encoding="utf-8", errors="replace") as stream:
        found = re.search(r"(?m)^\s*0x([0-9A-Fa-f]+)\s+_g_overflow_demo\s*$", stream.read())
    if not found:
        raise AssertionError("_g_overflow_demo is absent from " + map_path)
    offset = int(found.group(1), 16) - WRAM_HIGH
    with open(dump, "rb") as stream:
        image = stream.read()[offset:offset + STRUCT_BYTES]
    raw = struct.unpack(">18I", image)
    return {
        "magic": raw[0],
        "configure": [struct.unpack(">i", struct.pack(">I", v))[0] for v in raw[1:4]],
        "enable": [struct.unpack(">i", struct.pack(">I", v))[0] for v in raw[4:7]],
        "cycle": list(raw[7:15]),
        "tiles": raw[15],
        "tiles_past_511": raw[16],
        "frames": raw[17],
    }


def _expected_rgb(tile):
    """The colour of `tile`: the demo's index ramp through its palette (RGB555 -> 8 bits per channel)."""
    index = 1 + (tile * 253) // (TILES - 1)
    if index == 1:
        return (0, 0, 0)
    if index == 254:
        return (248, 248, 248)
    step = (index >> 3) & 31
    return (step << 3, (31 - step) << 3, 16 << 3)


def _close(a, b, tolerance=10):
    return all(abs(x - y) <= tolerance for x, y in zip(a, b))


class Vdp2TileOverflowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.r = _load()
        if cls.r["magic"] != MAGIC:
            raise AssertionError("g_overflow_demo not initialised: %r" % cls.r)
        shots = os.environ.get("LIBSATURN_OVERFLOW_SHOTS", "")
        from PIL import Image
        cls.image = Image.open(os.path.join(shots, "last.png")).convert("RGB")

    def test_every_layer_was_accepted(self):
        self.assertEqual(self.r["configure"], [0, 0, 0])
        self.assertEqual(self.r["enable"], [0, 0, 0])

    def test_three_256_colour_layers_fit_one_cycle(self):
        # each needs a name read and two character reads: the library placed all three
        self.assertNotEqual(self.r["cycle"], [0xEEEE] * 8)
        self.assertEqual(self.r["tiles"], TILES)
        self.assertEqual(self.r["tiles_past_511"], TILES - OV_FIRST)

    def test_demo_ran(self):
        self.assertGreater(self.r["frames"], 10)

    def test_no_tile_wrapped(self):
        """Every tile's colour at its cell centre, including all 449 tiles past 511."""
        width = self.image.size[0]
        # the screenshot is the whole 320x224 picture (or a 2x scale of it)
        scale = width // 320
        self.assertIn(scale, (1, 2), "unexpected screenshot size %r" % (self.image.size,))
        wrong = []
        for tile in range(TILES):
            x, y = tile % MAP_W, tile // MAP_W
            if y >= 24:
                continue
            px = (x * 8 + 4 + SHIFT_X) * scale
            py = (y * 8 + 4 + SHIFT_Y) * scale
            if px >= 320 * scale:
                continue
            got = self.image.getpixel((px, py))
            # the VDP1 text lines at the bottom cover a few cells of the last two rows only
            if y * 8 + 8 + SHIFT_Y > 200 and x * 8 < 180:
                continue
            if not _close(got, _expected_rgb(tile)):
                wrong.append((tile, got, _expected_rgb(tile)))
        self.assertEqual(wrong, [], "tiles drawn in the wrong colour: %r" % wrong[:8])

    def test_tiles_past_511_are_drawn_by_the_second_layer(self):
        """Tile 511 (the first of NBG1) and tile 959 (the last) are the ramp's high end, not tile 0-448's."""
        scale = self.image.size[0] // 320
        for tile in (OV_FIRST, OV_FIRST + 1, TILES - 1):
            x, y = tile % MAP_W, tile // MAP_W
            got = self.image.getpixel(((x * 8 + 4 + SHIFT_X) * scale, (y * 8 + 4 + SHIFT_Y) * scale))
            self.assertTrue(_close(got, _expected_rgb(tile)), "tile %d: %r" % (tile, got))
            self.assertFalse(_close(got, _expected_rgb(tile - OV_FIRST), 4),
                             "tile %d looks like tile %d" % (tile, tile - OV_FIRST))


class Vdp2TileOverflowScrollTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        _load()
        from PIL import Image
        cls.image = Image.open(os.path.join(os.environ["LIBSATURN_OVERFLOW_SHOTS"], "last.png")).convert("RGB")
        cls.scale = cls.image.size[0] // 320

    def pixel(self, x, y):
        return self.image.getpixel((x * self.scale, y * self.scale))

    def test_nbg0_first_line_is_the_line_it_names(self):
        """Tile 4 (the first green one, NBG0) starts 4 lines down and 3 dots right of its cell.
        Tiles 0-3 are black like the backdrop, so the first row is tested on tile 4."""
        green = _expected_rgb(4)
        x0, y0 = 4 * 8 + SHIFT_X, SHIFT_Y
        self.assertTrue(_close(self.pixel(x0 + 4, y0), green), self.pixel(x0 + 4, y0))
        self.assertFalse(_close(self.pixel(x0 + 4, y0 - 1), green), self.pixel(x0 + 4, y0 - 1))
        self.assertTrue(_close(self.pixel(x0, y0 + 4), green), self.pixel(x0, y0 + 4))
        self.assertFalse(_close(self.pixel(x0 - 1, y0 + 4), green), self.pixel(x0 - 1, y0 + 4))

    def test_nbg1_first_line_is_the_line_it_names(self):
        """The last tile (white, NBG1) ends where the map does: 4 lines down from tile row 23's top."""
        last = TILES - 1
        x0, y0 = (last % MAP_W) * 8 + SHIFT_X, (last // MAP_W) * 8 + SHIFT_Y
        self.assertEqual(self.pixel(x0 + 4, y0), (248, 248, 248))
        self.assertNotEqual(self.pixel(x0 + 4, y0 - 1), (248, 248, 248))
        self.assertEqual(self.pixel(x0 + 4, y0 + 7), (248, 248, 248))

    def test_nbg2_scroll_lands_on_the_line_it_names(self):
        """NBG2's 8-line checker cells start 4 lines down: the same line as NBG0 and NBG1."""
        column = [self.pixel(300, y) for y in range(200, 224)]
        edges = [200 + i for i in range(1, len(column)) if column[i] != column[i - 1]]
        self.assertEqual(edges, [204, 212, 220], "cell edges at %r" % edges)
        top = [self.pixel(1, y) for y in range(0, 12)]
        # the first 4 lines are the cell row above (scroll -4), the next 8 the next one
        self.assertEqual(len(set(top[0:4])), 1)
        self.assertNotEqual(top[3], top[4])


if __name__ == "__main__":
    unittest.main()
