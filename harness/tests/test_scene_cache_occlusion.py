"""Ymir screenshot regression for mixed world/cached painter (GPL-3.0).

The BIOS-backed probe is opt-in and is NOT silently counted as a pass if
captures are missing: invoke harness/run-scene-cache-occlusion.ps1 with a
lawfully obtained personal Saturn IPL ROM. Synthetic image tests still run
without firmware to validate that the acceptance criterion itself works.
"""
import json
import os
from pathlib import Path
import unittest

try:
    from PIL import Image
except ImportError:
    Image = None


def red_actor_pixels(image):
    """Count strong-red RGB actor pixels, excluding yellow/cyan wall and HUD."""
    rgb = image.convert("RGB")
    return sum(
        1 for r, g, b in rgb.getdata()
        if r > 180 and r > g * 1.65 and r > b * 1.55
    )


class OcclusionCriterionTests(unittest.TestCase):
    @unittest.skipIf(Image is None, "Pillow not installed")
    def test_red_mask_ignores_checker_and_white_hud(self):
        img = Image.new("RGB", (5, 1))
        img.putdata([
            (255, 49, 74),  # Player/actor.
            (205, 238, 49),  # Yellow cached wall.
            (41, 156, 246),  # Blue cached wall.
            (255, 255, 255), # White HUD.
            (0, 0, 0),
        ])
        self.assertEqual(red_actor_pixels(img), 1)


@unittest.skipUnless(
    Image is not None and os.environ.get("LIBSATURN_OCCLUSION_DIR"),
    "BIOS-backed screenshot capture not supplied",
)
class SceneCacheOcclusionProbeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.path = Path(os.environ["LIBSATURN_OCCLUSION_DIR"])
        with (cls.path / "probe.json").open(encoding="utf-8") as f:
            cls.probe = json.load(f)
        iso = cls.probe.get("iso_path", "").replace("\\", "/")
        if not iso.endswith("/scene_cache_occlusion.iso"):
            raise AssertionError(f"Unexpected probe ISO: {iso!r}")
        boot = cls.probe.get("boot")
        if not boot or not boot.get("injected"):
            raise AssertionError("Ymir did not inject the game binary")
        pc, begin = boot["pc_after_run"], boot["load_addr"]
        if not (begin <= pc < begin + boot["bin_bytes"]):
            raise AssertionError("Guest PC is outside the running example")
        cls.images = {}
        for name in ("far_55", "near_105", "camera_145", "zoom_175"):
            with Image.open(cls.path / f"{name}.png") as image:
                cls.images[name] = image.convert("RGB").copy()

    def test_matching_screen_dimensions(self):
        self.assertEqual(
            {im.size for im in self.images.values()}, {(320, 224)}
        )

    def test_actor_occlusion_changes_with_depth_not_submission_order(self):
        # Same camera, same wall, same app-side submission order; only actor
        # depth changed at frame 80. It must expose more red when in front.
        far = red_actor_pixels(self.images["far_55"])
        near = red_actor_pixels(self.images["near_105"])
        self.assertGreater(
            near, far + 40,
            f"Expected in-front actor to expose >=41 more red pixels; "
            f"behind={far}, in_front={near}",
        )

    def test_camera_and_zoom_rebakes_render_distinct_frames(self):
        near = self.images["near_105"].tobytes()
        camera = self.images["camera_145"].tobytes()
        zoom = self.images["zoom_175"].tobytes()
        self.assertNotEqual(near, camera, "A camera toggle did not change image")
        self.assertNotEqual(camera, zoom, "B zoom/rebake did not change image")


if __name__ == "__main__":
    unittest.main()
