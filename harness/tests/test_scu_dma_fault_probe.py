"""Check masked and public-handler DMA-illegal injection runs on Ymir.

The probe asserts the SCU source between frames; it does not perform an
invalid physical DMA. Supply both WRAM dumps, their probe JSON and the map.
"""
import json
import os
import unittest

from harness.scripts import telemetry


class DmaIllegalTests(unittest.TestCase):
    def test_masked_and_public_handler(self):
        map_path = os.environ.get("LIBSATURN_DMA_FAULT_MAP")
        if not map_path or not os.environ.get("LIBSATURN_DMA_FAULT_MASKED_DUMP"):
            self.skipTest("fault probe captures not supplied")
        for scenario, calls in (("MASKED", 0), ("HANDLER", 1)):
            with self.subTest(scenario=scenario):
                dump = os.environ["LIBSATURN_DMA_FAULT_" + scenario + "_DUMP"]
                report = os.environ["LIBSATURN_DMA_FAULT_" + scenario + "_JSON"]
                magic, frames, irq, handler, illegal, failed, success = telemetry.load_dump(
                    dump, map_path, "_g_dma_fault_probe", ">7I", 28)
                self.assertEqual(magic, 0x444D4146)
                self.assertGreater(frames, 200)
                self.assertEqual((irq, handler, illegal, failed), (1, calls, 1, 0))
                self.assertGreater(success, 200)
                with open(report, encoding="utf-8") as f:
                    probe = json.load(f)
                self.assertTrue(probe["dma_fault_injection"]["injected"])
                writes = probe["watch_writes"]
                self.assertEqual(writes["master"]["count"], 0)
                self.assertEqual(writes["slave"]["count"], 0)

    def test_interrupt_after_start_before_wait(self):
        map_path = os.environ.get("LIBSATURN_DMA_FAULT_MAP")
        if not map_path or not os.environ.get("LIBSATURN_DMA_FAULT_MID_MASKED_DUMP"):
            self.skipTest("pending-transfer captures not supplied")
        for scenario, calls in (("MID_MASKED", 0), ("MID_HANDLER", 1)):
            with self.subTest(scenario=scenario):
                dump = os.environ["LIBSATURN_DMA_FAULT_" + scenario + "_DUMP"]
                report = os.environ["LIBSATURN_DMA_FAULT_" + scenario + "_JSON"]
                magic, frames, irq, handler, illegal, failed, success = telemetry.load_dump(
                    dump, map_path, "_g_dma_fault_probe", ">7I", 28)
                self.assertEqual(magic, 0x444D4146)
                self.assertGreater(frames, 200)
                self.assertEqual((irq, handler, illegal, failed), (1, calls, 1, 1))
                self.assertGreater(success, 200)
                with open(report, encoding="utf-8") as f:
                    probe = json.load(f)
                self.assertTrue(probe["dma_fault_injection"]["injected"])
                writes = probe["watch_writes"]
                self.assertEqual(writes["master"]["count"], 0)
                self.assertEqual(writes["slave"]["count"], 0)

    def test_emulated_illegal_transfer(self):
        map_path = os.environ.get("LIBSATURN_DMA_FAULT_MAP")
        if not map_path or not os.environ.get("LIBSATURN_DMA_FAULT_REAL_MASKED_DUMP"):
            self.skipTest("invalid-source captures not supplied")
        for scenario, calls in (("REAL_MASKED", 0), ("REAL_HANDLER", 1)):
            with self.subTest(scenario=scenario):
                dump = os.environ["LIBSATURN_DMA_FAULT_" + scenario + "_DUMP"]
                report = os.environ["LIBSATURN_DMA_FAULT_" + scenario + "_JSON"]
                magic, frames, irq, handler, illegal, failed, success = telemetry.load_dump(
                    dump, map_path, "_g_dma_fault_probe", ">7I", 28)
                self.assertEqual(magic, 0x444D4146)
                self.assertGreater(frames, 200)
                self.assertEqual((irq, handler, illegal, failed), (1, calls, 1, 1))
                self.assertGreater(success, 200)
                with open(report, encoding="utf-8") as f:
                    probe = json.load(f)
                self.assertTrue(probe["dma_fault_injection"]["injected"])
                self.assertEqual(probe["watch_writes"]["master"]["count"], 0)
                self.assertEqual(probe["watch_writes"]["slave"]["count"], 0)

    def test_masked_cpu_blocks_start_then_recovers(self):
        map_path = os.environ.get("LIBSATURN_DMA_FAULT_MAP")
        dump = os.environ.get("LIBSATURN_DMA_FAULT_CRITICAL_DUMP")
        report = os.environ.get("LIBSATURN_DMA_FAULT_CRITICAL_JSON")
        if not map_path or not dump or not report:
            self.skipTest("masked-CPU capture not supplied")
        values = telemetry.load_dump(dump, map_path, "_g_dma_fault_probe", ">7I2iI", 40)
        magic, frames, irq, handler, illegal, failed, success, blocked, retry, attempted = values
        self.assertEqual(magic, 0x444D4146)
        self.assertGreater(frames, 200)
        self.assertEqual((irq, handler, illegal, failed), (1, 0, 1, 0))
        self.assertGreater(success, 200)
        self.assertEqual((blocked, retry, attempted), (-5, 0, 1))
        with open(report, encoding="utf-8") as f:
            probe = json.load(f)
        self.assertTrue(probe["dma_fault_injection"]["injected"])
        self.assertEqual(probe["watch_writes"]["master"]["count"], 0)
        self.assertEqual(probe["watch_writes"]["slave"]["count"], 0)


if __name__ == "__main__":
    unittest.main()
