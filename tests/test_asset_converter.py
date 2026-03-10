import subprocess
import tempfile
import unittest
from pathlib import Path


class AssetConverterTests(unittest.TestCase):
    def test_raw_conversion_generates_tex_and_palette(self) -> None:
        repo = Path(__file__).resolve().parents[1]
        tool = repo / "tools" / "convert_indexed8.py"

        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            raw = tmp_path / "sample.raw"
            pal = tmp_path / "sample.pal.txt"
            out_prefix = tmp_path / "sample_out"

            pixels = bytes([0, 1, 2, 3, 4, 5, 6, 7] * 8)  # 8x8
            raw.write_bytes(pixels)
            pal.write_text(
                "\n".join(
                    [
                        "0 0 0",
                        "255 255 255",
                        "255 0 0",
                        "0 255 0",
                        "0 0 255",
                    ]
                ),
                encoding="utf-8",
            )

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
            self.assertTrue(tex.exists())
            self.assertTrue(palette.exists())
            self.assertEqual(tex.read_bytes(), pixels)
            self.assertEqual(len(palette.read_bytes()), 512)

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


if __name__ == "__main__":
    unittest.main()

