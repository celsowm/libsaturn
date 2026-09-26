r"""Two-process persistence assertions for examples/save_cartridge_demo.

Run through the wrapper:

    .\harness\run-save-cartridge.ps1 -Bios .\bios\saturn_bios_us.bin

The wrapper reuses one throwaway 4 Mbit backup cartridge image and one internal
Backup RAM image across two independent probe processes. The guest writes only
to the cartridge, through the BIOS Backup Library: the record must survive the
restart and count up, while the internal image stays byte-identical.
"""

import json
import os
import re
import struct
import unittest

MAGIC = 0x43525431  # "CRT1"
WRAM_HIGH = 0x06000000
FIELDS = ("magic", "init_status", "status", "cart_connected", "cart_partitions", "total_size",
          "block_size", "free_size", "boot_count", "entries", "frames")


def _guest(dump, map_path):
    with open(map_path, "r", encoding="utf-8", errors="replace") as stream:
        found = re.search(r"(?m)^\s*0x([0-9A-Fa-f]+)\s+_g_cart_demo\s*$", stream.read())
    if not found:
        raise AssertionError("_g_cart_demo is absent from " + map_path)
    offset = int(found.group(1), 16) - WRAM_HIGH
    with open(dump, "rb") as stream:
        raw = struct.unpack(">%dI" % len(FIELDS), stream.read()[offset:offset + 4 * len(FIELDS)])
    return dict(zip(FIELDS, raw))


class SaveCartridgeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        env = os.environ
        paths = [env.get(k, "") for k in ("LIBSATURN_CART_FIRST_JSON", "LIBSATURN_CART_SECOND_JSON",
                                          "LIBSATURN_CART_FIRST_DUMP", "LIBSATURN_CART_SECOND_DUMP",
                                          "LIBSATURN_CART_MAP")]
        if not all(paths):
            raise unittest.SkipTest("run harness/run-save-cartridge.ps1 first")
        for path in paths:
            if not os.path.exists(path):
                raise AssertionError("missing input: " + path)
        with open(paths[0], "r", encoding="utf-8") as handle:
            cls.first = json.load(handle)
        with open(paths[1], "r", encoding="utf-8") as handle:
            cls.second = json.load(handle)
        cls.g1 = _guest(paths[2], paths[4])
        cls.g2 = _guest(paths[3], paths[4])

    @staticmethod
    def _cart_file(probe):
        for entry in probe["backup_cartridge"].get("files", []):
            if entry.get("filename") == "LIBSAT_CART":
                return entry
        raise AssertionError("LIBSAT_CART was not on the cartridge")

    @staticmethod
    def _boot_count(entry):
        data = bytes(entry["data_prefix"])
        if len(data) < 24:
            raise AssertionError("payload prefix too short")
        assert data[0:4] == b"LCRT"
        return int.from_bytes(data[16:20], "big")

    def test_guest_ran_and_the_bios_accepted_the_cartridge(self):
        for g in (self.g1, self.g2):
            self.assertEqual(g["magic"], MAGIC)
            self.assertEqual(g["init_status"], 0)
            self.assertEqual(g["status"], 0, "write/verify/read on the cartridge failed: %r" % g)
            self.assertEqual(g["cart_connected"], 1)
            self.assertEqual(g["cart_partitions"], 1)
            self.assertEqual(g["entries"], 1)   # BUP_Dir lists the record (an empty name lists all)
            self.assertGreater(g["frames"], 30)

    def test_the_bios_reports_the_cartridge_geometry(self):
        for g, probe in ((self.g1, self.first), (self.g2, self.second)):
            image = probe["backup_cartridge"]
            # the BIOS' own numbers must agree with the emulated 4 Mbit cartridge
            self.assertEqual(image["size"], 524288)
            self.assertEqual(g["block_size"], image["block_size"])
            self.assertGreater(g["total_size"], 400000)
            self.assertLessEqual(g["total_size"], 524288)
            self.assertGreater(g["free_size"], 300000)

    def test_the_record_survives_a_fresh_process_and_counts_up(self):
        a, b = self._cart_file(self.first), self._cart_file(self.second)
        self.assertEqual(self._boot_count(a), 1)
        self.assertEqual(self._boot_count(b), 2)
        self.assertNotEqual(a["data_hash"], b["data_hash"])
        self.assertEqual((self.g1["boot_count"], self.g2["boot_count"]), (1, 2))
        self.assertTrue(self.first["backup_cartridge"]["guest_changed_cart_memory"])
        # the second process started from the image the first one left
        self.assertEqual(self.first["backup_cartridge"]["raw_hash_after"],
                         self.second["backup_cartridge"]["raw_hash_before"])

    def test_the_internal_backup_ram_is_untouched(self):
        for probe in (self.first, self.second):
            self.assertEqual(probe["backup_memory"]["files"], [])
        self.assertEqual(self.first["backup_memory"]["raw_hash"], self.second["backup_memory"]["raw_hash"])


if __name__ == "__main__":
    unittest.main()
