import re
import subprocess
import tempfile
import unittest
from pathlib import Path


class AssetConverterTests(unittest.TestCase):
    def test_png_conversion_generates_c_and_legacy_outputs(self) -> None:
        repo = Path(__file__).resolve().parents[1]
        tool = repo / "tools" / "convert_indexed8.py"

        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            png = tmp_path / "sample.png"
            out_prefix = tmp_path / "png_out"

            try:
                from PIL import Image
            except ModuleNotFoundError as exc:  # pragma: no cover - environment guard
                self.skipTest(f"pillow unavailable: {exc}")

            img = Image.new("RGBA", (8, 8), (0, 0, 0, 0))
            for y in range(8):
                for x in range(8):
                    if x < 4 and y < 4:
                        color = (255, 0, 0, 255)
                    elif x >= 4 and y < 4:
                        color = (0, 255, 0, 255)
                    elif x < 4 and y >= 4:
                        color = (0, 0, 255, 255)
                    else:
                        color = (255, 255, 255, 255)
                    img.putpixel((x, y), color)
            img.save(png)

            result = subprocess.run(
                [
                    "python",
                    str(tool),
                    "--input",
                    str(png),
                    "--out-prefix",
                    str(out_prefix),
                ],
                cwd=repo,
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(result.returncode, 0, msg=result.stderr)

            tex = out_prefix.with_suffix(".tex8")
            palette = out_prefix.with_suffix(".pal")
            header = out_prefix.with_suffix(".h")
            source = out_prefix.with_suffix(".c")

            self.assertTrue(tex.exists())
            self.assertTrue(palette.exists())
            self.assertTrue(header.exists())
            self.assertTrue(source.exists())
            self.assertEqual(len(tex.read_bytes()), 64)
            self.assertEqual(len(palette.read_bytes()), 512)
            self.assertIn("png_out_pixels", source.read_text(encoding="utf-8"))

    def test_png_resize_generates_smaller_asset(self) -> None:
        repo = Path(__file__).resolve().parents[1]
        tool = repo / "tools" / "convert_indexed8.py"

        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            png = tmp_path / "resize.png"
            out_prefix = tmp_path / "resize_out"

            try:
                from PIL import Image
            except ModuleNotFoundError as exc:  # pragma: no cover - environment guard
                self.skipTest(f"pillow unavailable: {exc}")

            img = Image.new("RGBA", (32, 16), (255, 255, 255, 255))
            for y in range(16):
                for x in range(32):
                    if x < 16:
                        img.putpixel((x, y), (255, 0, 0, 255))
                    else:
                        img.putpixel((x, y), (0, 0, 255, 255))
            img.save(png)

            result = subprocess.run(
                [
                    "python",
                    str(tool),
                    "--input",
                    str(png),
                    "--resize",
                    "16",
                    "8",
                    "--out-prefix",
                    str(out_prefix),
                ],
                cwd=repo,
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(result.returncode, 0, msg=result.stderr)

            source_text = out_prefix.with_suffix(".c").read_text(encoding="utf-8")
            self.assertIn("16u,", source_text)
            self.assertIn("8u,", source_text)
            self.assertEqual(len(out_prefix.with_suffix(".tex8").read_bytes()), 128)

    def test_raw_conversion_generates_c_and_legacy_outputs(self) -> None:
        repo = Path(__file__).resolve().parents[1]
        tool = repo / "tools" / "convert_indexed8.py"

        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            raw = tmp_path / "sample.raw"
            pal = tmp_path / "sample.pal.txt"
            out_prefix = tmp_path / "sample_out"

            pixels = bytes([0, 1, 2, 3, 4, 5, 6, 7] * 8)  # 8x8
            palette_lines = [
                "0 0 0",
                "255 255 255",
                "255 0 0",
                "0 255 0",
                "0 0 255",
            ]
            raw.write_bytes(pixels)
            pal.write_text("\n".join(palette_lines), encoding="utf-8")

            result = subprocess.run(
                [
                    "python",
                    str(tool),
                    "--input",
                    str(raw),
                    "--width",
                    "8",
                    "--height",
                    "8",
                    "--palette",
                    str(pal),
                    "--out-prefix",
                    str(out_prefix),
                ],
                cwd=repo,
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(result.returncode, 0, msg=result.stderr)

            tex = out_prefix.with_suffix(".tex8")
            palette = out_prefix.with_suffix(".pal")
            header = out_prefix.with_suffix(".h")
            source = out_prefix.with_suffix(".c")

            self.assertTrue(tex.exists())
            self.assertTrue(palette.exists())
            self.assertTrue(header.exists())
            self.assertTrue(source.exists())
            self.assertEqual(tex.read_bytes(), pixels)
            self.assertEqual(len(palette.read_bytes()), 512)

            header_text = header.read_text(encoding="utf-8")
            source_text = source.read_text(encoding="utf-8")

            self.assertIn("typedef struct sat_indexed8_asset", header_text)
            self.assertIn("extern const sat_indexed8_asset_t", header_text)
            self.assertIn("8u,", source_text)
            self.assertIn("const sat_indexed8_asset_t", source_text)

            symbol_prefix = re.sub(r"[^0-9A-Za-z]+", "_", out_prefix.name)
            symbol_prefix = re.sub(r"_+", "_", symbol_prefix).strip("_")
            pixel_match = re.search(
                rf"const uint8_t {re.escape(symbol_prefix)}_pixels\[\d+\] = \{{(.*?)\}};",
                source_text,
                re.S,
            )
            palette_match = re.search(
                rf"const uint16_t {re.escape(symbol_prefix)}_palette\[\d+\] = \{{(.*?)\}};",
                source_text,
                re.S,
            )
            self.assertIsNotNone(pixel_match)
            self.assertIsNotNone(palette_match)

            pixel_tokens = re.findall(r"0x([0-9A-Fa-f]{2})", pixel_match.group(1))
            palette_tokens = re.findall(r"0x([0-9A-Fa-f]{4})", palette_match.group(1))
            palette_values = {int(token, 16) for token in palette_tokens}

            self.assertEqual(bytes(int(token, 16) for token in pixel_tokens), pixels)
            self.assertEqual(len(palette_tokens), 256)
            self.assertEqual(int(palette_tokens[0], 16), 0x8000)
            self.assertIn(0x801F, palette_values)
            self.assertIn(0x83E0, palette_values)
            self.assertIn(0xFC00, palette_values)
            self.assertIn(0xFFFF, palette_values)

    def test_rejects_non_multiple_of_8_width(self) -> None:
        repo = Path(__file__).resolve().parents[1]
        tool = repo / "tools" / "convert_indexed8.py"

        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            raw = tmp_path / "bad.raw"
            pal = tmp_path / "bad.pal.txt"
            raw.write_bytes(bytes([0] * 70))
            pal.write_text("0 0 0\n", encoding="utf-8")

            result = subprocess.run(
                [
                    "python",
                    str(tool),
                    "--input",
                    str(raw),
                    "--width",
                    "10",
                    "--height",
                    "7",
                    "--palette",
                    str(pal),
                    "--out-prefix",
                    str(tmp_path / "out"),
                ],
                cwd=repo,
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("multipla de 8", result.stderr + result.stdout)

    def test_rejects_missing_palette_for_raw(self) -> None:
        repo = Path(__file__).resolve().parents[1]
        tool = repo / "tools" / "convert_indexed8.py"

        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            raw = tmp_path / "sample.raw"
            raw.write_bytes(bytes([0] * 16))

            result = subprocess.run(
                [
                    "python",
                    str(tool),
                    "--input",
                    str(raw),
                    "--width",
                    "4",
                    "--height",
                    "4",
                    "--out-prefix",
                    str(tmp_path / "out"),
                ],
                cwd=repo,
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Entrada .raw exige --palette", result.stderr + result.stdout)


if __name__ == "__main__":
    unittest.main()
