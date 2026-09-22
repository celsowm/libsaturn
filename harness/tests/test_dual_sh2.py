"""Assertions for the dual-SH2 Ymir run.

This test is intentionally based on Ymir's separate CPU instruction counters,
not on a rendered screenshot or a Master-side simulation of Slave progress.
Run ``harness/run-harness.ps1 dual_sh2`` with ``-ProfileInstructions`` first.
"""

import csv
import json
import os
import unittest


def _path(env_name, filename):
    value = os.environ.get(env_name)
    if value:
        return value
    here = os.path.dirname(os.path.abspath(__file__))
    return os.path.join(here, "..", "build", filename)


class DualSh2ProbeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.json_path = _path("LIBSATURN_DUAL_SH2_JSON", "dual_sh2_probe.json")
        cls.csv_path = _path("LIBSATURN_DUAL_SH2_INSTRUCTIONS", "dual_sh2_instructions.csv")
        if not os.path.exists(cls.json_path) or not os.path.exists(cls.csv_path):
            raise unittest.SkipTest(
                "dual-SH2 probe artifacts not found; run "
                "harness/run-harness.ps1 dual_sh2 -ProfileInstructions ..."
            )

        with open(cls.json_path, "r", encoding="utf-8") as stream:
            cls.probe = json.load(stream)
        with open(cls.csv_path, "r", encoding="utf-8", newline="") as stream:
            cls.samples = list(csv.DictReader(stream))

    def test_direct_injection_reached_the_application(self):
        boot = self.probe["boot"]
        self.assertTrue(boot["injected"])
        self.assertGreaterEqual(boot["pc_after_run"], boot["load_addr"])
        self.assertLess(boot["pc_after_run"], boot["load_addr"] + boot["bin_bytes"])

    def test_master_and_slave_both_execute(self):
        self.assertTrue(self.samples)
        master = [int(row["master_sh2_instructions"]) for row in self.samples]
        slave = [int(row["slave_sh2_instructions"]) for row in self.samples]
        self.assertGreater(sum(master), 0)
        self.assertGreater(sum(slave), 0)
        self.assertTrue(
            any(m > 0 and s > 0 for m, s in zip(master, slave)),
            "no sample contained overlapping Master and Slave execution",
        )


if __name__ == "__main__":
    unittest.main()
