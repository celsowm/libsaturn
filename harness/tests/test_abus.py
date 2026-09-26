r"""Acceptance for the A-Bus slot API (examples/abus_demo) across four cartridge
configurations on Ymir: nothing, the 1 MiB and 4 MiB RAM expansions, and a Backup
Memory cartridge. Run through the wrapper:

    .\harness\run-abus.ps1 -Bios .\bios\saturn_bios_us.bin
"""

import os
import re
import struct
import unittest

MAGIC = 0x41425531  # "ABU1"
WRAM_HIGH = 0x06000000
FIELDS = ("magic", "detect_status", "kind", "id", "backup_bytes", "read_status", "write_status",
          "write_readback_ok", "write_outside_status", "write_cs1_status", "frames")


def _read(dump, map_path):
    with open(map_path, "r", encoding="utf-8", errors="replace") as stream:
        found = re.search(r"(?m)^\s*0x([0-9A-Fa-f]+)\s+_g_abus_demo\s*$", stream.read())
    if not found:
        raise AssertionError("_g_abus_demo is absent from " + map_path)
    offset = int(found.group(1), 16) - WRAM_HIGH
    with open(dump, "rb") as stream:
        raw = struct.unpack(">%dI" % len(FIELDS), stream.read()[offset:offset + 4 * len(FIELDS)])
    return dict(zip(FIELDS, raw))


def _codes():
    """sat_result_t error codes, as the guest reports them (-result)."""
    header = os.path.join(os.path.dirname(__file__), "..", "..", "include", "saturn", "core.h")
    with open(header, "r", encoding="utf-8") as stream:
        text = stream.read()
    return {name: -int(value) for name, value in re.findall(r"(SAT_ERR_[A-Z_]+)\s*=\s*(-\d+)", text)}


class AbusTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        map_path = os.environ.get("LIBSATURN_ABUS_MAP", "")
        if not map_path:
            raise unittest.SkipTest("run harness/run-abus.ps1 first")
        base = os.environ["LIBSATURN_ABUS_DIR"]
        cls.codes = _codes()
        cls.r = {mode: _read(os.path.join(base, "wram_%s.bin" % mode), map_path)
                 for mode in ("none", "1m", "4m", "backup")}
        for mode, r in cls.r.items():
            if r["magic"] != MAGIC:
                raise AssertionError("%s: g_abus_demo not initialised: %r" % (mode, r))

    def test_the_id_byte_classifies_every_configuration(self):
        expect = {"none": (0, 0xFF, 0), "1m": (1, 0x5A, 0), "4m": (2, 0x5C, 0),
                  "backup": (3, 0x21, 512 * 1024)}
        for mode, (kind, ident, backup) in expect.items():
            r = self.r[mode]
            self.assertEqual((r["kind"], r["id"], r["backup_bytes"]), (kind, ident, backup), mode)
            self.assertEqual(r["detect_status"], 0)
            self.assertGreater(r["frames"], 10)

    def test_an_empty_slot_is_refused_without_touching_anything(self):
        r = self.r["none"]
        for key in ("read_status", "write_status", "write_outside_status", "write_cs1_status"):
            self.assertEqual(r[key], self.codes["SAT_ERR_NOT_CONNECTED"], key)
        self.assertEqual(r["write_readback_ok"], 0)

    def test_ram_expansions_take_a_write_and_read_it_back(self):
        for mode in ("1m", "4m"):
            r = self.r[mode]
            self.assertEqual(r["read_status"], 0, mode)
            self.assertEqual(r["write_status"], 0, mode)
            self.assertEqual(r["write_readback_ok"], 1, mode)
            # writes outside the DRAM banks and to CS1 are refused
            self.assertEqual(r["write_outside_status"], self.codes["SAT_ERR_UNSUPPORTED"], mode)
            self.assertEqual(r["write_cs1_status"], self.codes["SAT_ERR_UNSUPPORTED"], mode)

    def test_a_backup_cartridge_is_never_touched_raw(self):
        r = self.r["backup"]
        for key in ("read_status", "write_status", "write_outside_status", "write_cs1_status"):
            self.assertEqual(r[key], self.codes["SAT_ERR_UNSUPPORTED"], key)
        self.assertEqual(r["write_readback_ok"], 0)


if __name__ == "__main__":
    unittest.main()
