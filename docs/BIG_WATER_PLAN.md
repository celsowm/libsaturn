# Big Water — implementation plan

Status: M0–M7 are implemented; the rendering is measured in Ymir and
physical-hardware acceptance is pending. Where a
measurement contradicted this plan's first assumptions, the text says so.
Target: SDK support for levels with large bodies of water, such as castle
moats, bays, lakes and flooded rooms in a 3D platformer. The game supplies
level geometry and water regions. The SDK draws the water cheaply, orders it
exactly against the level, tints what lies under it, cuts actors at the
waterline and handles the underwater view.

This builds on what exists: `water.h` (waterline split, shaded split),
`water_volume.h` (one box volume, buoyancy, camera immersion),
`water_sim.h` (small simulated pools), `vdp2_color_offset.h` and the scene
painter's 8-bit `pass`. The [Water Pool](../examples/water_pool/README.md)
and [Water Lab](../examples/water_lab/README.md) examples are the evidence
base for the rendering choices below.

## Why the current pieces are not enough

- **Huge quads near the camera.** A level usually models water as a few
  large quads. The near plane cuts them, and a cut face leaves the projected
  fast path for the clipping fallback. A textured face with a corner behind
  the near plane cannot be clipped at all: it is dropped, or drawn as a solid
  `rgb555` fallback (`scene3d_faces.h`). *Measured (M0/M6): this holds for
  textured water only. For flat RGB mesh water, four large quads are four
  faces and the clip path handles them; the plane did not reduce clipped or
  fallback faces and costs more per frame. See "Measured results".*
- **Draw order.** Depth sorting large water faces against the banks and the
  bed misorders them. A water face's centroid depth says little about the
  terrain on either side of it.
- **Translucency.** VDP1 half-transparency on a distorted quad plots some
  pixels twice and shows a double-blended hatch (Water Pool, README). VDP1
  mesh does not, and it is what Saturn games used for water. The plan uses
  mesh by default and keeps half-transparency opt-in.
- **One box per world.** `sat_water_volume_t` is a single box with a fixed
  surface. Levels have several water regions, and some change their water
  height at run time.
- **No underwater look.** Water Lab has camera immersion inline in its
  example code. Nothing reusable turns it into a screen tint.

## Exact ordering with one water plane

For a horizontal plane at height `h`, and faces wholly above or below it:

- With the camera above the plane, a face above the plane is never hidden by
  the water or by anything below it: the segment from the eye to any point
  above the plane lies entirely above it. A face below the plane, and inside
  the water's region, is always behind the water wherever the water covers it
  on screen. A ray crosses the plane once, so if the water is drawn at that
  pixel, the crossing lies inside the region and in front of the point.
- So three painter passes are exact: **below** (submerged faces inside the
  region), **water**, then **above** (everything else, depth sorted as
  today). Faces below `h` outside every region keep ordinary sorting in the
  above pass.
- With the camera under the plane the order mirrors: above, water (seen
  from below), below.
- Faces crossing the plane must be split at it. Static level faces can be
  classified and split once at load or in the converter; only moving actors
  classify per frame.

This holds for one plane at a time. With several regions at different
heights, the plane is the region the camera is in or over (M1). Other
regions fall back to ordinary depth sorting, which is the current behavior.
The plan does not attempt exact ordering for several planes in one frame.

## Constraints

- Bare-metal Saturn, 320×224, no RAM cartridge, no heap in frame paths.
  Caller-owned storage with `*_requirements` helpers and `SAT_ERR_CAPACITY`
  on overflow, as elsewhere in the SDK.
- Reuse the scene painter, its passes, materials and clipping. No second
  renderer.
- Opt-in: games that do not call the new APIs see no change.
- Static work (classification, splitting, tinting) happens once, at load or
  in a host tool, not per frame.
- Measure in Ymir with the harness. Report emulator figures as emulator
  figures; physical acceptance is separate.

## Milestones (in order)

