"""Check telemetry and captures produced by harness/run-water-lake.sh."""
import hashlib
from pathlib import Path
import struct
import unittest

from PIL import Image

EVIDENCE = Path("build/water_lake_evidence")
SECTIONS = 13
MODES = ("naive", "full", "naive_tex", "full_tex")
HEADER = struct.Struct(">23I3i")
SECTION = struct.Struct(">8I")

# Faces submitted, culled, clipped, fallback, skipped and world commands per 30-tick stretch of
# the route, from Ymir core 5c571f68. They follow the route by tick, not by
# frame pacing, so they repeat exactly; update them only after a reviewed
# change to the scene or the example.
PINNED = {
    "naive": [
        (384, 0, 16, 16, 0, 368),
        (384, 0, 17, 17, 0, 367),
        (384, 0, 6, 6, 0, 378),
        (376, 8, 32, 32, 0, 347),
        (218, 166, 40, 40, 0, 183),
        (75, 311, 40, 40, 0, 37),
        (199, 185, 32, 32, 0, 171),
        (305, 79, 103, 103, 0, 207),
        (56, 330, 38, 38, 0, 19),
        (282, 102, 53, 53, 0, 236),
        (220, 164, 96, 96, 0, 128),
        (275, 109, 131, 131, 0, 151),
        (276, 108, 50, 50, 0, 233),
    ],
    "full": [
        (440, 0, 16, 16, 0, 424),
        (429, 0, 17, 17, 0, 412),
        (427, 0, 6, 6, 0, 421),
        (418, 8, 34, 34, 0, 384),
        (272, 190, 48, 48, 0, 200),
        (106, 351, 49, 49, 0, 35),
        (251, 211, 43, 43, 0, 186),
        (345, 97, 122, 122, 0, 230),
        (86, 380, 46, 46, 0, 19),
        (338, 123, 63, 63, 0, 276),
        (280, 186, 114, 114, 0, 158),
        (342, 120, 168, 168, 0, 180),
        (336, 124, 60, 60, 0, 276),
    ],
    "naive_tex": [
        (416, 0, 17, 17, 0, 399),
        (416, 0, 18, 18, 0, 398),
        (416, 0, 6, 6, 0, 410),
        (408, 8, 33, 33, 0, 377),
        (250, 172, 45, 45, 0, 206),
        (107, 339, 44, 44, 0, 40),
        (231, 191, 40, 40, 0, 186),
        (337, 85, 110, 110, 0, 224),
        (88, 358, 42, 42, 0, 19),
        (314, 114, 57, 57, 0, 250),
        (252, 176, 102, 102, 0, 138),
        (307, 119, 142, 142, 0, 161),
        (308, 120, 55, 55, 0, 246),
    ],
    "full_tex": [
        (440, 0, 17, 17, 0, 423),
        (429, 0, 18, 18, 0, 411),
        (427, 0, 6, 6, 0, 421),
        (418, 8, 34, 34, 0, 384),
        (272, 190, 48, 48, 0, 200),
        (106, 351, 49, 49, 0, 35),
        (251, 211, 44, 44, 0, 189),
        (345, 97, 122, 122, 0, 230),
        (86, 380, 46, 46, 0, 19),
        (338, 123, 65, 65, 0, 274),
        (280, 186, 116, 116, 0, 156),
        (342, 120, 169, 169, 0, 179),
        (336, 124, 62, 62, 0, 274),
    ],
}


# SHA-256 of each pinned capture's RGB pixels: the route paused at tick 120 (the
# approach over the shore), 240 (the bridge) and 330 (under water), in each mode, from Ymir core
# 5c571f68 with a synchronous software renderer, NTSC 320x224. They are taken at a
# route tick, not a field, so frame pacing does not move them. Update them only
# after a reviewed visual change.
CAPTURES = {
    "naive-s4": "e313a30feadc91af1b2a11d61fe10f3ce4d7fe5daa172382f60254f4395e0d2b",
    "naive-s8": "1fd128ffa6cd273debec4046610fe2c784891947b9dab75331a03ec71346a083",
    "naive-s11": "8313e495b8d0f4b9872eb2a9387b9d486c57303da24cf840818cbb79c81da44c",
    "full-s4": "2667c37a31595c0e49a46d6e011b03c903410c01a18e86ab154102018a70021b",
    "full-s8": "94ae3c060c6a12e2c35728f56805c983677eed79ecf6ee987378e9158c0b319d",
    "full-s11": "38babb091859dc5691af45fa9d011432ed64e72c8e5f70dfc400537f760b4717",
    "naive_tex-s4": "2a5873a5fc0c84d2b3dab3fbb5a5fbd040f9a381d3d280c249abbd31edbd375e",
    "naive_tex-s8": "af59c0ad86acd82f4bdffaadf6073a0be987c4c4abf5229d74aa12a749fe8ed2",
    "naive_tex-s11": "f4ff559a1c633465cd523be4dc18cf5c965d8e45743042ab5d738a008962b93f",
    "full_tex-s4": "a1e3c5142aff50061a2f2d30b74c781675b3164a28c8b99674e9d91bf1d68dac",
    "full_tex-s8": "c9f49d1d8db38d01c79c2e6cc19e275178e300a49c7d47073fb4ee5c7597295b",
    "full_tex-s11": "a3b611a9f73ecf7c7c09a92ee626ffb3630d4d72d52821222e24f4292ee2cfe4",
}


