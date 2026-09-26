r"""Acceptance for SCU DMA (examples/dma_demo).

Run through the wrapper, which builds the demo, runs Ymir and dumps Work RAM:

    .\harness\run-dma-demo.ps1 -Bios .\bios\saturn_bios_us.bin

The demo reads every destination back and compares it with the CPU path, so
these are pass/fail flags plus the timing it measured.
"""

import os
import re
import struct
import unittest

MAGIC = 0x444D4131  # "DMA1"
FIELDS = (
    "magic done irq_active direct_vdp1 direct_vdp2 direct_scsp matches_cpu_path "
    "readback indirect indirect_end_irq cache_coherent illegal_rejected "
    "ram_copy_on_cpu ram_copy_on_sh2 sh2_refuses_misfits low_ram_on_sh2 sh2_cache_coherent sh2_overlap_down "
    "small_copy_on_cpu cpu_ms dma_ms ram_cpu_ms ram_sh2_ms frame_cpu_ms frame_dma_ms scu_transfers timeouts illegal"
).split()
WRAM_HIGH = 0x06000000


def _load():
    dump = os.environ.get("LIBSATURN_DMA_DUMP")
    map_path = os.environ.get("LIBSATURN_DMA_MAP")
    if not dump or not map_path or not os.path.exists(dump) or not os.path.exists(map_path):
        raise unittest.SkipTest("run harness/run-dma-demo.ps1 first")
    with open(map_path, "r", encoding="utf-8", errors="replace") as stream:
        found = re.search(r"(?m)^\s*0x([0-9A-Fa-f]+)\s+_g_dma_demo\s*$", stream.read())
    if not found:
        raise AssertionError("_g_dma_demo is absent from " + map_path)
    offset = int(found.group(1), 16) - WRAM_HIGH
    with open(dump, "rb") as stream:
        data = stream.read()[offset:offset + 4 * len(FIELDS)]
    return dict(zip(FIELDS, struct.unpack(">%dI" % len(FIELDS), data)))


class DmaDemoTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.r = _load()
        if cls.r["magic"] != MAGIC:
            raise AssertionError("g_dma_demo not initialised: %r" % cls.r)
        if cls.r["done"] != 1:
            raise AssertionError("the demo did not finish its checks: %r" % cls.r)

    def test_direct_uploads_reach_every_b_bus_target(self):
        for name in ("direct_vdp1", "direct_vdp2", "direct_scsp"):
            self.assertEqual(self.r[name], 1, name)

    def test_dma_leaves_the_bytes_the_cpu_path_leaves(self):
        self.assertEqual(self.r["matches_cpu_path"], 1)

    def test_b_bus_to_work_ram_reads_back(self):
        self.assertEqual(self.r["readback"], 1)

    def test_indirect_list_transfers_every_entry(self):
        self.assertEqual(self.r["indirect"], 1)

    def test_dma_end_interrupt_fires_once_per_list(self):
        if self.r["irq_active"] != 1:
            self.skipTest("SCU interrupts inactive")
        self.assertEqual(self.r["indirect_end_irq"], 1)

    def test_work_ram_destination_is_not_stale_in_the_cache(self):
        self.assertEqual(self.r["cache_coherent"], 1)

    def test_illegal_scu_routes_are_refused_and_small_copies_use_the_cpu(self):
        self.assertEqual(self.r["illegal_rejected"], 1)
        self.assertEqual(self.r["small_copy_on_cpu"], 1)

    def test_work_ram_copies_stay_on_the_cpu_unless_the_sh2_dmac_is_asked_for(self):
        self.assertEqual(self.r["ram_copy_on_cpu"], 1)
        self.assertEqual(self.r["ram_copy_on_sh2"], 1)
        self.assertEqual(self.r["sh2_refuses_misfits"], 1)
        self.assertEqual(self.r["low_ram_on_sh2"], 1)  # Work RAM Low, unreachable by the SCU
        self.assertEqual(self.r["sh2_overlap_down"], 1)

    def test_sh2_dmac_destination_is_not_stale_in_the_cache(self):
        self.assertEqual(self.r["sh2_cache_coherent"], 1)

    def test_no_dma_failed(self):
        self.assertEqual(self.r["timeouts"], 0)
        self.assertEqual(self.r["illegal"], 0)
        self.assertGreater(self.r["scu_transfers"], 10)

    def test_dma_is_not_slower_than_the_cpu(self):
        # 16 rounds of 32 KiB into VDP1 VRAM; both numbers are reported.
        print("cpu_ms=%d dma_ms=%d" % (self.r["cpu_ms"], self.r["dma_ms"]))
        self.assertLessEqual(self.r["dma_ms"], self.r["cpu_ms"])

    def test_command_table_copy_timing_is_reported(self):
        # 64 x 400 VDP1 commands: the per-frame copy submit() used to do by CPU.
        print("frame_cpu_ms=%d frame_dma_ms=%d" % (self.r["frame_cpu_ms"], self.r["frame_dma_ms"]))
        self.assertLessEqual(self.r["frame_dma_ms"], self.r["frame_cpu_ms"])

    def test_sh2_dmac_timing_is_reported(self):
        # Ymir undercharges DMA, so it reads faster there; Mednafen measures the
        # DMAC slower than the CPU loop, which is why sat_dma_copy never picks it.
        print("ram_cpu_ms=%d ram_sh2_ms=%d" % (self.r["ram_cpu_ms"], self.r["ram_sh2_ms"]))


if __name__ == "__main__":
    unittest.main()
