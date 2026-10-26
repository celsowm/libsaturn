# Exact composited video observations

The headless probe can record changes in the same composited RGB output used
for its PNG screenshots. This measures VDP1 and VDP2 together; draw starts,
draw completion and framebuffer swaps are separate events and do not determine
whether the displayed pixels changed.

Use `--video-observations PATH.csv` to retain one row per program field.
Optionally choose `--video-crop X:Y:WIDTH:HEIGHT` in software-output pixels to
exclude counters/HUD. Without a crop the complete image participates. Crop
dimensions must be positive and fit every observed output frame. This is an
opt-in host diagnostic; it adds no guest hooks or runtime policy changes.

```sh
harness/build-macos/probe --iso "$SATURN_ISO" --bin "$SATURN_BIN" \
  --bios "$SATURN_BIOS" --sh2-cache on --frames 3901 \
  --video-observations "$CAPTURE/video.csv" --video-crop 0:61:320:135 \
  --out "$CAPTURE/probe.json"
python3 harness/scripts/analyze_video_observations.py \
  --csv "$CAPTURE/video.csv" --probe-report "$CAPTURE/probe.json" \
  --after-field 300 --out "$CAPTURE/video-summary.json"
```

The caller supplies an existing output directory and its own firmware/program.
The crop above fits the local 320x224 Paul scene; other applications must choose
their own region. Fields 300 through 3900 give 3,600 adjacent-field intervals,
or 60 seconds at the declared nominal 60 Hz. The first selected image is the
baseline and its preceding transition is excluded.

## Equality, completeness and units

The observer compares every RGB pixel with the preceding crop and ignores the
unused alpha byte, matching `png_writer.hpp`. Its FNV-1a64 fingerprint is only
diagnostic: a hash collision cannot hide a changed pixel. Repeated rendering
of identical RGB counts as unchanged. Returning to an earlier image counts as
a transition; this is not a count of globally unique poses or images.

The CSV includes resolution, crop, software-render callback delta, comparison
coverage, changed-pixel count, fingerprint and system-cycle timestamp. Storage
keeps one previous crop and streams the rows rather than buffering a complete
video. Synchronous VDP1/VDP2/deinterlacer readback is required.

The analyzer rejects missing/noncontiguous fields, incomplete run length,
missing or multiple composited callbacks per field in the selected window,
resolution/crop changes, corrupt comparison counts and nonadvancing system
cycles. The initial baseline may have no predecessor. Reports bind the CSV
and probe JSON with SHA-256 and retain cache/divider-model metadata. System
cycles describe the scheduler timeline, not either CPU's unsynchronized FRT.

`rgb_changes_per_second` is **a content-transition rate**, not complete-frame
FPS. Partial drawing, transient erases, changing HUD and unstable double-buffer
contents can all change pixels. The selected crop and actual frame lifecycle
must be investigated before declaring distinct completed application pictures.
Automatic VDP1 drawing can repeat the same list every VBlank, and can expose
table-update timing that a submission counter does not describe.

## Qualified initial controls

The native portable observer passes ASan/UBSan and the harness CTest gate.
Synthetic tests exercise repeated images, alpha-only changes, a real pixel
change, returning to an old image, excluded HUD pixels, resolution changes,
malformed crops and out-of-bounds data. Python tests exercise the observation
schema, warmup/interval boundaries, completeness, callback counts, dimensions,
system time, corrupt counts/fingerprints and metadata requirements. BIOS-free
CLI checks reject malformed crop requests and a crop without an output path.

Private Paul controls with the same release guest and cache-on emulator:

| Scene/crop | Field interval | Observed RGB transitions |
| --- | --- | ---: |
| Frozen tick 120, models below HUD | 300–1199 | 0 / 899 intervals |
| Animated neutral, same crop | 300–3900 | 800 / 3600 intervals |

The frozen control continues CPU submissions/HUD updates and VDP activity, yet
the model crop stays identical. The animated 13.33 transitions/s exceeds the
roughly 11.2 submissions/s measured for this guest: it must **not** be labeled
13.33 FPS. Screenshots at program fields 300 and 301 have the same HUD FRAME
51 and different model pixels. The current Tekken integration uses automatic
PTMR=2 drawing, while Dino already selects `SAT_VDP1_FRAME_ON_SUBMIT` for
multifield preparation. Qualifying that existing policy in the port is the
next experiment; this observer does not alter the VDP1 driver.

Evidence is retained privately in the sibling port's
`assets/imported/visible-{static,neutral,transition}-128/` directories. These
are injected emulator observations, not CD-boot or physical timing evidence.
