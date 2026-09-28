"""city_walk acceptance checks (GPL-3.0, harness/LICENSE).

Do not run these by hand: they need four harness runs whose outputs are kept
apart, and a stale ISO would silently test the wrong program. Use

    .\\harness\\run-city-walk-checks.ps1 -Bios .\\bios\\saturn_bios_us.bin

which rebuilds the example, runs the scenarios below, and fails if any test
was skipped. Each scenario leaves `city_walk_<name>.json` (the probe report),
`city_walk_<name>_wramh.bin` (Work RAM High) and screenshots in harness/build.

  walk    4 MB cart; run across several chunks so the residency rings page
  wall    4 MB cart; jump to a viewpoint facing a building and walk into it
  nocart  no cartridge: a refusal screen, not a crash
  onemeg  1 MB cartridge: refused, not loaded halfway

Assertions are on observable invariants (telemetry the program writes, VDP
registers, the screen), never on tuned constants, apart from the frame-rate
budget the plan sets and the caps the archive format guarantees.
"""
import json
import os
import re
import struct
import unittest
from pathlib import Path

import city_telemetry

HARNESS_BUILD = Path(__file__).resolve().parents[1] / "build"
REPO = Path(__file__).resolve().parents[2]
MAP = REPO / "build" / "city_walk.map"
CITY_DATA = REPO / "build" / "generated" / "city_walk" / "city_data.h"

FACE_CAP = 500
COMMAND_CAP = 711  # face cap + 160 HUD commands + clip slack + setup
PRIME_SLOTS = 83   # 9 + 25 + 49 ring slots
NBG0_BIT, RBG0_BIT = 1 << 0, 1 << 4


def header_constant(name):
    text = CITY_DATA.read_text(encoding="utf-8")
    match = re.search(r"#define\s+%s\s+(0x[0-9A-Fa-f]+|\d+)u?" % name, text)
    if not match:
        raise AssertionError(f"{name} missing from {CITY_DATA}")
    return int(match.group(1), 0)


def load(scenario):
    report = HARNESS_BUILD / f"city_walk_{scenario}.json"
    dump = HARNESS_BUILD / f"city_walk_{scenario}_wramh.bin"
    for path in (report, dump, MAP):
        if not path.is_file():
            raise unittest.SkipTest(f"{path} missing: run harness/run-city-walk-checks.ps1")
    probe = json.loads(report.read_text(encoding="utf-8"))
    telemetry = city_telemetry.read(MAP, dump)
    if telemetry["magic"] != city_telemetry.MAGIC:
        raise AssertionError(f"{scenario}: g_city was never initialised (magic {telemetry['magic']:08X})")
    return probe, telemetry


def executed_inside_the_program(testcase, probe):
    iso = probe.get("iso_path", "").replace("\\", "/")
    testcase.assertTrue(iso.endswith("/city_walk.iso"), f"probe report is for {iso!r}")
    boot = probe.get("boot", {})
    testcase.assertTrue(boot.get("injected"), "the program was not injected")
    start = boot["load_addr"]
    testcase.assertTrue(start <= boot["pc_after_run"] < start + boot["bin_bytes"],
                        "execution left the injected program (crash or runaway)")


def screenshot(name):
    from PIL import Image
    path = HARNESS_BUILD / name
    if not path.is_file():
        raise unittest.SkipTest(f"{path} missing")
    return Image.open(path).convert("RGB")


def histogram(telemetry):
    return [telemetry[f"hist{i}"] for i in range(8)]  # bucket i = i + 1 VBlanks per frame


def percentile_bucket(hist, fraction):
    """The VBlanks-per-frame value at `fraction` of the measured frames."""
    total = sum(hist)
    running = 0
    for bucket, count in enumerate(hist):
        running += count
        if running >= fraction * total:
            return bucket + 1
    return len(hist)


class WalkTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.probe, cls.t = load("walk")

    def test_program_ran_to_the_running_state(self):
        executed_inside_the_program(self, self.probe)
        self.assertEqual(self.t["state"], 2, "not RUNNING")
        self.assertEqual(city_telemetry.signed(self.t["status"]), 0)
        self.assertGreater(self.t["frames"], 200)

    def test_archive_was_streamed_intact_to_a_4mb_cart(self):
        self.assertEqual(self.t["cart_type"], 4)
        self.assertEqual(self.t["cart_capacity"], 4 * 1024 * 1024)
        self.assertGreater(self.t["cart_used_bytes"], 0)
        self.assertEqual(self.t["cart_used_bytes"] + self.t["cart_free_bytes"],
                         self.t["cart_capacity"])
        self.assertEqual(self.t["archive_bytes"], header_constant("CITY_ARCHIVE_BYTES"))
        self.assertEqual(self.t["blob_base"], header_constant("CITY_BLOB_BASE"))
        self.assertEqual(self.t["material_count"], header_constant("CITY_MATERIAL_COUNT"))
        self.assertEqual(self.t["toc_crc_ok"], 1, "TOC/material CRC mismatch after the copy")
        self.assertEqual(self.t["ground_loaded"], 1)
        self.assertGreater(self.t["load_spans"], 1)

    def test_load_time_is_bounded_by_the_disc_not_by_spinning(self):
        # ~2.6 s per MiB-scale archive was measured; the bound leaves 3x for
        # emulator scheduling but would catch a per-sector seek regression.
        seconds = self.t["load_total_vblanks"] / 60.0
        self.assertGreater(seconds, 0.5, "load finished implausibly fast: was the disc read?")
        self.assertLess(seconds, 12.0)

    def test_residency_paged_while_walking(self):
        t = self.t
        self.assertGreaterEqual(t["prime_loads"], 1)
        self.assertGreater(t["chunks_loaded"], t["prime_loads"] + 20,
                           "walking crossed no chunk border: nothing paged")
        # The invariant of the state machine: every load is either still
        # resident or was evicted, exactly once.
        self.assertEqual(t["chunks_loaded"] - t["evictions"], t["resident_slots"])
        self.assertLessEqual(t["resident_slots"], PRIME_SLOTS)

    def test_no_stale_slot_is_ever_drawn_and_nothing_fails_to_decode(self):
        self.assertEqual(self.t["stale_generation_submits"], 0)
        self.assertEqual(self.t["decode_failures"], 0, hex(self.t["decode_first_error"]))
        self.assertEqual(self.t["scene_first_error"], 0)
        self.assertEqual(self.t["hud_status"], 0, "a HUD draw call failed")

    def test_facade_textures_streamed_into_vdp1_vram_and_drawn(self):
        t = self.t
        # Every served slot with a texture block copies it: at least the prime
        # of the first view, plus some while walking.
        self.assertGreater(t["textures_uploaded"], 40, "no facade texture reached VDP1 VRAM")
        self.assertEqual(t["texture_failures"], 0, "a texture block was refused or not written")
        self.assertGreater(t["texture_bytes_uploaded"], 100_000)
        self.assertGreater(t["max_textured_faces"], 50, "no textured face was drawn")
        # The arena sits after the command/Gouraud area and the solid pool.
        self.assertGreaterEqual(t["texture_vram_base"], 80 * 1024)
        self.assertLessEqual(t["texture_vram_base"] + 350_720, 512 * 1024)

    def test_face_and_command_budgets_hold(self):
        self.assertGreater(self.t["max_world_faces"], 50, "nothing was drawn")
        self.assertLessEqual(self.t["max_world_faces"], FACE_CAP)
        self.assertLessEqual(self.t["max_vdp1_commands"], COMMAND_CAP)

    def test_slave_took_the_outer_rings_without_falling_back(self):
        self.assertEqual(self.t["parallel_backend"], 1, "Slave SH-2 did not start")
        self.assertGreater(self.t["slave_batches"], 0)
        self.assertGreater(self.t["slave_items"], self.t["slave_batches"])
        self.assertEqual(self.t["slave_fallbacks"], 0)

    def test_frame_rate_meets_the_budget(self):
        hist = histogram(self.t)
        self.assertGreater(sum(hist), 300, "too few measured frames")
        # At most 3 VBlanks (20 fps) for the median, 4 for the 95th percentile.
        self.assertLessEqual(percentile_bucket(hist, 0.50), 3, f"median too slow: {hist}")
        self.assertLessEqual(percentile_bucket(hist, 0.95), 4, f"p95 too slow: {hist}")

    def test_ground_and_sky_layers_are_on_and_vdp1_does_not_hide_them(self):
        vdp2 = self.probe["vdp2"]
        self.assertNotEqual(vdp2["bgon"] & NBG0_BIT, 0, "NBG0 sky is off")
        self.assertNotEqual(vdp2["bgon"] & RBG0_BIT, 0, "RBG0 ground is off")
        self.assertEqual(vdp2["ktctl"] & 1, 1, "RBG0 coefficient table is off")
        # An opaque VDP1 erase would paint over both.
        self.assertEqual(self.probe["vdp1"]["ewdr"], 0)

    def test_screen_shows_sky_ground_and_buildings(self):
        image = screenshot("city_walk_walk_end.png")
        w, h = image.size
        top = image.crop((0, 0, w, h // 6)).resize((1, 1)).getpixel((0, 0))
        bottom = image.crop((0, h - h // 6, w, h)).resize((1, 1)).getpixel((0, 0))
        self.assertGreater(top[2], top[0] + 20, f"top of the screen is not sky-blue: {top}")
        # The bottom is ground (road, kerb, lawn): anything but the sky. A
        # missing RBG0 plane shows the sky below the horizon, as blue as the top.
        gap = sum((a - b) ** 2 for a, b in zip(top[:3], bottom[:3])) ** 0.5
        self.assertGreater(gap, 60, f"bottom of the screen looks like sky: {bottom} vs {top}")
        colours = image.getcolors(maxcolors=w * h)
        self.assertGreater(len(colours), 12, "the frame is nearly flat: no geometry drawn")


class WallTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.probe, cls.t = load("wall")

    def test_walking_into_a_building_stops_at_it(self):
        executed_inside_the_program(self, self.probe)
        t = self.t
        self.assertGreaterEqual(t["collisions"], 5, "the walker was never pushed out of a wall")
        # The building is 17.5 units ahead inside chunk (8, 5); walking on for
        # many seconds must not carry the walker through it into chunk z = 6.
        self.assertEqual((t["chunk_x"], t["chunk_z"]), (8, 5))
        self.assertLess(t["local_z_fx"], 32 * 65536)
        self.assertEqual(t["decode_failures"], 0, hex(t["decode_first_error"]))


class RefusalTests(unittest.TestCase):
    def check(self, scenario, state, status, cart_type, capacity, screenshot_name):
        probe, t = load(scenario)
        executed_inside_the_program(self, probe)
        self.assertEqual(t["state"], state)
        self.assertEqual(city_telemetry.signed(t["status"]), status)
        self.assertEqual(t["cart_type"], cart_type)
        self.assertEqual(t["cart_capacity"], capacity)
        # Refused before touching anything: nothing loaded, nothing drawn.
        self.assertEqual(t["chunks_loaded"], 0)
        self.assertEqual(t["archive_bytes"], 0)
        self.assertEqual(t["done"], 0, "the running loop must not start")
        image = screenshot(screenshot_name)
        r, g, b = image.getpixel((3, 3))
        self.assertGreater(r, g + 60, f"refusal screen is not the red one: {(r, g, b)}")
        self.assertGreater(len(image.getcolors(maxcolors=100000)), 1, "no message drawn")

    def test_no_cartridge_is_refused_cleanly(self):
        self.check("nocart", 3, -9, 0, 0, "city_walk_nocart.png")

    def test_one_megabyte_cartridge_is_refused_not_half_loaded(self):
        self.check("onemeg", 4, -4, 1, 1024 * 1024, "city_walk_onemeg.png")


if __name__ == "__main__":
    unittest.main()
