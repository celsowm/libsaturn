"""Incremental input/signature behavior for tools/import_model.py."""

import json
import shutil
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path
from types import SimpleNamespace

REPO = Path(__file__).resolve().parents[1]
TOOLS = REPO / "tools"
DATA = REPO / "tests" / "data" / "model3d"
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(REPO / "tests"))

import import_model
from test_import_animated import make_quadrant_glb


class ImportIncrementalTests(unittest.TestCase):
    def run_cli(self, *args):
        return subprocess.run(
            [sys.executable, str(TOOLS / "import_model.py"), *map(str, args)],
            cwd=REPO, capture_output=True, text=True, check=False,
        )

    def copy_obj_fixture(self, directory):
        for name in ("tri.obj", "test.mtl", "atlas.png"):
            shutil.copy(DATA / name, directory / name)
        return directory / "tri.obj", directory / "model"

    def test_obj_inputs_options_and_output_damage_invalidate_cache(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            obj, prefix = self.copy_obj_fixture(directory)
            args = ["--input", obj, "--out-prefix", prefix, "--incremental"]

            first = self.run_cli(*args)
            self.assertEqual(first.returncode, 0, msg=first.stderr)
            source = Path(str(prefix) + ".c")
            header = Path(str(prefix) + ".h")
            manifest = Path(str(prefix) + ".import.json")
            self.assertTrue(manifest.is_file())
            original_source = source.read_bytes()

            source_time = source.stat().st_mtime_ns
            second = self.run_cli(*args)
            self.assertEqual(second.returncode, 0, msg=second.stderr)
            self.assertIn("UP-TO-DATE", second.stdout)
            self.assertEqual(source.stat().st_mtime_ns, source_time)

            forced = self.run_cli(*args, "--force-import")
            self.assertEqual(forced.returncode, 0, msg=forced.stderr)
            self.assertNotIn("UP-TO-DATE", forced.stdout)

            time.sleep(0.02)
            changed_option = self.run_cli(*args, "--scale", "2")
            self.assertEqual(changed_option.returncode, 0, msg=changed_option.stderr)
            self.assertNotIn("UP-TO-DATE", changed_option.stdout)
            self.assertNotEqual(source.read_bytes(), original_source)

            time.sleep(0.02)
            texture = directory / "atlas.png"
            texture.write_bytes(texture.read_bytes() + b"\0")
            changed_texture = self.run_cli(*args, "--scale", "2")
            self.assertEqual(changed_texture.returncode, 0, msg=changed_texture.stderr)
            self.assertNotIn("UP-TO-DATE", changed_texture.stdout)

            source.write_text("corrupted output", encoding="utf-8")
            repaired_source = self.run_cli(*args, "--scale", "2")
            self.assertEqual(repaired_source.returncode, 0, msg=repaired_source.stderr)
            self.assertNotEqual(source.read_text(encoding="utf-8"), "corrupted output")

            header.unlink()
            repaired_header = self.run_cli(*args, "--scale", "2")
            self.assertEqual(repaired_header.returncode, 0, msg=repaired_header.stderr)
            self.assertTrue(header.is_file())

            manifest.write_text("[]", encoding="utf-8")
            repaired_manifest = self.run_cli(*args, "--scale", "2")
            self.assertEqual(repaired_manifest.returncode, 0, msg=repaired_manifest.stderr)
            self.assertNotIn("UP-TO-DATE", repaired_manifest.stdout)
            self.assertIsInstance(json.loads(manifest.read_text(encoding="utf-8")), dict)

            manifest_before_failure = manifest.read_bytes()
            texture.unlink()
            failed = self.run_cli(*args, "--scale", "3")
            self.assertNotEqual(failed.returncode, 0)
            self.assertEqual(manifest.read_bytes(), manifest_before_failure)

    def test_animated_glb_incremental_cli_and_parameter_invalidation(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            glb = make_quadrant_glb(directory)
            prefix = directory / "animated"
            args = ["--input", glb, "--target", "saturn", "--simplify", "off",
                    "--out-prefix", prefix, "--report", directory / "report.json",
                    "--incremental"]

            first = self.run_cli(*args)
            self.assertEqual(first.returncode, 0, msg=first.stderr[-2000:])
            second = self.run_cli(*args)
            self.assertEqual(second.returncode, 0, msg=second.stderr[-2000:])
            self.assertIn("UP-TO-DATE", second.stdout)

            changed = self.run_cli(*args, "--scale", "2")
            self.assertEqual(changed.returncode, 0, msg=changed.stderr[-2000:])
            self.assertNotIn("UP-TO-DATE", changed.stdout)

            glb_bytes = glb.read_bytes()
            self.assertIn(b'"still"', glb_bytes)
            glb.write_bytes(glb_bytes.replace(b'"still"', b'"other"', 1))
            changed_glb = self.run_cli(*args, "--scale", "2")
            self.assertEqual(changed_glb.returncode, 0, msg=changed_glb.stderr[-2000:])
            self.assertNotIn("UP-TO-DATE", changed_glb.stdout)

    def test_obj_and_gltf_sidecars_are_part_of_signature(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            obj, _ = self.copy_obj_fixture(directory)
            obj_dependencies = import_model._referenced_input_paths(obj)
            self.assertEqual(
                {path.name for path in obj_dependencies}, {"test.mtl", "atlas.png"}
            )

            gltf = directory / "external.gltf"
            buffer = directory / "mesh.bin"
            image = directory / "texture.png"
            buffer.write_bytes(b"buffer-a")
            image.write_bytes(b"image-a")
            gltf.write_text(json.dumps({
                "asset": {"version": "2.0"},
                "buffers": [{"uri": "mesh.bin", "byteLength": 8}],
                "images": [{"uri": "texture.png"}],
            }), encoding="utf-8")

            dependencies = import_model._referenced_input_paths(gltf)
            self.assertEqual({path.name for path in dependencies}, {"mesh.bin", "texture.png"})
            args = SimpleNamespace(
                input=str(gltf), out_prefix=str(directory / "out"), report=None,
                incremental=True, force_import=False, signature_only=False,
                signature_file=None,
            )
            first_signature = import_model._import_signature(args)["signature"]
            buffer.write_bytes(b"buffer-b")
            second_signature = import_model._import_signature(args)["signature"]
            self.assertNotEqual(first_signature, second_signature)

    def test_signature_stamp_is_not_rewritten_when_unchanged(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            obj, prefix = self.copy_obj_fixture(directory)
            stamp = directory / "model.signature.json"
            generated = self.run_cli("--input", obj, "--out-prefix", prefix,
                                     "--incremental")
            self.assertEqual(generated.returncode, 0, msg=generated.stderr)
            args = ["--input", obj, "--out-prefix", prefix,
                    "--signature-only", "--signature-file", stamp]
            first = self.run_cli(*args)
            self.assertEqual(first.returncode, 0, msg=first.stderr)
            first_mtime = stamp.stat().st_mtime_ns
            first_payload = stamp.read_bytes()

            second = self.run_cli(*args)
            self.assertEqual(second.returncode, 0, msg=second.stderr)
            self.assertEqual(stamp.stat().st_mtime_ns, first_mtime)
            self.assertEqual(stamp.read_bytes(), first_payload)

            changed = self.run_cli(*args, "--scale", "2")
            self.assertEqual(changed.returncode, 0, msg=changed.stderr)
            self.assertNotEqual(stamp.read_bytes(), first_payload)


if __name__ == "__main__":
    unittest.main()
