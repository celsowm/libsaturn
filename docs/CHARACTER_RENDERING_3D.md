# Character rendering in the canonical scene

Skinned meshes use `sat_scene_submit_instance`. Their mutable vertices belong
to each actor; topology, textures and clips can be shared. Character placement
is an instance world matrix. `sat_skin_bounds` supplies current local-space
bounds for whole-character frustum rejection; conservative unions should be
used when a bound must remain valid before deformation. The example computes
exact posed bounds after deformation, so it does not claim pre-skin culling.

All fighters, floor tiles and shadows enter the same face painter. Separate
character draws are not used as an ordering substitute. Materials retain
`SAT_SCENE3D_SLOT_INHERIT`; RGB materials require the opaque slot. The existing
near clipping, projection, backface testing, bucket ordering, texture residency
and command backend remain authoritative.

## Materials and light

`sat_character_materials_upload` creates owners through the existing managed
texture allocator and produces native material bindings for the scene.
Equal palettes share CRAM; different costume palettes remain independent.
Ownership lasts through queued scene work and GPU use. Release with
`sat_character_materials_release` at a confirmed frame boundary. Nothing
uploads texture pixels during ordinary pose updates.

The default original fighters use offline indexed flat panels with three directional baked light variants, one
face-detail panel and independent blue/red palettes. The shading toggle
uses existing vertex-normal and RGB Gouraud lighting helpers with actor-owned
material overrides. Indexed8 Gouraud arithmetic is rejected explicitly;
there is no claim of arbitrary palette-texture per-pixel lighting. Index zero
provides cutout transparency. Other blend techniques require an explicit
supported hardware color mode rather than silent modern alpha emulation.

VDP1 cannot freely interpolate UVs created by arbitrary near-plane clipping.
The material bridge supplies a representative solid-color fallback for that
case. Fully safe projected faces retain their textures and VDP1 system clipping.
This is an explicit approximation, not an invented texture interpolation mode.

## LOD, commands and diagnostics

LOD levels are immutable mesh/skin assets over the same skeleton and clips.
The examples select whole actors' detail before submission, account for stage
and HUD overhead, and report requested/accepted/downgraded/rejected counts.
The native painter checks capacities and reserves the HUD. A scene capacity
failure rejects the frame's character work atomically; simulation continues.

`sat_scene3d_profile_get` exposes last submission/flush FRT costs when metrics
are enabled. It returns UNSUPPORTED when compiled out, rather than advertising
zero as a measured cost. `sat_time_frequency_hz` supplies the calibrated timer
conversion. Submission throughput uses independently observed VBlanks in the
harness; the decimal HUD is a rounded loop rate, not newly presented images/s.
Completed-frame snapshots prevent reading half-updated actor totals.

## Attachments, shadows and limitations

`sat_skeleton_attachment` composes character world, joint world and optional
attachment-local matrices. The skeleton overlay exercises this path and uses
the existing projection and line renderer.

`sat_character_shadow` queues an optional 3..16-triangle contact ellipse with
the canonical scene. It follows each actor's placement and remains on the
ground, with eight commands chosen by the examples. It approximates ground
contact; it is not an exact animated mesh or self-shadow.

Painter ordering has no Z-buffer and cannot exactly resolve arbitrary
interpenetrating textured surfaces. Existing bounded plane splitting is
available for solid geometry; textured fighters use the selected depth-sorted
approximation. No whole-character order is imposed, and overlap does not stop
simulation or animation. The arena separates example-local XZ body circles every fixed tick, with bounded
wall correction. Its hit/hurt volumes are example-local
proximity boxes, not accurate per-triangle collision or a production combat
system. See [fighting-game readiness](FIGHTING_GAME_READINESS.md).
