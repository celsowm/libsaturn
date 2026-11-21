"""Check captures produced by harness/run-water-pool.sh."""
import hashlib
from pathlib import Path
import struct
import unittest

from PIL import Image, ImageChops

EVIDENCE = Path("build/water_pool_evidence")
ONE = 1 << 16

# SHA-256 of each capture's RGB pixels, from Ymir core 5c571f68 with a
# synchronous software renderer, NTSC 320x224 and no RAM cartridge. Captures
# repeat exactly; update these only after a reviewed visual change. The replay
# is stopped by simulation tick, so it is identical in NTSC and PAL. The rest
# and grid captures are taken at fixed fields and so depend on frame pacing;
# the drop-early capture is only checked for a visible ball.
PINNED = {
    "replay": "7a3e76a85100de128af0e2fe0ba4e55c770e6fd546c1025fd5b99cb01fa0c846",
    "rest": "4e4a43d5718b650b3eea52d691caaaf904484198126ce525a0b7ef02941f9721",
    "grid13": "712d1199fe937990287c9b1cbbb41b215fe7e7205059c959eb1faaa611b63b9e",
    "grid17": "47757f01964c505a77393b1cd30a2d38fa3988b1944ae77075a7534d6d3c322b",
}


def telemetry(name):
    fields = struct.unpack(">8I3i3I", (EVIDENCE / f"{name}-telemetry.bin").read_bytes()[:56])
    keys = ("tick", "nodes", "volume_high", "volume_low", "faces", "preparation",
            "flush", "frequency", "ball_x", "ball_y", "ball_z", "gravity", "paused", "script")
    data = dict(zip(keys, fields))
    data["volume"] = struct.unpack(">q", struct.pack(">II", data["volume_high"], data["volume_low"]))[0]
    return data


def shot(name):
    with Image.open(EVIDENCE / f"{name}.png") as image:
        return image.convert("RGB")


def orange_pixels(image):
    return sum(1 for r, g, b in image.get_flattened_data() if r > 200 and 100 < g < 190 and b < 90)


class WaterPoolTest(unittest.TestCase):
    def test_rest_pool_has_no_water_volume(self):
        self.assertEqual(telemetry("rest")["volume"], 0)

    def test_drops_add_exact_volume(self):
        # One pressed field plus one held tick: two drops of 4 units each. The
        # ball only moves water between nodes, so the total is exactly the drops.
        data = telemetry("drop")
        self.assertEqual(data["nodes"], 9)
        self.assertEqual(data["volume"] % (4 * ONE), 0)
        self.assertGreater(data["volume"], 0)

    def test_ball_is_visible_and_surface_changes(self):
        # The replay holds a rippled pool at a fixed tick; the rest capture is calm.
        calm, rippled = shot("rest"), shot("replay")
        self.assertGreater(orange_pixels(calm) + orange_pixels(rippled), 20)
        changed = sum(1 for p in ImageChops.difference(calm, rippled).get_flattened_data() if max(p) > 6)
        self.assertGreater(changed, 500)

    def test_ball_floats_near_the_surface(self):
        data = telemetry("drop")
        self.assertGreater(data["ball_y"], -3 * ONE)
        self.assertLess(data["ball_y"], 2 * ONE)

    def test_detail_grid_runs(self):
        data = telemetry("grid17")
        self.assertEqual(data["nodes"], 17)
        self.assertGreater(data["tick"], 20)
        self.assertNotEqual(shot("grid13").tobytes(), shot("grid17").tobytes())

    def test_captures_match_pinned_pixels(self):
        for name, expected in PINNED.items():
            self.assertEqual(hashlib.sha256(shot(name).tobytes()).hexdigest(), expected, name)

    def test_replay_is_independent_of_frame_pacing(self):
        ntsc, pal = telemetry("replay"), telemetry("replay-pal")
        for data in (ntsc, pal):
            self.assertEqual((data["paused"], data["tick"], data["volume"]), (1, 69, 4 * ONE))
        self.assertEqual((ntsc["ball_x"], ntsc["ball_y"], ntsc["ball_z"]),
                         (pal["ball_x"], pal["ball_y"], pal["ball_z"]))
        self.assertEqual(shot("replay").tobytes(), shot("replay-pal").tobytes())

    def test_pool_is_framed(self):
        image = shot("rest")
        self.assertEqual(image.size, (320, 224))
        water = image.getpixel((160, 130))
        self.assertGreater(water[2], water[0])


if __name__ == "__main__":
    unittest.main()
