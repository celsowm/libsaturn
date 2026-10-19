r"""Acceptance for the transactional VDP2 resource plan
(examples/vdp2_vram_stress).

Run through the wrapper, which builds the example, runs Ymir and dumps
Work RAM and screenshots:

    .\harness\run-vdp2-vram-stress.ps1 -Bios .\bios\saturn_bios_us.bin

The struct read here is g_vram_stress. Seven scenarios run in a fixed frame
timeline; the assertions check that every documented success and refusal
path behaved as the plan API promises (including the CRAM transaction), that
the refusals never changed the display registers (the cycle patterns and the
on-screen bands), and that the live reclaim-and-reconfigure actually took
effect.
"""

import os
import struct
import unittest

from harness.scripts import telemetry as tel

MAGIC = 0x56525331  # "VRS1"
SAT_OK = 0
SAT_ERR_CAPACITY = -3
SAT_ERR_BUSY = -7

# 64 big-endian uint32 words.
FIELDS = ">64I"


def _load():
    dump = os.environ.get("LIBSATURN_VRS_DUMP")
    map_path = os.environ.get("LIBSATURN_VRS_MAP")
    if not dump or not map_path:
        raise unittest.SkipTest("run harness/run-vdp2-vram-stress.ps1 first")
    raw = tel.load_dump(dump, map_path, "_g_vram_stress", FIELDS, 256)
    return {
        "magic": raw[0],
        "scenario_ran": raw[1],
        "planned_layout_ok": raw[2],
        "layers_configured": _signed(raw[3:7]),
        "names_bases": list(raw[7:11]),
        "free_before_rollback": raw[11],
        "free_after_rollback": raw[12],
        "oversized_refused": _signed([raw[13]])[0],
        "names_in_claimed_bank": _signed([raw[14]])[0],
        "table_in_claimed_bank": _signed([raw[15]])[0],
        "claim_over_live": _signed([raw[16]])[0],
        "banks_released": _signed([raw[17]])[0],
        "fragmentation_detected": raw[18],
        "largest_hole": raw[19],
        "total_free": raw[20],
        "reconfigured_ok": _signed([raw[21]])[0],
        "names_bases_b": list(raw[22:26]),
        "cycle_after_first": list(raw[26:34]),
        "cycle_mid": list(raw[34:42]),
        "cycle_at_end": list(raw[42:50]),
        "refusals": raw[50],
        "last_status": _signed([raw[51]])[0],
        "free_bytes_per_bank": list(raw[52:56]),
        "frames": raw[56],
        "cram_first_bank": raw[57],
        "cram_first_code": raw[58],
        "cram_rollback_reused": raw[59],
        "cram_capacity_refused": raw[60],
        "cram_commit_ok": raw[61],
        "cram_release_ok": raw[62],
        "cram_used_after_release": raw[63],
    }


def _signed(values):
    return [struct.unpack(">i", struct.pack(">I", v))[0] for v in values]


def _region(path, y0, y1):
    """The pixels of rows [y0, y1) as a flat tuple, for exact comparison."""
    from PIL import Image
    image = Image.open(path).convert("RGB")
    rows = []
    for y in range(y0, min(y1, image.height)):
        rows.extend(image.crop((0, y, image.width, y + 1)).getdata())
    return rows