- [x] **M0 — Lake baseline.** Add `examples/water_lake`, an original level:
  a lake about 64×64 units with shores, an island, a bridge over the water,
  a submerged slope and a deep part. Draw the water the naive way first: a
  few large mesh quads. Script a fixed camera route (high orbit, low skim
  over the water, a bridge crossing, a dive under). Record telemetry for
  each route point: submitted faces, clipped and fallback faces, dropped
  textured faces, preparation and flush time. Pin captures. This is the
  number every later milestone must beat.

- [x] **M1 — Water regions.** `water_regions.h`: a caller-owned array of
  regions, each a box with its own mutable surface height, current,
  buoyancy and drag (today's `sat_water_volume_t` fields). Queries:
  - the region containing a point, or none;
  - the region a vertical segment samples, with the existing
    `sat_water_sample_t` result;
  - camera immersion with hysteresis, keyed by region.

  `sat_water_volume_*` stay as they are; a region is a volume plus an id.
  Host tests: overlapping and adjacent regions, a moving surface, entering
  one region from another, no-region equivalence with today's single volume.

- [x] **M2 — Water plane.** `water_plane.h`: draws one region's surface as a
  camera-centred grid.
  - **Grid.** Square rings around the camera snapped to the cell size, so
    vertices do not swim as the camera moves. Each ring doubles the cell
    size of the one inside it, up to a ring count. Cells are clipped to the
    region rectangle in XZ, which keeps them quads. Cells outside the view
    are culled before projection.
  - **Near the camera.** The finest ring keeps the faces that the near
    plane cuts small, so a dropped textured cell loses little. The
    material's `rgb555` fallback is the water colour, so a dropped textured
    cell is still water. (The saving is for textured water; it is not
    measured yet, see "Measured results".)
  - **Seams.** Where a fine ring meets a coarse one, the fine edge
    vertices that do not exist on the coarse side are moved onto the coarse
    edge. With wobble, the wobble is sampled at the coarse spacing along that
    edge. Check for VDP1 pixel gaps at these seams in captures. Add a
    one-pixel overlap only if gaps show.
  - **Look.** Mesh by default, half-transparency opt-in. Per-vertex Gouraud
    for a distance tint toward the horizon and an optional sky band. An
    optional sine wobble uses `sat_sin_deg_whole`, with an amplitude that
    fades to zero by the second ring.
  - **Texture motion.** VDP1 has no UV offset, so a texture cannot scroll by
    coordinates. Offer two cheap paths and measure both: cycling LUT4 colour
    entries (16 words per frame) and cycling a few pre-shifted texture frames
    in VRAM. One texture covers one cell, so coarser rings use a caller-given
    texture baked at a lower resolution, to keep the world scale of the
    pattern constant.
  - **Submission.** One scene instance per ring, in the water pass. When the
    camera is under the surface, the grid is submitted with the underside
    material and the mirrored passes.
  - **Bounds.** A fixed face cap from the ring count. `sat_water_plane_requirements`
    reports the storage needed.

  Host tests: ring layout, snapping, rectangle clipping, seam vertices and
  the face cap. Ymir: the M0 route, comparing fallback and dropped faces and
  timings against the naive quads.

- [x] **M3 — Level classification and tint.** `sat_water_prepare_mesh`: a
  one-shot pass over a static mesh, given the regions and one tint
  description.
  - It classifies each face as above, below (inside a region) or crossing.
  - It splits RGB and Gouraud crossing faces at the surface with the
    existing shaded split, into caller storage. Textured faces cannot be cut,
    since VDP1 maps a texture over the whole quad. A textured crossing face
    goes to the below class when its centroid is under the surface, and the
    function returns their count so a converter can split them offline.
  - It writes a Gouraud table that shifts submerged vertices toward the water
    colour, more with depth, up to a limit, combined with any table the mesh
    already has. LUT4 and RGB materials accept Gouraud; INDEX8 does not, so
    those faces are only classified, and counted.
  - The output is two face lists, below and above, that the game submits
    as two instances in the passes from the ordering section.

  Re-running it after a water height change (a level that floods or drains)
  is supported, but not per frame. Also provide the same classification in
  a host Python tool under `tools/`, so a converter can do it at build time
  and ship the result. Host tests compare the C and Python outputs on the
  same mesh.

- [x] **M4 — Actors at the waterline.** `sat_water_submit_instance`
  generalizes Water Pool's ball code: back-face cull, then faces wholly
  above or below go out as two instances, and only crossing faces are cut
  with the shaded split. Submerged pieces take the wet tint, and pieces go to
  the passes from the ordering section. Skinned characters reuse it after
  skinning. Textured crossing faces are classified by centroid, not cut.
  The function reports the count, so a game can see how much of a character
  stays uncut. Host tests for classification and pass assignment. Ymir:
  an actor wading, half under, and fully under.

- [x] **M5 — Underwater view.** `sat_water_view_t`, one call per frame:
  - camera immersion against the regions (M1);
  - a VDP2 colour-offset fade toward a tint over a fixed number of frames,
    on a caller-chosen bank and layer mask, released on exit;
  - flags telling the game which way round the passes run and when to swap
    its far clip, fog or back colour.

  It owns only the colour-offset bank it is given, and rejects a bank it
  cannot verify is free. Ymir: VDP2 register dumps across a dive and
  resurface, with the fade frames pinned.

- [x] **M6 — Lake example, finished.** `water_lake` uses M1–M5: the plane,
  the prepared terrain, an actor wading across, a swim with buoyancy, a
  dive under the bridge and the underwater tint. A harness script replays
  the M0 route by simulation tick (as Water Pool's replay does, so NTSC and
  PAL match), with pinned captures and telemetry. Compare with M0 in a
  table in `docs/WATER_BENCHMARKS.md`.

- [x] **M7 — Docs and packaging.** Done: the sources are in
  `cmake/LibSaturnSources.cmake` and the Makefile, the package consumer source
  includes the new headers (`ctest --preset package-consumer`, which builds and
  installs to a temporary prefix and links the consumer against it, passes 5/5;
  the scratch build was removed afterwards),
  `docs/WATER_RENDERING.md` and the README link the example. M3's host tool is
  `tools/water_prepare.py`, with `tests/tools/test_water_prepare_parity.py`
  comparing it bit for bit against the C implementation on seeded random
  levels.

## Measured results

Ymir only, immediate division, no SH-2 cache emulation. Full tables are in
[Water benchmarks](WATER_BENCHMARKS.md#water-lake); the harness pins the face
counts for four runs (flat and textured water, naive and full) and 12 matched
captures.

- **Cost.** The full draw costs about 6–12 ms more scene preparation per frame
  than the naive one (about 20–26 ms against 14 ms at the start of the route).
  The plane build is 1.7 ms and its submit 1.8 ms; cutting the actor adds about
  1 ms. The prepared level first cost 13.2 ms against 8.1 for the unprepared
  one, because both instances projected every vertex; giving each side its own
  vertex block brought it to 9.5 ms.
- **No clipping win, flat or textured.** Clipped and fallback faces are equal or
  slightly higher with the plane, and no run skips a face: four big quads (or a
  6×6 grid of textured tiles) are handled by the clip path, and a textured face
  behind the near plane is drawn as the flat fallback colour rather than
  dropped. The plan's acceptance criterion of "fewer fallback and dropped faces
  near the camera" is therefore **not met**, and the argument that motivated the
  plane's near rings did not hold on this route. Rerunning the route with the
  water not drawn isolates the water's own share of the clipped faces: 24 for
  the naive quads and 122 for the plane (the rest of the scene clips 630 to
  664), so the plane increases that share instead of reducing it. What the plane does give is
  finer texture detail near the camera (not measured in pixels), a wobble and
  distance tint, and the right painter passes.
- **Arithmetic.** A first plane of 4 rings of 8×8 cells cost 13 ms to build with
  64-bit arithmetic, 5 ms after moving to 32-bit, and 1.7 ms with 4×4 cells.
  The SH-2 has no 64-bit divide, and a 64-bit divide by a 64-bit variable
  compiled to an `abort` call in the example, so the plane uses 32-bit
  positions and refuses worlds past 8192 units.
- **Correctness win.** At matched ticks the naive draw lets the bed paint over
  the water, leaves the waterline uncut and shows the dive untreated; the full
  draw does not. This is what the full draw is for.
- **Open.** A level with far more water faces (where fewer, coarser far rings
  could win), a camera that skims much lower, and physical-hardware timing.

## Outside this repository

The first consumer is `~/dev/sm64sd-saturn`. Once M2–M5 land there:

- **Castle grounds.** Replace the partial-alpha moat faces with a water
  plane. Prepare the castle chunks against the moat region. Cut Mario with
  `sat_water_submit_instance` when he wades.
- **Jolly Roger Bay.** Add a region for the bay, prepare the bay floor and
  cave, use the underwater view while swimming and the regions for
  buoyancy. The ship and eel go through the waterline path.
- **Data.** In the sm64sd-saturn tree (read, not changed) water is not a set
  of boxes: it is floor triangles in the collision data whose CLPS attribute
  word0 carries the water bit `0x20`. `tools/sm64ds_pipeline/level_objects.py`
  (`extract_clps_masks`) already yields the bitmask of such attribute indices,
  `tools/prepare_castle.py` emits it as `water_attrs`, and the runtime
  (`runtime/sm64ds_play.h`) treats those floors as the surface Mario swims on.
  Swimming itself is not implemented there yet (`docs/STAGE_ENGINE_PLAN.md`).
  The converter would therefore group the water-flagged triangles into
  rectangles, one `sat_water_regions` entry per surface height, and mark the
  same faces for the prepared-level split. That converter step is the
  remaining integration work.

This plan does not cover that integration itself.

## Risks

- **Seam gaps.** VDP1 rasterizes each quad independently, so even a planar
  T-junction can leave single-pixel gaps. M2 checks captures before deciding
  whether an overlap is needed.
- **Fill cost.** A lake can cover most of the screen. VDP1 still walks every
  pixel of a mesh polygon. Measure the VDP1 draw wait, not only CPU
  preparation, at the low skim camera.
- **Texture scale at ring borders.** Coarser rings need their own baked
  texture. Without one, the pattern doubles in size at a visible line.
- **Textured level faces.** Many crossing faces in a textured level stay
  uncut at run time, so M3's offline split in the converter matters more
  than the run-time one.
- **Several planes.** Exact ordering covers one plane per frame. A frame
  showing two water bodies at different heights, such as a fountain beside a
  moat, sorts the second one by depth only.
- **Mesh on emulators.** Mesh shows as a checkerboard in sharp emulator
  output and blends on a composite display. Captures will show the pattern.

## Acceptance

- Host: `gmake test` passes with the new tests.
- Ymir: the lake harness passes with pinned captures, and the replay is
  identical in NTSC and PAL. Compared with M0 on the same route: no
  misordered water against the banks and the bridge, and preparation time
  reported for each stage. The first draft also required fewer fallback and
  dropped faces near the camera; the measurements did not show that (see
  "Measured results"), so it is recorded as not met rather than claimed.
- Mednafen boots the example and shows the same scene.
- Existing examples, Water Lab 9/9 and Water Pool 8/8 harnesses, and the
  package consumer still pass.
- Physical Saturn acceptance is recorded separately and not claimed without
  hardware evidence.

[Water rendering](WATER_RENDERING.md) · [Water Lab plan](WATER_LAB_PLAN.md) · [Water benchmarks](WATER_BENCHMARKS.md) · [Transparency and alpha](TRANSPARENCY_ALPHA.md)
