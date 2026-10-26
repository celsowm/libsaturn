#!/usr/bin/env python3
"""Validate complete, ordered skinning benchmark telemetry."""
import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "harness/scripts"))
from skin_mac_bench import decode


def capture():
    words = [0x534B4D31, 1, 210540, 1, 0, 7544, 1, 2]
    for kernel in range(2):
        words.extend([kernel, 0, 32, *([100 + kernel] * 5), 0x12345678])
    return struct.pack(">26I", *words)


class SkinMacBenchTests(unittest.TestCase):
    def test_full_qualification_and_relative_cost(self):
        result = decode(capture())
        self.assertEqual(result["checks"], 7544)
        self.assertEqual(len(result["samples"]), 2)
        self.assertAlmostEqual(result["mac_l_change_percent"], 1.0)

    def test_rejects_incomplete_or_wrong_case_data(self):
        with self.assertRaises(ValueError):
            decode(capture()[:-4])
        for word, value in ((0, 0), (3, 0), (4, 1), (5, 7543), (7, 1)):
            raw = bytearray(capture())
            struct.pack_into(">I", raw, word * 4, value)
            with self.subTest(word=word), self.assertRaises(ValueError):
                decode(raw)
        raw = bytearray(capture())
        struct.pack_into(">I", raw, 32, 1)
        with self.assertRaises(ValueError):
            decode(raw)

    def test_rejects_bad_clock_or_different_checksums(self):
        raw = bytearray(capture())
        struct.pack_into(">I", raw, 44, 0)
        with self.assertRaises(ValueError):
            decode(raw)
        raw = bytearray(capture())
        struct.pack_into(">I", raw, 32 + 9 * 4 - 4, 0)
        with self.assertRaises(ValueError):
            decode(raw)


if __name__ == "__main__":
    unittest.main()
