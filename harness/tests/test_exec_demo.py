r"""Acceptance for sat_exec (examples/exec_demo).

Run through the wrapper, which builds the demo, runs Ymir and dumps Work RAM:

    .\harness\run-exec.ps1 -Bios .\bios\saturn_bios_us.bin

The demo execs itself (0.BIN) four times, handing the round number over, with
the Slave and VBlank interrupts running each time. Each run starts with a
zeroed .bss, so ``g_exec_demo`` only reaches round 4 if every exec loaded and
started the image and carried the argument across.
"""

import os
import struct
import unittest

from harness.scripts import telemetry as tel

MAGIC = 0x45584543  # "EXEC"
ROUNDS = 4
FIELDS = (
    "magic round arg_status missing_status exec_status slave_status slave_beats frames done"
).split()
SIGNED = {"arg_status", "missing_status", "exec_status", "slave_status"}
SAT_OK = 0
SAT_ERR_NOT_FOUND = -6


def _load():
    dump = os.environ.get("LIBSATURN_EXEC_DUMP")
    map_path = os.environ.get("LIBSATURN_EXEC_MAP")
    if not dump or not map_path:
        raise unittest.SkipTest("run harness/run-exec.ps1 first")
    raw = tel.load_dump(
        dump, map_path, "_g_exec_demo",
        ">%dI" % len(FIELDS), 4 * len(FIELDS))
    values = dict(zip(FIELDS, raw))
    for name in SIGNED:
        values[name] = struct.unpack(">i", struct.pack(">I", values[name]))[0]
    return values


class ExecDemoTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.r = _load()
        if cls.r["magic"] != MAGIC:
            raise AssertionError("g_exec_demo not initialised: %r" % cls.r)

    def test_every_exec_started_the_next_run(self):
        self.assertEqual(self.r["round"], ROUNDS, self.r)
        self.assertEqual(self.r["done"], 1, self.r)

    def test_the_argument_crossed_over(self):
        self.assertEqual(self.r["arg_status"], SAT_OK, self.r)

    def test_no_exec_came_back(self):
        self.assertEqual(self.r["exec_status"], SAT_OK, self.r)

    def test_a_missing_file_returns_without_stopping_anything(self):
        self.assertEqual(self.r["missing_status"], SAT_ERR_NOT_FOUND, self.r)
        self.assertGreater(self.r["frames"], 40, "the last run stopped drawing")

    def test_the_slave_starts_again_after_an_exec(self):
        self.assertEqual(self.r["slave_status"], SAT_OK, self.r)
        self.assertGreater(self.r["slave_beats"], 0, self.r)


if __name__ == "__main__":
    unittest.main()
