r"""Acceptance for the shmup stress demo (examples/bullet_hell_stress) on Ymir.

Run through the wrapper, which builds the demo twice (each run reads the telemetry through its own
link map), runs the probe and dumps Work RAM and screenshots:

    .\harness\run-bullet-hell.ps1 -Bios .\bios\saturn_bios_us.bin

  play   the default build with a pad script that skips to stage 2: the next stage's enemy sprites
         and background are streamed from the disc while stage 1 plays, then swapped in. Checks the
         swap, that it never waited for the disc, that the music never underran, and the pictures.
  bench  the BH_AUTOBENCH build: 128/256/512/1024 simultaneous bullets x 300 frames. Checks the
         command counts, the collision test counts, that no list was late, and the CPU time per frame.

Ymir has no VDP1 fill-rate model, so the bounds on frames-per-field here are for the CPU side only
(simulation, instance build, command submission); the fill-bound figures come from Mednafen, see
docs/SHMUP_BULLET_HELL_SUPPORT.md.

The struct is g_bh_telemetry in examples/bullet_hell_stress/main.c: all uint32_t, big-endian.
"""

import os
import unittest

from harness.scripts import telemetry as tel

MAGIC = 0x42485331  # "BHS1"
FIELDS = (
    "magic setup_ok setup_error frame video_frames frt_hz sim_hash bullets enemies shots explosions "
    "stage stage_serial bench_phase cmd_used cmd_capacity instances batch_submitted batch_culled "
    "batch_dropped batch_invalid batch_no_preclip t_serv t_sim t_build t_draw t_end t_wait late_swaps "
    "draw_waits draw_timeouts fps_x10 scroll_x underruns refills music_buffered music_playing "
    "music_min_buffered music_segments music_loops data_deferred stream_bytes stream_segments "
    "stream_jobs stream_waits stream_error stage_swaps transition_stalls group_ready_mask bg_swaps "
    "pad_held cd_progress_errors prof_vdp2 prof_pad prof_disc prof_audio prof_poll prof_music "
    "prof_data").split()
BENCH_FIELDS = (
    "n frames vblanks ticks_total ticks_serv ticks_sim ticks_build ticks_draw ticks_end ticks_max "
    "late_swaps commands drawn bullet_tests player_hits done").split()
TAIL_FIELDS = "ccr_initial ccr_now".split()
SIZES = (128, 256, 512, 1024)
# Fields per program frame on Ymir (CPU only): 60 fps for the light cases, and well under the
# 30 fps cliff (2.0) for 1024. A regression that doubles the per-frame work trips them.
VBLANK_RATIO_MAX = {128: 1.05, 256: 1.05, 512: 1.15, 1024: 1.6}


def _load(tag):
    base = os.environ.get("LIBSATURN_BH_DIR")
    if not base:
        raise unittest.SkipTest("run harness/run-bullet-hell.ps1 first")
    count = len(FIELDS) + 4 * len(BENCH_FIELDS) + len(TAIL_FIELDS)
    values = tel.load_dump(os.path.join(base, tag + ".wram"), os.path.join(base, tag + ".map"),
                           "_g_bh_telemetry", ">%dI" % count)
    result = dict(zip(FIELDS, values))
    result["bench"] = []
    for phase in range(4):
        first = len(FIELDS) + phase * len(BENCH_FIELDS)
        result["bench"].append(dict(zip(BENCH_FIELDS, values[first:first + len(BENCH_FIELDS)])))
    tail = len(FIELDS) + 4 * len(BENCH_FIELDS)
    result.update(zip(TAIL_FIELDS, values[tail:]))
    result["base"] = base
    return result


def _image(base, name):
    from PIL import Image
    image = Image.open(os.path.join(base, name + ".png")).convert("RGB")
    sx, sy = image.size[0] // 640, image.size[1] // 224
    if sx not in (1, 2) or sy not in (1, 2):
        raise AssertionError("not a 640x224 picture: %r" % (image.size,))
    return image.resize((640, 224)) if (sx, sy) != (1, 1) else image


def _region(image, box):
    return list(image.crop(box).getdata())


def _spread(pixels):
    """Distinct colours: a flat or blank picture has one or two."""
    return len(set(pixels))


def _lit(pixels):
    return sum(1 for p in pixels if max(p) > 180)


def _mean(pixels):
    n = len(pixels)
    return tuple(sum(p[c] for p in pixels) / n for c in range(3))


# The 448 x 224 field sits between two 96 px panels of the 640 x 224 screen.
FIELD = (96, 0, 544, 224)
LEFT = (0, 0, 96, 224)
RIGHT = (544, 0, 640, 224)


class PlayTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.t = _load("play")

    def test_boot_and_setup(self):
        self.assertEqual(self.t["magic"], MAGIC)
        self.assertEqual((self.t["setup_ok"], self.t["setup_error"]), (1, 0))
        self.assertGreater(self.t["frame"], 600)

    def test_stage_swap_without_waiting_for_the_disc(self):
        t = self.t
        self.assertEqual(t["stage_swaps"], 1)
        self.assertEqual(t["stage_serial"], 1)
        self.assertEqual(t["transition_stalls"], 0)
        self.assertEqual(t["bg_swaps"], 1)
        self.assertEqual(t["stream_error"], 0)
        self.assertGreaterEqual(t["stream_jobs"], 4)   # music is not a job; groups + backgrounds are
        # residency groups 0 (common art) and 2 (stage 2 enemies) are drawable
        self.assertEqual(t["group_ready_mask"] & 0b101, 0b101)

    def test_music_never_underran(self):
        t = self.t
        self.assertEqual(t["underruns"], 0)
        self.assertEqual(t["music_playing"], 1)
        self.assertGreater(t["music_segments"], 20)
        # the rings hold 16384 frames; data segments only start above the 8192-frame reserve
        self.assertGreaterEqual(t["music_min_buffered"], 4096)

    def test_no_late_lists_or_draw_timeouts(self):
        self.assertEqual(self.t["late_swaps"], 0)
        self.assertEqual(self.t["draw_timeouts"], 0)
        self.assertEqual(self.t["cd_progress_errors"], 0)

    def test_pictures_show_each_stage(self):
        base = self.t["base"]
        one, two = _image(base, "play_s1"), _image(base, "play_s2")
        for name, image in (("stage 1", one), ("stage 2", two)):
            self.assertGreater(_spread(_region(image, FIELD)), 20, name + " field is flat")
            self.assertGreater(_lit(_region(image, LEFT)), 30, name + " left panel is empty")
            self.assertGreater(_lit(_region(image, RIGHT)), 100, name + " right panel has no HUD text")
        # a different scrolling background and different enemy colours
        m1, m2 = _mean(_region(one, FIELD)), _mean(_region(two, FIELD))
        self.assertGreater(sum(abs(a - b) for a, b in zip(m1, m2)), 12, (m1, m2))


class BenchTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.t = _load("bench")

    def test_setup_and_streaming_clean(self):
        t = self.t
        self.assertEqual(t["magic"], MAGIC)
        self.assertEqual((t["setup_ok"], t["setup_error"]), (1, 0))
        self.assertEqual((t["stream_error"], t["underruns"], t["draw_timeouts"]), (0, 0, 0))
        self.assertEqual(t["batch_dropped"], 0)
        self.assertEqual(t["batch_invalid"], 0)

    def test_every_size_ran_300_frames(self):
        for size, b in zip(SIZES, self.t["bench"]):
            self.assertEqual((b["n"], b["frames"], b["done"]), (size, 300, 1), b)

    def test_commands_follow_the_bullet_count(self):
        for size, b in zip(SIZES, self.t["bench"]):
            # the bullets plus the player, shots, enemies and the HUD glyphs; never past the list
            self.assertGreaterEqual(b["drawn"], size, b)
            self.assertLessEqual(b["drawn"], size + 160, b)
            self.assertLessEqual(b["commands"], self.t["cmd_capacity"], b)
            self.assertLessEqual(b["commands"], b["drawn"] + 24, b)   # setup commands and END
        self.assertEqual(self.t["cmd_capacity"], 2048)

    def test_collision_tests_are_exact(self):
        # every bullet is tested against the player core every frame: no bullet is skipped
        for size, b in zip(SIZES, self.t["bench"]):
            self.assertEqual(b["bullet_tests"], size * b["frames"], b)

    def test_no_late_lists(self):
        for b in self.t["bench"]:
            self.assertEqual(b["late_swaps"], 0, b)

    def test_cpu_time_per_frame(self):
        hz = self.t["frt_hz"]
        field = hz / 59.92
        for size, b in zip(SIZES, self.t["bench"]):
            ratio = b["vblanks"] / b["frames"]
            self.assertLessEqual(ratio, VBLANK_RATIO_MAX[size], (size, ratio))
            self.assertLess(b["ticks_total"] / b["frames"], 1.6 * field, (size, b))
            self.assertLess(b["ticks_max"], 2.5 * field, (size, b))
        # work grows with the bullet count: the simulation does, and nothing is skipped
        sims = [b["ticks_sim"] for b in self.t["bench"]]
        self.assertEqual(sims, sorted(sims))

    def test_picture_during_the_run(self):
        base = self.t["base"]
        for name in ("bench_mid", "bench_end"):
            image = _image(base, name)
            self.assertGreater(_spread(_region(image, FIELD)), 20, name)
            self.assertGreater(_lit(_region(image, RIGHT)), 100, name + " has no HUD text")


if __name__ == "__main__":
    unittest.main()
