"""Check captures produced by harness/run-water-lab.sh."""
import csv
import json
import os
from pathlib import Path
import struct
import sys
import unittest

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from verify_water_lab_capture import check_pairs, layers

EVIDENCE = Path("build/water_lab_evidence")


def words(name, count):
    return struct.unpack(f">{count}I", (EVIDENCE / f"{name}-telemetry.bin").read_bytes())


def shot(name):
    with Image.open(EVIDENCE / f"{name}.png") as image:
        return image.convert("RGB")


def fingerprint(*pixels):
    value = 14695981039346656037
    for pixel in pixels:
        for channel in pixel:
            value = ((value ^ channel) * 1099511628211) & ((1 << 64) - 1)
    return value


def command_modes(name):
    data = (EVIDENCE / f"{name}-commands.bin").read_bytes()
    offset = struct.unpack_from(">H", data, 2)[0] * 8
    modes = []
    while offset + 32 <= len(data):
        control, _, mode = struct.unpack_from(">3H", data, offset)
        if control & 0x8000:
            return modes
        if control & 15 == 4:
            modes.append(mode)
        offset += 32
    raise AssertionError("VDP1 list reached the end of the dump without an END command")


class WaterLab(unittest.TestCase):
    def test_startup_water_and_diagnostics(self):
        clean, hud = shot("rippling-water"), shot("compact-hud")
        self.assertEqual(clean.size, (320, 224))
        self.assertEqual(hud.size, clean.size)
        self.assertNotEqual(clean.getpixel((5, 31)), (16, 32, 56))
        self.assertEqual(hud.getpixel((5, 31)), (16, 32, 56))
        for x in (6, 110, 212):
            text = hud.crop((x, 30, x + 98, 89))
            self.assertIn((240, 232, 192), (color for _, color in text.getcolors()))
        data = words("hud", 20)
        self.assertEqual((data[9], data[10]), (6, 8))
        self.assertGreaterEqual(command_modes("hud").count(0x00C7), 64)
        self.assertGreater(data[8] - data[4], 200)

    def test_rippling_water_presentation_rate(self):
        with (EVIDENCE / "hud-fields.csv").open(newline="") as source:
            fields = [row for row in csv.DictReader(source) if 60 <= int(row["frame"]) < 170]
        self.assertEqual(len(fields), 110)
        changed = sum(int(row["changed_pixels"]) > 0 for row in fields)
        refresh = json.loads((EVIDENCE / "hud.json").read_text())["boot"]["nominal_refresh_hz"]
        self.assertGreaterEqual(changed * refresh / len(fields), 24)
        self.assertEqual(words("hud", 20)[7], 0)

    def test_ntsc_pool_pacing(self):
        with (EVIDENCE / "pool-fields.csv").open(newline="") as source:
            changes = [int(row["frame"]) for row in csv.DictReader(source)
                       if int(row["frame"]) >= 40 and int(row["changed_pixels"])]
        self.assertGreaterEqual(len(changes), 64)
        self.assertEqual({b - a for a, b in zip(changes, changes[1:])}, {2})
        self.assertEqual(words("pool", 20)[6:8], (0, 0))

    def test_native(self):
        for name in ("matched-dry", "opaque", "mesh", "half", "underwater", "underwater-low-camera"):
            self.assertEqual(shot(name).size, (320, 224))
        dry, opaque, mesh, half = (shot(name) for name in ("matched-dry", "opaque", "mesh", "half"))
        indexed = (112, 112)
        # Pick an interior sample from the RGB floor patch. VDP1 polygons can
        # draw a shared edge twice, particularly as the camera tick shifts.
        for x in range(178, 192, 2):
            rgb = (x, 112)
            if (dry.getpixel(rgb), opaque.getpixel(rgb), half.getpixel(rgb),
                mesh.getpixel(rgb), mesh.getpixel((x + 1, 112))) == (
                    (96, 128, 120), (40, 160, 184), (64, 144, 152),
                    (40, 160, 184), (96, 128, 120)):
                break
        else:
            self.fail("RGB floor patch has no matching half-blend and mesh samples")
        self.assertEqual(dry.getpixel(indexed), (80, 112, 120))
        self.assertEqual(half.getpixel(indexed), opaque.getpixel(indexed))
        native = words("native", 20)
        self.assertEqual(native[9:12], (4, 8, 1))
        self.assertEqual(native[13], 1)
        self.assertGreater(len(shot("underwater-low-camera").getcolors(1024)), 25)
        self.assertGreaterEqual(command_modes("mesh").count(0x01C0), 64)
        self.assertGreaterEqual(command_modes("half").count(0x00C3), 64)
        self.assertEqual(words("mesh", 20)[0], words("half", 20)[0])
        for name, mode in (("mesh", 2), ("half", 3)):
            data = words(name, 20)
            self.assertEqual((data[3], data[4], data[9]), (128, 192, mode))
            self.assertEqual(data[7], 0)
        self.assertEqual(json.loads((EVIDENCE / "pal.json").read_text())["boot"]["nominal_refresh_hz"], 50)

    def test_synthetic_transparency(self):
        image = shot("synthetic")
        self.assertEqual(image.getpixel((30, 40)), (136, 104, 88))
        self.assertEqual(image.getpixel((100, 40)), (136, 104, 88))
        self.assertEqual(image.getpixel((160, 40)), (68, 52, 44))
        self.assertEqual(image.getpixel((239, 40)), (68, 52, 44))
        self.assertEqual(image.getpixel((240, 40)), (100, 160, 156))

    def test_scene_compositor(self):
        native, wide = (shot(name) for name in ("scene-fx-native", "scene-fx-wide"))
        for x, y in ((90, 109), (164, 105), (175, 119), (164, 165)):
            self.assertEqual(wide.getpixel((x, y)), native.getpixel((x, y)))
        # Inside the water, the VDP2 palette and blending path change pixels.
        self.assertNotEqual(wide.getpixel((164, 145)), native.getpixel((164, 145)))
        for box in ((4, 6, 90, 19), (4, 192, 312, 222)):
            self.assertEqual(wide.crop(box).tobytes(), native.crop(box).tobytes())
        near = shot("scene-fx-near")
        self.assertNotEqual(near.tobytes(), wide.tobytes())
        moving, later = (shot(name) for name in ("scene-fx-moving", "scene-fx-moving-later"))
        self.assertNotEqual(moving.crop((72, 100, 236, 162)).tobytes(),
                            later.crop((72, 100, 236, 162)).tobytes())
        self.assertNotEqual(wide.crop((4, 19, 150, 29)).tobytes(),
                            shot("scene-fx-exit").crop((4, 19, 150, 29)).tobytes())
        data = words("scene-fx", 12)
        self.assertGreaterEqual(data[1], 100)
        self.assertEqual(data[5], 0)
        self.assertEqual(data[9], 0)  # Layer disabled and workspace released.
        self.assertGreater(data[10], 160 * 2)
        self.assertLess(data[10], 160 * 112 * 2)
        self.assertEqual(data[10], data[11] * 2)

    def test_composition_recovery(self):
        for name, serial in (("held-draw", 6), ("held-upload", 10), ("held-partial", 14)):
            with self.subTest(name=name):
                image = shot(name)
                base, blend = layers(serial)
                self.assertEqual(image.getpixel((159, 40)), blend)
                self.assertEqual(image.getpixel((160, 40)), base)
                self.assertEqual(image.getpixel((280, 16)), (224, 232, 232))
        after = shot("composed")
        self.assertIn((after.getpixel((160, 40)), after.getpixel((159, 40))),
                      [layers(i) for i in range(128)])
        disabled = shot("composition-disabled")
        base, _ = layers(32)
        self.assertEqual(disabled.getpixel((159, 40)), base)
        self.assertEqual(disabled.getpixel((160, 40)), base)
        data = words("composition", 21)
        self.assertEqual(data[1], 0)
        self.assertGreaterEqual(data[3], 15)
        self.assertGreaterEqual(data[2], data[3] - 2)
        self.assertEqual(data[12], 1)
        self.assertEqual(data[14:17], (1, 1, 1))
        self.assertEqual(data[20], 2)

    def test_composition_field_sequence(self):
        self.check_sequence(EVIDENCE, int(os.environ.get("WATER_LAB_MIN_PAIRS", "3600")), 60)

    def test_pal_composition_field_sequence(self):
        self.check_sequence(EVIDENCE / "pal-composition", 300, 50)

    def check_sequence(self, directory, minimum_pairs, refresh):
        def signature(base, blend):
            return fingerprint(blend, base)

        with (directory / "composition-fields.csv").open(newline="") as source:
            fields = list(csv.DictReader(source))
        samples = ((int(row["frame"]), int(row["rgb_fingerprint"], 16)) for row in fields)
        result = check_pairs(samples, signature, minimum_pairs, refresh)
        for field in fields[result["first_pair_field"] + 1:]:
            self.assertEqual(field["callback_delta"], "1", f"field {field['frame']}")
        data = struct.unpack(">21I", (directory / "composition-telemetry.bin").read_bytes())
        self.assertIn(result["matched_pairs"], (data[3], data[3] + 1))
        self.assertEqual(data[1], 0)
        self.assertEqual(data[20], 2)
        boot = json.loads((directory / "composition.json").read_text())["boot"]
        self.assertEqual(boot["nominal_refresh_hz"], refresh)
        print(f"Composition {refresh} Hz: {result['matched_pairs']} matched pairs in {result['fields']} fields, "
              f"{result['pairs_per_second']:.2f} pairs/s from field {result['first_pair_field']}")


if __name__ == "__main__":
    EVIDENCE = Path(sys.argv.pop(1)) if len(sys.argv) > 1 else EVIDENCE
    unittest.main()
