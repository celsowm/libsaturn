"""test_basic_3d_texture.py - harness assertions for the textured model viewer.

GPL-3.0 (see harness/LICENSE / harness/README.md for why this directory is
licensed separately from the rest of this repo).

Run after probing the example (requires a Saturn BIOS/IPL image)::

    .\\harness\\run-harness.ps1 basic_3d_texture -Frames 120
    $env:LIBSATURN_PROBE_JSON = 'harness/build/probe.json'
    python -m unittest discover harness/tests -k basic_3d_texture

What this proves (and what it does not): the probe dumps VDP1 erase
registers and the display framebuffer, not the VDP1 command stream, so this
file asserts boot + "something textured was drawn and stays drawn". The
command-level proof that textured faces emit distorted sprites (and polygons
for untextured faces, with shared culling/sorting) lives in host tests
tests/host/test_mesh3d_textured.cpp, which stubs the submit layer. Orbit
motion itself was verified during development with --pad-script RIGHT plus
--screenshot pairs (holding RIGHT for 60 frames changed ~1000 pixels while
an idle 100-frame run changed zero).
"""

import json
import os
import unittest


def _default_probe_json_path():
    env = os.environ.get("LIBSATURN_PROBE_JSON")
    if env:
        return env
    here = os.path.dirname(os.path.abspath(__file__))
    return os.path.join(here, "..", "build", "probe.json")


class Basic3DTextureProbeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = _default_probe_json_path()
        if not os.path.exists(path):
            raise unittest.SkipTest(
                f"probe.json not found at {path} - run "
                f"'.\\harness\\run-harness.ps1 basic_3d_texture' first (requires a "
                f"Saturn BIOS/IPL image; see harness/README.md)."
            )
        with open(path, "r", encoding="utf-8") as f:
            cls.probe = json.load(f)
        iso = cls.probe.get("iso_path", "")
        if "basic_3d_texture" not in iso.replace("\\", "/"):
            raise unittest.SkipTest(
                f"probe.json is for {iso!r}, not basic_3d_texture - re-run "
                f"run-harness.ps1 basic_3d_texture first."
            )
        boot = cls.probe.get("boot")
        if boot is None or not boot.get("injected"):
            raise AssertionError("probe.json has no boot.injected=true - was probe.exe run with --bin?")
        load_addr = boot["load_addr"]
        pc = boot["pc_after_run"]
        if not (load_addr <= pc < load_addr + boot["bin_bytes"]):
            raise AssertionError(
                f"pc_after_run=0x{pc:08X} outside injected program range - our code never ran."
            )

    def test_opaque_erase_for_clean_backdrop(self):
        # The viewer uses sat_app_frame_begin with an opaque black clear.
        self.assertEqual(self.probe["vdp1"]["ewdr"], 0x8000)

    def test_model_and_hud_drawn_to_framebuffer(self):
        sample = self.probe["vdp1"]["display_framebuffer_sample"]
        self.assertGreater(len(sample), 0)
        nonzero = sum(1 for b in sample if b != 0)
        # An empty scene would be all zeros; the textured model plus HUD text
        # lights roughly half of even a tiny 256-byte sample window.
        self.assertGreater(nonzero, 0, "framebuffer sample is all zeros - model never drew")


if __name__ == "__main__":
    unittest.main()
