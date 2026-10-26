# Native WRAM placement study

The procedural cube ladder isolates memory traffic before changing the
shared renderer. Two identical 864-quad cubes use the same topology, camera,
materials, visibility tests and painter. Stage 4 rebuilds geometry every
submission; stage 2 emits a previously projected and ordered control.
All SDK-driven runs below use complete-list presentation. These are emulator
observations, not measurements on a physical Saturn.

## Controls

The unchanged working arrays occupy 246,256 bytes: actors 56,176, faces
179,712, keys 6,912 and order 3,456. The runner checks each ELF range against
the selected bank. Stock RAM is used. Make's ordinary sources compile at
`-O2`, with scene face preparation at its existing `-O3` override. Cache-on
Ymir explicitly selects cache emulation; BIOS/probe hashes match across
controls. Compiler commands and exact ELFs are retained with each case.

| High WRAM selection | Ymir geometry ms | Ymir flush ms | Ymir complete work ms |
| --- | ---: | ---: | ---: |
| None | 47.104 | 12.873 | 62.341 |
| Actors only | 46.306 | 12.873 | 61.538 |
| Faces only | 46.705 | 12.754 | 61.819 |
| Actors, faces and ordering | 45.850 | 12.635 | 60.845 |

All four still schedule 15 submissions/s in Ymir. Exact model-region pixels
match, as do face and command counts and the 210,600 Hz FRT calibration.
Full High placement reduces measured Ymir work by about 2.4%; that result
alone would give little reason to prioritize memory placement.

With the same cases booted from real CD images in Mednafen, displayed
geometry samples at recorded seconds 18, 22 and 26 are:

| High WRAM selection | Geometry FRT ticks (18 / 22 / 26 seconds) |
| --- | --- |
| None | 13024 / 13023 / 13024 |
| Actors only | 10521 / 10521 / 10521 |
| Faces only | 12553 / 12553 / 12553 |
| Actors, faces and ordering | 9973 / 9973 / 9972 |

The actors-only control reduces that stage by about **19.2%**. The faces-only
control reduces it by about **3.6%**. These three snapshots are not a latency
distribution. Full High placement is around 23.4% below the Low control.
The final isolated controls all publish the independent bank mask. A separate
paired actor-High/face-Low run completes 295 Slave tasks without guest errors
or draw timeouts, validating the mixed-bank worker path. This evidence points
first to hot vertex/projection data, rather than
assuming that reducing or moving face records is the largest opportunity.

## Retained evidence

Private local artifacts under `harness/build/`:

- `native-memory-isolated-{low,high}/`: final explicit bank-mask Ymir controls.
- `native-memory-actors-high-checked/` and `native-memory-faces-high/`:
  independently placed controls; retained command lines, hashes and pixels.
- `native-memory-isolated-{low,high}-cd/`, `native-memory-actors-high-cd/`,
  `native-memory-faces-high-cd/`: serial actual Mednafen CD recordings.
- Corresponding `*-timeline/` directories contain extracted original movie
  frames and a movie-hash manifest. No reboot is used for extraction.
- `native-memory-complete-{low,high}-scene-cd/`: preceding complete-list
  all-Low/all-High control, before independent placement switches.

An early `native-memory-actors-high/` report is invalid: an initial runner
variable-shadowing error used the last buffer name as the case key. It is
retained for diagnosis and replaced by `*-checked/`. Early automatic-draw
controls are likewise retained but superseded by complete-list controls.

## Applying the result

Keep immutable descriptors, textures and topology separate from hot mutable
vertex/projection scratch, and budget both banks before moving data. The
Tekken private build cannot fit its entire actor or scene face array into
the available High WRAM. Moving a smaller hot subset is a separate consumer
experiment that must preserve exact pixels, Slave publication and memory
bounds, and pass native CD execution. Dino already declares its mesh
projection buffers in High WRAM; this study does not establish a new Dino
speed improvement. No shared runtime renderer optimization is claimed here.
