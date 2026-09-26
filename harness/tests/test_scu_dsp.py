r"""Acceptance for the SCU DSP (examples/scu_dsp_demo) on Ymir. Run through the
wrapper:

    .\harness\run-scu-dsp.ps1 -Bios .\bios\saturn_bios_us.bin

The guest transforms 420 vectors on the DSP (data moved by the CPU and by the
DSP itself) and on the SH-2, compares the results word for word, overlaps SH-2
work with a DSP run and probes DSP DMA address additions. The struct read is
g_dsp_demo.
"""

import os
import re
import struct
import unittest

MAGIC = 0x44535031  # "DSP1"
WRAM_HIGH = 0x06000000
FIELDS = ("magic", "matrix_status", "transform_status", "mismatches", "dsp_ticks", "sh2_ticks",
          "overlap_status", "overlap_iterations", "overlap_ticks", "overlap_mismatches", "ended_seen",
          "wait_status", "raw_value", "dma_status", "dma_mismatches", "dma_ticks", "dma_overlap_status",
          "dma_overlap_iterations", "dma_overlap_mismatches", "probe_correct0", "probe_correct1",
          "probe_correct2", "probe_correct3", "probe_status0", "probe_status1", "probe_status2",
          "probe_status3", "frames")


def _load():
    dump = os.environ.get("LIBSATURN_DSP_DUMP")
    map_path = os.environ.get("LIBSATURN_DSP_MAP")
    if not dump or not map_path:
        raise unittest.SkipTest("run harness/run-scu-dsp.ps1 first")
    if not os.path.exists(dump) or not os.path.exists(map_path):
        raise AssertionError("missing input: %s / %s" % (dump, map_path))
    with open(map_path, "r", encoding="utf-8", errors="replace") as stream:
        found = re.search(r"(?m)^\s*0x([0-9A-Fa-f]+)\s+_g_dsp_demo\s*$", stream.read())
    if not found:
        raise AssertionError("_g_dsp_demo is absent from " + map_path)
    offset = int(found.group(1), 16) - WRAM_HIGH
    with open(dump, "rb") as stream:
        raw = struct.unpack(">%dI" % len(FIELDS), stream.read()[offset:offset + 4 * len(FIELDS)])
    return dict(zip(FIELDS, raw))


class ScuDspTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.r = _load()
        if cls.r["magic"] != MAGIC:
            raise AssertionError("g_dsp_demo not initialised: %r" % cls.r)

    def test_the_ports_round_trip_a_word(self):
        self.assertEqual(self.r["wait_status"], 0)
        self.assertEqual(self.r["raw_value"], 0x12345678)

    def test_the_dsp_transform_is_bit_exact(self):
        r = self.r
        self.assertEqual((r["matrix_status"], r["transform_status"]), (0, 0))
        self.assertEqual(r["mismatches"], 0)

    def test_a_run_overlaps_sh2_work_and_signals_its_end(self):
        r = self.r
        self.assertEqual((r["overlap_status"], r["overlap_mismatches"]), (0, 0))
        self.assertGreater(r["overlap_iterations"], 1)           # the SH-2 kept going while it ran
        self.assertEqual(r["ended_seen"], 1)                     # ENDI raised the end flag

    def test_the_dsp_moves_its_own_data_bit_exact(self):
        r = self.r
        self.assertEqual((r["dma_status"], r["dma_mismatches"]), (0, 0))
        self.assertEqual((r["dma_overlap_status"], r["dma_overlap_mismatches"]), (0, 0))
        self.assertGreater(r["dma_overlap_iterations"], 1)

    def test_dsp_dma_needs_an_addition_of_two_for_work_ram(self):
        # copying eight longwords: only the addition 2 (four bytes) in both directions is contiguous;
        # the manual default of 1 steps two bytes and overwrites half of the words
        r = self.r
        self.assertEqual([r["probe_status%d" % i] for i in range(4)], [0, 0, 0, 0])
        self.assertEqual(r["probe_correct3"], 8)
        self.assertLess(r["probe_correct0"], 8)
        self.assertLess(r["probe_correct1"], 8)
        self.assertLess(r["probe_correct2"], 8)


if __name__ == "__main__":
    unittest.main()
