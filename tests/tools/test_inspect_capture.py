"""The capture inspector parses colors, spans, registers and WRAM structs."""
import sys
import tempfile
import unittest
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

import inspect_capture as cap


class CaptureInspectorTests(unittest.TestCase):
    def test_color_forms(self):
        self.assertEqual(cap.parse_color("#102030"), (0x10, 0x20, 0x30))
        self.assertEqual(cap.parse_color("10,20,30"), (10, 20, 30))
        self.assertEqual(cap.parse_color("0x102030"), (0x10, 0x20, 0x30))
        with self.assertRaises(Exception):
            cap.parse_color("256,2,3")

    def test_at_and_span(self):
        self.assertEqual(cap.parse_at("10,100"), (10, 100))
        self.assertEqual(cap.parse_at("0x0A,0"), (10, 0))
        self.assertEqual(cap.parse_span("100:0:40:2"), (100, 0, 40, 2))
        with self.assertRaises(Exception):
            cap.parse_span("1:2")

    def test_color_bounds(self):
        image = Image.new("RGB", (8, 6), (0, 0, 0))
        image.putpixel((2, 3), (248, 0, 248))
        image.putpixel((5, 3), (248, 0, 248))
        count, bounds = cap.find_color_bounds(image, (248, 0, 248))
        self.assertEqual(count, 2)
        self.assertEqual(bounds, (2, 3, 5, 3))
        count, bounds = cap.find_color_bounds(image, (249, 1, 249), tolerance=1)
        self.assertEqual(count, 2)
        self.assertEqual(cap.find_color_bounds(image, (1, 2, 3)), (0, None))

    def test_struct_from_map_and_dump(self):
        with tempfile.TemporaryDirectory() as folder:
            directory = Path(folder)
            dump = directory / "wram.bin"
            dump.write_bytes(b"\x00" * 0x100 +
                             (0x11223344).to_bytes(4, "big") +
                             (0x55667788).to_bytes(4, "big"))
            map_file = directory / "example.map"
            map_file.write_text("                0x06000100                _g_test\n",
                                encoding="utf-8")
            address, words = cap.unpack_struct(dump, map_file, "_g_test", 2)
            self.assertEqual(address, 0x06000100)
            self.assertEqual(words, (0x11223344, 0x55667788))
            with self.assertRaises(SystemExit):
                cap.unpack_struct(dump, map_file, "_g_missing", 1)

    def test_register_format(self):
        self.assertEqual(cap.format_register(0xA001), "0xA001 (40961)")
        self.assertEqual(cap.format_register(0x123456789), "0x123456789 (4886718345)")


if __name__ == "__main__":
    unittest.main()
