"""Unit tests for harness/scripts/telemetry.py. Pure-Python; no fixture
files. Each test builds a throwaway .map and dump in
``tempfile.gettempdir()`` so it can run in any environment without
Ymir or a real Saturn build.

Run with::

    python -m unittest discover harness/tests -p test_telemetry_helper.py -v
"""

import os
import re
import struct
import tempfile
import unittest

from harness.scripts import telemetry as tel


class FindSymbolTests(unittest.TestCase):
    def test_returns_offset_minus_wram_high(self):
        with tempfile.TemporaryDirectory() as td:
            map_path = os.path.join(td, "out.map")
            with open(map_path, "w", encoding="utf-8") as stream:
                # A small synthetic .map of the shape GNU ld produces:
                # one section header, then one entry "address name".
                # _g_demo at 0x06000024 means the function should
                # return 0x24 (the offset within Work RAM High).
                stream.write("""
                    .text           0x06000000       0x100
 0x06000024                _g_demo
                    .bss            0x06000040       0x20
""")
            self.assertEqual(tel.find_symbol(map_path, "_g_demo"), 0x24)

    def test_missing_map_is_a_failure(self):
        with self.assertRaises(AssertionError):
            tel.find_symbol("/nonexistent/path/to.map", "_g_demo")

    def test_missing_symbol_is_a_failure(self):
        with tempfile.TemporaryDirectory() as td:
            map_path = os.path.join(td, "out.map")
            with open(map_path, "w", encoding="utf-8") as stream:
                stream.write("\n")
            with self.assertRaises(AssertionError):
                tel.find_symbol(map_path, "_g_demo")

    def test_address_below_wram_rejected(self):
        with tempfile.TemporaryDirectory() as td:
            map_path = os.path.join(td, "out.map")
            with open(map_path, "w", encoding="utf-8") as stream:
                stream.write("\n0x00200000                _g_low\n")
            with self.assertRaises(AssertionError):
                tel.find_symbol(map_path, "_g_low")


class LoadDumpTests(unittest.TestCase):
    def _build_fixture(self, contents):
        """Write a self-consistent dump+map and return both paths."""
        td = tempfile.mkdtemp()
        self.addCleanup(lambda: __import__("shutil").rmtree(td, ignore_errors=True))
        map_path = os.path.join(td, "out.map")
        dump_path = os.path.join(td, "wram.bin")
        with open(map_path, "w", encoding="utf-8") as stream:
            stream.write("\n0x06000010                _g_demo\n")
        # WRAM_HIGH = 0x06000000; offset = 0x10. Write contents at that
        # offset and a byte of padding in front to prove offsets are
        # respected.
        with open(dump_path, "wb") as stream:
            stream.write(b"\xAA" * 0x10)
            stream.write(contents)
        return dump_path, map_path

    def test_little_endian_format(self):
        payload = struct.pack("<III", 0x12345678, 0xABCDEF01, 0x00000002)
        dump_path, map_path = self._build_fixture(payload)
        v = tel.load_dump(dump_path, map_path, "_g_demo", "<III")
        self.assertEqual(v, (0x12345678, 0xABCDEF01, 0x00000002))

    def test_big_endian_format(self):
        payload = struct.pack(">III", 0x12345678, 0xABCDEF01, 0x00000002)
        dump_path, map_path = self._build_fixture(payload)
        v = tel.load_dump(dump_path, map_path, "_g_demo", ">III")
        self.assertEqual(v, (0x12345678, 0xABCDEF01, 0x00000002))

    def test_short_dump_raises(self):
        td = tempfile.mkdtemp()
        self.addCleanup(lambda: __import__("shutil").rmtree(td, ignore_errors=True))
        map_path = os.path.join(td, "out.map")
        dump_path = os.path.join(td, "wram.bin")
        with open(map_path, "w", encoding="utf-8") as stream:
            stream.write("\n0x06000000                _g_demo\n")
        with open(dump_path, "wb") as stream:
            stream.write(b"")  # empty
        with self.assertRaises(AssertionError):
            tel.load_dump(dump_path, map_path, "_g_demo", ">I")


class VerifyFieldsTests(unittest.TestCase):
    def test_tuple_index_match_passes(self):
        tel.verify_fields((0, 1, 2), {0: 0, 2: 2})  # no raise

    def test_tuple_mismatch_raises(self):
        with self.assertRaisesRegex(AssertionError, "field 1: want 9, got 1"):
            tel.verify_fields((0, 1, 2), {1: 9})

    def test_dict_match_passes(self):
        tel.verify_fields({"a": 1, "b": 2}, {"a": 1})  # no raise

    def test_extra_values_are_ignored(self):
        # Tuple has more entries than `expected` -- only the listed
        # indices are checked.
        tel.verify_fields((0, 1, 2, 3, 4), {4: 4})  # no raise


if __name__ == "__main__":
    unittest.main()
