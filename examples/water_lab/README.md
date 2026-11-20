# Water Lab

Water Lab is an original flooded-temple example for a stock Sega Saturn. It
uses the SDK scene painter at 320×224 in a non-interlaced 16-bit VDP1
framebuffer. The room, mosaic, glass, actor and particles are generated in
code and are covered by the repository's MIT license.

| Saturn pad | Action |
| --- | --- |
| D-pad | Move |
| L / R | Orbit the camera |
| A / B | Jump or swim up / dive |
| X | Cycle rippling water, dry reference, opaque, mesh, RGB half, underwater and multipass request |
| Y | Cycle the water level |
| C | Cycle the fixed-tick orbit, low waterline and follow views |
| Z | Toggle diagnostics |
| Z + X | Pause or resume the simulation tick |
| Z + Y | Cycle 128, 256 and 512 dry faces |
| Z + B | Cycle the 4×4, 8×8 and 16×16 water grid |
| Z + A | Toggle waves on native water |
| Start | Reset scene and controls |

The demo opens in Rippling Water. Animated wave lighting and ripple highlights
move over the submerged mosaic. Dry Reference hides the surface while keeping
the simulation running. Press Z for the compact diagnostics panel. It reports
display FPS, CPU preparation, surface and command emission times in
microseconds, VDP1 command counts and pacing, clipping and fallbacks,
water level, and player and camera immersion.
Pause before changing modes to compare the same camera, player and actor
state. The underwater mode uses a flat surface so its geometry and cut share
the same waterline. Multipass uses the experimental VDP2 compositor where
supported. It displays the native RGB half mode with a reason if the setup or
scene is refused.

```sh
./build-example.sh water_lab
./run-example.sh water_lab
./harness/run-water-lab.sh
python3 harness/tests/test_water_lab.py
```

The playable disc is `build/examples/water_lab.bin` and
`build/examples/water_lab.cue`. The Ymir run writes screenshots, register
dumps and telemetry to `build/water_lab_evidence/`. Ymir takes a caller-owned
BIOS dump. Screenshot assertions use Pillow.

The procedural assets and application code are MIT-licensed. The default
local disc embeds a boot payload whose redistribution rights are pending in
the SDK packaging documentation. Distribute the application binary or build
the disc with a boot payload cleared for distribution.

After building both `water_lab` and `water_lab_probe`, create the application
archive with `python3 tools/package_water_lab.py`. Add `--discs local` to
package both discs for hardware testing. For publication, build both examples
with the cleared `NATIVE_BOOT_PAYLOADS` manifest, then pass that same manifest
to `--discs public --payloads PATH`. The packager checks the actual disc
sectors and first-read files against the binaries and boot manifest and
records the hashes in the archive. Rebuild with `--force` when switching to an
older payload manifest.

[Water rendering](../../docs/WATER_RENDERING.md) · [Compositor probe](../../docs/FX_COMPOSITOR.md) · [Benchmark results](../../docs/WATER_BENCHMARKS.md) · [License](../../LICENSE)
