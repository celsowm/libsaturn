"""Parallax Lab converter emits the VDP2 cell, bitmap and 2-word map layout."""
import sys
import tempfile
import unittest
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from convert_vdp2_parallax_lab import (  # noqa: E402
    MOUNTAIN_CHAR_BASE,
    SKY_CHAR_BASE,
    SKY_PALETTE,
    _cell_map,
    _pack_4bpp_cells,
    _pack_8bpp_bitmap,
    _pack_8bpp_cells,
    convert,
)


class PackingTests(unittest.TestCase):
    def test_8bpp_bitmap_is_big_endian_pixel_pairs(self):
        pixels = bytearray(512 * 256)
        pixels[0:4] = bytes((0x11, 0x22, 0x33, 0x44))
        words = _pack_8bpp_bitmap(bytes(pixels))
        self.assertEqual(len(words), 512 * 256 // 2)
        self.assertEqual(words[0], 0x1122)
        self.assertEqual(words[1], 0x3344)

    def test_8bpp_cell_is_row_major_big_endian(self):
        pixels = bytearray(512 * 256)
        pixels[0:2] = bytes((0xAB, 0xCD))
        words = _pack_8bpp_cells(bytes(pixels))
        self.assertEqual(len(words), 2048 * 32)
        self.assertEqual(words[0], 0xABCD)

    def test_4bpp_cell_packs_four_dots_per_word(self):
        pixels = bytearray(512 * 256)
        pixels[0:4] = bytes((0x1, 0x2, 0x3, 0x4))
        words = _pack_4bpp_cells(bytes(pixels))
        self.assertEqual(len(words), 2048 * 16)
        self.assertEqual(words[0], 0x1234)

    def test_sky_map_uses_256_colour_bank_and_char_step_two(self):
        words = _cell_map(SKY_CHAR_BASE, SKY_PALETTE, 2, False)
        self.assertEqual(SKY_PALETTE, 0x20)
        self.assertEqual(words[:4], [0x0020, 0x3000, 0x0020, 0x3002])
        self.assertEqual(words[-2:], [0x0020, 0x3000])

    def test_mountain_map_starts_after_transparent_tile(self):
        words = _cell_map(MOUNTAIN_CHAR_BASE, 4, 1, True)
        self.assertEqual(words[:4], [0x0004, 0x2401, 0x0004, 0x2402])
        self.assertEqual(words[32 * 64 * 2:32 * 64 * 2 + 2], [0, 0])


class ConversionTests(unittest.TestCase):
    def test_full_conversion_emits_all_arrays(self):
        with tempfile.TemporaryDirectory() as folder:
            directory = Path(folder)
            assets = directory / "assets"
            assets.mkdir()
            Image.new("RGBA", (512, 256), (1, 79, 235, 255)).save(assets / "sky.png")
            Image.new("RGBA", (512, 256), (20, 100, 30, 255)).save(assets / "mountains.png")
            Image.new("RGBA", (512, 256), (10, 40, 180, 255)).save(assets / "sea.png")
            Image.new("RGBA", (512, 256), (20, 120, 20, 255)).save(assets / "foreground.png")
            prefix = directory / "out" / "assets"
            report = convert(assets, prefix)
            header = prefix.with_suffix(".h").read_text()
            source = prefix.with_suffix(".c").read_text()
            self.assertEqual(report["foreground_words"], 512 * 256 // 2)
            self.assertEqual(report["sky_char_words"], 2048 * 32)
            self.assertEqual(report["mountain_char_words"], 2048 * 16 + 16)
            for name in ("plab_foreground_words", "plab_sea_words",
                         "plab_sky_chars", "plab_mountain_chars",
                         "plab_sky_map", "plab_mountain_map"):
                self.assertIn(name, header)
                self.assertIn(name, source)


if __name__ == "__main__":
    unittest.main()