class Vdp2VramStressTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.r = _load()
        if cls.r["magic"] != MAGIC:
            raise AssertionError("g_vram_stress not initialised: %r" % cls.r)
        cls.shots = os.environ.get("LIBSATURN_VRS_SHOTS", "")

    def test_every_scenario_ran(self):
        self.assertEqual(self.r["scenario_ran"], 0x7F)
        self.assertGreaterEqual(self.r["frames"], 255)

    def test_legal_layout_was_planned_and_configured(self):
        self.assertEqual(self.r["planned_layout_ok"], SAT_OK)
        self.assertEqual(self.r["layers_configured"], [SAT_OK] * 4)
        # One names table per bank (A0, A1, B0) and NBG3 packed into A0.
        self.assertEqual(self.r["names_bases"],
                         [0x00000, 0x20000, 0x40000, 0x02000])

    def test_oversized_reservation_refused_and_rollback_exact(self):
        self.assertEqual(self.r["oversized_refused"], SAT_ERR_CAPACITY)
        self.assertGreater(self.r["free_before_rollback"], 0)
        self.assertEqual(self.r["free_after_rollback"],
                         self.r["free_before_rollback"])

    def test_bank_claim_routing(self):
        self.assertEqual(self.r["names_in_claimed_bank"], SAT_ERR_BUSY)
        self.assertEqual(self.r["table_in_claimed_bank"], SAT_OK)
        self.assertEqual(self.r["banks_released"], SAT_OK)

    def test_claim_over_live_reservation_refused(self):
        self.assertEqual(self.r["claim_over_live"], SAT_ERR_BUSY)

    def test_fragmentation_reported_not_hidden(self):
        self.assertEqual(self.r["fragmentation_detected"], 1)
        # The request (0x38000) was larger than any single hole but smaller
        # than the total free space: the refusal is honest fragmentation.
        self.assertGreater(self.r["total_free"], 0x38000)
        self.assertLess(self.r["largest_hole"], 0x38000)

    def test_reclaim_and_reconfigure(self):
        self.assertEqual(self.r["reconfigured_ok"], SAT_OK)
        # Layout B: names in two banks, characters after them.
        self.assertEqual(self.r["names_bases_b"],
                         [0x00000, 0x02000, 0x20000, 0x22000])
        # The reconfiguration must have actually changed the fetch plan.
        self.assertNotEqual(self.r["cycle_at_end"], self.r["cycle_after_first"])

    def test_cram_transaction_rolls_back_and_reuses(self):
        self.assertEqual(self.r["cram_first_bank"], 1)
        self.assertEqual(self.r["cram_first_code"], 0)
        self.assertEqual(self.r["cram_rollback_reused"], 1)
        self.assertEqual(self.r["cram_capacity_refused"], 1)
        self.assertEqual(self.r["cram_commit_ok"], 1)
        self.assertEqual(self.r["cram_release_ok"], 1)
        self.assertEqual(self.r["cram_used_after_release"], 0)

    def test_refusals_never_touched_the_display_registers(self):
        self.assertEqual(self.r["cycle_mid"], self.r["cycle_after_first"])

    def test_telemetry_at_the_end(self):
        # Layout B owns A0 and A1 (two names plus two character windows
        # each); B0 and B1 are completely free again after the reclaim.
        self.assertEqual(self.r["free_bytes_per_bank"][0], 0x20000 - 0x4080)
        self.assertEqual(self.r["free_bytes_per_bank"][1], 0x20000 - 0x4080)
        self.assertEqual(self.r["free_bytes_per_bank"][2], 0x20000)
        self.assertEqual(self.r["free_bytes_per_bank"][3], 0x20000)
        self.assertGreaterEqual(self.r["refusals"], 4)

    def test_the_picture_survived_the_refusals_pixel_for_pixel(self):
        """The colour bands must be identical before and after every
        refusal scenario: a refusal must never corrupt the display."""
        if not self.shots:
            self.skipTest("no screenshots captured")
        before = os.path.join(self.shots, "f80.png")
        after = os.path.join(self.shots, "f195.png")
        end = os.path.join(self.shots, "f255.png")
        for path in (before, after, end):
            if not os.path.isfile(path):
                self.skipTest("missing capture %s" % path)
        bands_before = _region(before, 24, 136)
        bands_after = _region(after, 24, 136)
        self.assertEqual(bands_before, bands_after)
        # The reconfiguration redraws the same bands (they do not overlap),
        # but the bank bars under them must reflect the reclaimed layout.
        self.assertNotEqual(_region(before, 196, 205), _region(end, 196, 205))


if __name__ == "__main__":
    unittest.main()