def load(mode):
    data = (EVIDENCE / f"{mode}-telemetry.bin").read_bytes()
    fields = HEADER.unpack(data[:HEADER.size])
    keys = ("tick mode script paused underwater fade region fraction plane above below cut "
            "frequency errors last s0 s1 s2 s3 s4 underwater_ticks max_fade textured ax ay az").split()
    head = dict(zip(keys, fields))
    sections = [SECTION.unpack(data[HEADER.size + 32 * i:HEADER.size + 32 * (i + 1)])
                for i in range(SECTIONS)]
    return head, sections


class WaterLakeTest(unittest.TestCase):
    def test_both_replays_finish_on_the_last_tick(self):
        for mode, value in (("naive", 0), ("full", 1), ("naive_tex", 0), ("full_tex", 1)):
            head, _ = load(mode)
            self.assertEqual(head["mode"], value, mode)
            self.assertEqual(head["textured"], mode.endswith("_tex"), mode)
            self.assertEqual(head["tick"], 390, mode)
            self.assertEqual((head["script"], head["paused"]), (0, 1), mode)

    def test_every_section_drew_and_nothing_was_skipped(self):
        for mode in MODES:
            head, sections = load(mode)
            self.assertEqual((head["errors"], head["last"]), (0, 0), mode)
            for i, s in enumerate(sections):
                submitted, culled, clipped, fallback, skipped, commands = s[:6]
                self.assertGreater(submitted, 0, (mode, i))
                self.assertEqual(skipped, 0, (mode, i))
                self.assertGreater(commands, 0, (mode, i))
                self.assertLessEqual(clipped, submitted, (mode, i))

    def test_full_mode_dives_and_fades_to_the_full_tint(self):
        head, _ = load("full_tex")
        # Route ticks 300..360 are under the surface; the fade has six frames.
        self.assertGreater(head["underwater_ticks"], 30)
        self.assertLess(head["underwater_ticks"], 90)
        self.assertEqual(head["max_fade"], 6)
        self.assertEqual(head["fade"], 0)  # surfaced again, tint released

    def test_naive_mode_never_tints(self):
        for mode in ("naive", "naive_tex"):
            head, _ = load(mode)
            self.assertEqual((head["underwater_ticks"], head["max_fade"]), (0, 0), mode)

    def test_full_mode_draws_a_plane_and_prepared_level(self):
        head, _ = load("full")
        self.assertGreater(head["plane"], 0)
        self.assertLessEqual(head["plane"], 4 * 16)
        self.assertGreater(head["above"], 0)
        self.assertGreater(head["below"], 0)

    def test_routes_are_identical_between_modes_up_to_the_water(self):
        # The same camera route: the culled counts differ with the geometry, but
        # both modes see the same ticks and finish with the actor in the same place.
        naive, full = load("naive")[0], load("full")[0]
        self.assertEqual((naive["ax"], naive["ay"], naive["az"]), (full["ax"], full["ay"], full["az"]))

    def test_section_face_counts_match_pinned(self):
        for mode in MODES:
            counts = [tuple(s[:6]) for s in load(mode)[1]]
            self.assertEqual(counts, PINNED[mode], mode)

    def test_captures_stop_at_the_same_tick_in_both_modes(self):
        for section, tick in ((4, 120), (8, 240), (11, 330)):
            for mode in MODES:
                data = (EVIDENCE / f"{mode}-s{section}-telemetry.bin").read_bytes()
                tick_now, _, script, paused = struct.unpack(">4I", data[:16])
                self.assertEqual((tick_now, script, paused), (tick, tick + 1, 1), (mode, section))

    def test_captures_match_pinned_pixels(self):
        for name, expected in CAPTURES.items():
            with Image.open(EVIDENCE / f"{name}.png") as image:
                self.assertEqual(hashlib.sha256(image.convert("RGB").tobytes()).hexdigest(), expected, name)

    def test_replay_is_identical_in_pal(self):
        data = (EVIDENCE / "full-s8-pal-telemetry.bin").read_bytes()
        self.assertEqual(struct.unpack(">4I", data[:16]), (240, 1, 241, 1))
        with Image.open(EVIDENCE / "full-s8-pal.png") as image:
            self.assertEqual(hashlib.sha256(image.convert("RGB").tobytes()).hexdigest(), CAPTURES["full-s8"])

    def test_modes_render_differently_at_every_capture(self):
        for section in (4, 8, 11):
            for suffix in ("", "_tex"):
                self.assertNotEqual(CAPTURES[f"naive{suffix}-s{section}"], CAPTURES[f"full{suffix}-s{section}"], section)

    def test_textured_water_changes_the_picture(self):
        for section in (4, 8, 11):
            for mode in ("naive", "full"):
                self.assertNotEqual(CAPTURES[f"{mode}-s{section}"], CAPTURES[f"{mode}_tex-s{section}"], (mode, section))

    def test_textured_water_is_never_dropped(self):
        # A textured face behind the near plane is dropped without a fallback colour;
        # the water has one, so no mode skips a face and every clipped face is drawn.
        for mode in ("naive_tex", "full_tex"):
            for section in load(mode)[1]:
                self.assertEqual(section[4], 0, mode)
                self.assertEqual(section[2], section[3], mode)


if __name__ == "__main__":
    unittest.main()
