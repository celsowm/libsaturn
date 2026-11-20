# Water rendering

The Water Lab puts the surface, solid scenery and moving objects into the same
bounded scene painter. A shared sine sample determines each grid vertex.
Subdivision is selectable at 4×4, 8×8 or 16×16. The example bounds dry faces to
512 and allocates 1024 caller-owned face records for dry, split and water
geometry. It reserves a separate command quota for the HUD.

`sat_scene3d_material_t.sprite_flags` carries the VDP1 effect flags for RGB
materials. Projected and clipped emission both keep the flags, including for
Gouraud material. The compact opaque command path remains available at zero
flags. VDP1 mesh covers alternating pixels. RGB half-transparency averages an
RGB source with a previously drawn RGB destination. Over indexed destinations
VDP1 uses the new RGB source. Shared edges can be written twice and the
painter's depth key cannot resolve every intersecting polygon. Both
limitations can be seen by comparing the native modes on the same tick.

`sat_water_split_quad` cuts a convex planar solid face at a fixed Y plane.
Each resulting piece carries its dry or submerged classification, and
reversed shared edges produce the same 16.16 intersection. Water Lab applies
Gouraud depth tint and moving caustic light to submerged pieces. A small
depth-dependent horizontal shear vanishes at the waterline. The existing
scene clipper then clips the adjusted geometry against the camera near
plane. The wholly submerged mosaic uses a preloaded cool palette.
Cross-waterline indexed textures use their ordinary path and are
counted, including the temple's mosaic seal.

`sat_water_volume_sample` is a pure query on an axis-aligned volume. Use
`sat_water_volume_apply` to blend velocity toward the current and add
buoyancy in proportion to immersion. The camera uses point hysteresis
independently of the player's partial immersion. This example uses a flat
gameplay surface and cosmetic waves in the native modes.

[Water Lab](../examples/water_lab/README.md) · [Transparency and alpha](TRANSPARENCY_ALPHA.md) · [Water geometry](../include/saturn/water.h) · [Water volume](../include/saturn/water_volume.h)
