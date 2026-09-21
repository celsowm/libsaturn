"""Two-process persistence assertions for examples/save_backup_demo.

Normally run by:

    .\harness\run-save-persistence.ps1 -Bios .\bios\saturn_bios_us.bin

The wrapper deliberately reuses the same 32 KiB Ymir internal Backup RAM image
across two independent probe processes.
"""

import json
import os
import unittest


class SaveBackupPersistenceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        first_path = os.environ.get("LIBSATURN_SAVE_FIRST_JSON", "")
        second_path = os.environ.get("LIBSATURN_SAVE_SECOND_JSON", "")
        if not first_path or not second_path:
            raise unittest.SkipTest("save persistence JSON paths are not set")
        if not os.path.exists(first_path) or not os.path.exists(second_path):
            raise unittest.SkipTest("save persistence JSON files are missing")

        with open(first_path, "r", encoding="utf-8") as handle:
            cls.first = json.load(handle)
        with open(second_path, "r", encoding="utf-8") as handle:
            cls.second = json.load(handle)

    @staticmethod
    def _assert_guest_ran(probe):
        iso = probe.get("iso_path", "").replace("\\", "/")
        if not iso.endswith("/save_backup_demo.iso"):
            raise AssertionError(f"probe JSON is for {iso!r}, not save_backup_demo")
        boot = probe.get("boot", {})
        load = boot.get("load_addr", 0)
        pc = boot.get("pc_after_run", 0)
        size = boot.get("bin_bytes", 0)
        if not boot.get("injected") or not (load <= pc < load + size):
            raise AssertionError("save_backup_demo did not execute inside the injected program")

    @staticmethod
    def _demo_file(probe):
        bup = probe["backup_memory"]
        for entry in bup.get("files", []):
            if entry.get("filename") == "LIBSAT_DEMO":
                return entry
        raise AssertionError("LIBSAT_DEMO was not present in internal Backup RAM")

    @staticmethod
    def _payload(entry):
        data = bytes(entry.get("data_prefix", []))
        # sat_save_schema_header_t (16 bytes) prefixes the two-word demo
        # payload. Saturn stores the multi-byte fields in big-endian order.
        if len(data) < 24:
            raise AssertionError(f"demo payload prefix too short: {len(data)}")
        return {
            "magic": data[0:4],
            "version": int.from_bytes(data[4:6], "big"),
            "header_size": int.from_bytes(data[6:8], "big"),
            "payload_size": int.from_bytes(data[8:12], "big"),
            "checksum": int.from_bytes(data[12:16], "big"),
            "boot_count": int.from_bytes(data[16:20], "big"),
            "checksum_seed": int.from_bytes(data[20:24], "big"),
        }

    def test_guest_ran_both_times(self):
        self._assert_guest_ran(self.first)
        self._assert_guest_ran(self.second)

    def test_same_persistent_internal_backup_image(self):
        first = self.first["backup_memory"]
        second = self.second["backup_memory"]
        for bup in (first, second):
            self.assertTrue(bup["enabled"])
            self.assertTrue(bup["header_valid"])
            self.assertEqual(bup["size"], 32768)
        self.assertEqual(first["path"], second["path"])

    def test_save_survives_process_restart_and_increments(self):
        first_entry = self._demo_file(self.first)
        second_entry = self._demo_file(self.second)
        first = self._payload(first_entry)
        second = self._payload(second_entry)

        self.assertEqual(first["magic"], b"LSAV")
        self.assertEqual(second["magic"], b"LSAV")
        self.assertEqual(first["version"], 1)
        self.assertEqual(second["version"], 1)
        self.assertEqual(first["header_size"], 16)
        self.assertEqual(second["header_size"], 16)
        self.assertEqual(first["payload_size"], 8)
        self.assertEqual(second["payload_size"], 8)
        self.assertEqual(first["checksum_seed"], 0x13579BDF)
        self.assertEqual(second["checksum_seed"], 0x13579BDF)

        self.assertEqual(first["boot_count"], 1)
        self.assertEqual(second["boot_count"], 2)
        self.assertNotEqual(first_entry["data_hash"], second_entry["data_hash"])


if __name__ == "__main__":
    unittest.main()
