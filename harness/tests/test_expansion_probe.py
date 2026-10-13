r"""Acceptance for the MPEG card probe and the UART open (examples/expansion_probe_demo) on Ymir,
which has neither an MPEG card nor a NetLink. Run through the wrapper:

    .\harness\run-expansion-probe.ps1 -Bios .\bios\saturn_bios_us.bin

Both must report "not there" and change nothing. The CD Block answers (hardware and drive
versions) are the ones the Ymir core gives for Get Hardware Info. What a present card would do is
tested on the host only (tests/host/test_mpeg_api.cpp): it is unverified on any emulator.
"""

import os
import unittest

from harness.scripts import telemetry as tel

MAGIC = 0x45585031  # "EXP1"
FIELDS = ["magic", "cd_init", "probe_status", "present", "mpeg_version", "hardware_flags", "hardware_version",
          "drive_version", "drive_revision", "authentication_status", "start_status", "abus_status", "abus_kind",
          "uart_status", "uart_attempted", "frames"]
SAT_ERR_NOT_CONNECTED = 9      # the results hold -status


def _load():
    base = os.environ.get("LIBSATURN_EXPANSION_DIR")
    map_path = os.environ.get("LIBSATURN_EXPANSION_MAP")
    if not base or not map_path:
        raise unittest.SkipTest("run harness/run-expansion-probe.ps1 first")
    dump = os.path.join(base, "wram.bin")
    values = tel.load_dump(
        dump, map_path, "_g_expansion",
        ">%dI" % len(FIELDS), 4 * len(FIELDS))
    return dict(zip(FIELDS, values))


class ExpansionProbeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.r = _load()
        if cls.r["magic"] != MAGIC:
            raise AssertionError("g_expansion not initialised: %r" % cls.r)

    def test_the_probe_reports_no_mpeg_card(self):
        r = self.r
        self.assertEqual((r["cd_init"], r["probe_status"]), (0, 0))
        self.assertEqual((r["present"], r["mpeg_version"]), (0, 0))

    def test_the_hardware_info_is_the_cd_blocks(self):
        r = self.r
        self.assertEqual((r["hardware_flags"], r["hardware_version"]), (0x00, 0x02))
        self.assertEqual((r["drive_version"], r["drive_revision"]), (0x06, 0x00))

    def test_start_refuses_without_a_card(self):
        self.assertEqual(self.r["start_status"], SAT_ERR_NOT_CONNECTED)

    def test_no_uart_on_the_empty_slot(self):
        r = self.r
        self.assertEqual(r["abus_status"], 0)
        self.assertEqual(r["abus_kind"], 0, "the run has no cartridge")
        self.assertEqual(r["uart_attempted"], 1)
        self.assertEqual(r["uart_status"], SAT_ERR_NOT_CONNECTED)

    def test_the_program_kept_running(self):
        self.assertGreater(self.r["frames"], 30)


if __name__ == "__main__":
    unittest.main()
