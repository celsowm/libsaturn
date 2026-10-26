# Compiled material tables and texture descriptor placement

The native WRAM study first separated hot geometry from cold actor state.
This follow-up independently moves immutable compiled command prefixes and
their borrowed native texture descriptors. No material validation, painter
ordering, clipping, resource lifetime or VDP1 command rule is removed.
The shared renderer runtime and public ABI are unchanged.

## Dino controls

`DINO_HIGH_MATERIAL_PROGRAMS` and `DINO_HIGH_TEXTURE_DESCRIPTORS` now default
to 1. Zero retains each independent Low-WRAM control. Dino's 1,256 texture
slots require 20,096 bytes of compiled entries and 15,072 bytes of native
descriptors: **35,168 bytes** move to High WRAM. The profiled High build ends
BSS at `0x060be5a0`; the ordinary stock-memory linker and stack reserve remain
enforced. No new allocation or frame-time table compilation is introduced.

`DINO_PIPELINE_METRICS=1 DINO_PROFILE_HUD=1` displays FRAME and the previous
completed sample's FLUSH/WORK **raw local FRT counts**. Both diagnostics default
OFF. `DINO_BENCH_PAUSED=1` starts at animation time zero with the VDP2 light
disabled, making real CD boots comparable without injected state or manual
pad timing. This control defaults OFF; pad controls can still change it.

Four explicit bank combinations compile with native disc and stock-memory
checks. The runner's optional `--expect-programs-ram` and
`--expect-textures-ram` check actual complete ELF ranges in normal Low/High
WRAM; a missing expected table, empty table, wrong bank or crossed boundary
rejects the run. Actual addresses/sizes are retained in `analysis.json`.

## Measurements and correctness

Cache-on Ymir paused controls have zero different model pixels in the
640x224 viewport crop `(0,56)-(640,200)`, with 900 reported Slave tasks and
zero failed tasks at field 1200. Both schedule 24 submissions/s between
fields 600 and 1200. Profiling medians in the final paused-HUD controls are:

| Phase | Both Low, ms | Both High, ms |
| --- | ---: | ---: |
| Flush | 7.8845 | 7.7990 |
| Emission within flush | 4.7212 | 4.6784 |
| Complete work | 33.1338 | 33.0294 |

The new controls therefore do not establish a Ymir cadence improvement.
An animated input-control run also completes without render overflow,
failed tasks or music errors. The importer, topology and model are unchanged.

Actual serial Mednafen CD boots of the paused controls show the same model
pixels, including a separately checked normalized crop `(0,120)-(704,420)`
with zero differing RGB pixels. Manually read original movie observations:

| Recorded second | Low FRAME | High FRAME | Low FLUSH | High FLUSH | Low WORK | High WORK |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 22 | 92 | 92 | 4753 | 3802 | 13501 | 12375 |
| 28 | 164 | 164 | 4753 | 3802 | 13509 | 12384 |
| 34 | 236 | 236 | 4754 | 3802 | 13516 | 12389 |

These are selected instrumented local FRT samples, not a latency distribution
or hardware qualification. Flush decreases by roughly **20%**. The selected
complete-work samples decrease by roughly 8.3%; other end-of-movie samples
show about 5.2%, demonstrating why a single HUD sample is insufficient to
claim an overall speedup. Both counters advance 144 submissions over the
same 12 recorded seconds: **no native Dino cadence gain is established**.
The scene is static in this control; submissions are not distinct-image FPS.

The private Tekken consumer independently moves 40,544 bytes, preserves exact
frozen model pixels and serial/Slave results, and measures its own live CD
interval. Its result must not be extrapolated from these Dino measurements.

## Reusable build and capture commands

```sh
python3 harness/scripts/build_example_variant.py --example dino_demo \
  --out harness/build/dino-table-control --compiler-dir "$SH2_TOOLS" \
  --python "$PYTHON_WITH_PILLOW" \
  --make-option DINO_HIGH_MATERIAL_PROGRAMS=0 \
  --make-option DINO_HIGH_TEXTURE_DESCRIPTORS=0 \
  --make-option DINO_PIPELINE_METRICS=1 \
  --make-option DINO_PROFILE_HUD=1 --make-option DINO_BENCH_PAUSED=1
python3 harness/scripts/dino_pipeline.py \
  --build harness/build/dino-table-control --out harness/build/dino-table-run \
  --bios "$BIOS" --scenario paused --paused-at-boot \
  --expect-programs-ram low --expect-textures-ram low
python3 harness/scripts/cube_pipeline.py \
  --record-cd harness/build/dino-table-control --record-example dino_demo \
  --bios "$BIOS" --out harness/build/dino-table-cd --seconds 35
```

Run Mednafen recordings serially. Its base directory lock prevents overlapping
instances. Preserve movies and extract additional frames from those recordings.

The example builder retains six artifacts, exact hashes, command line, revision
and hashes of the main source/Makefiles under the example's shared-alias lock.
It rejects output reuse, reserved/duplicate overrides, failed compilation and
changes to its recorded source files during compilation. This is not a complete
transitive source manifest. The Dino runner verifies and copies this manifest
when supplied, and takes the same lock while retaining shared build aliases.

Two build reliability fixes apply to the generic builder and cube runner:

- Preserve the venv interpreter path. Resolving its symlink to the base Python
  loses the venv's Pillow installation.
- Select GNU Make >=4.3, preferring `gmake`. macOS's stock 3.81 treats grouped
  `&:` asset targets incorrectly and can start duplicate model imports.

Six targeted tests cover artifact retention, source drift/failure, venv paths,
Make selection and table-bank bounds; the existing 12 cube and nine phase
decoder tests pass. CI builds all four Dino bank combinations with the
profiling HUD and paused control, alongside the existing cube controls.

## Retained evidence

Private local `harness/build/material-banks-dino-*` artifacts:

- `*-{low,both,programs,textures}-paused-build/`: explicit independent builds.
- `*-{low,both,programs,textures}-boot-paused/`: injected paused pixel/timing
  controls and checked table placements.
- `*-{low,both}-paused-mednafen/` and `*-paused-timeline/`: actual native CD
  movies and extracted/hash-identified original frames.
- `*-both-controls/`: scripted animation/camera/light inputs.
- `*-release/`: normal High-table build with diagnostic HUD/paused mode OFF.

Early failed venv/system-Make builds and preliminary animated-HUD runs are
retained but superseded by these controls. The separate Phase 3 all-example
gate has an independently confirmed missing
`examples/bullet_hell_stress/assets/palette_bgr555_be.bin`; these results are
not represented as an all-SDK green qualification.
