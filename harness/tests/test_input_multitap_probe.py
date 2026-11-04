"""Ymir synthetic INTBACK report with two six-device adapters.

The test injects report bytes, not an electrical model of the 6Player.
"""
import os
import unittest

from harness.tests.test_input_devices import _load


class SyntheticMultitapTests(unittest.TestCase):
    def test_three_chunk_report(self):
        if not os.environ.get("LIBSATURN_INPUT_TAP_DUMP"):
            self.skipTest("synthetic multitap capture not supplied")
        result = _load("LIBSATURN_INPUT_TAP_DUMP")
        self.assertEqual(result["magic"], 0x49444531)
        self.assertGreater(result["polls"], 100)
        self.assertEqual(result["poll_errors"], 0)
        for index, port in enumerate(result["ports"]):
            self.assertEqual((port["multitap_id"], port["tap_count"]), (1, 6))
            self.assertEqual((port["kind"], port["x"], port["y"]), (2, 0x40 + 6 * index, 0x80))
        self.assertEqual([result[f"tap_kind{i}"] for i in range(6)], [2] * 6)


if __name__ == "__main__":
    unittest.main()
