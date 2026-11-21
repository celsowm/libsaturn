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

`sat_water_split_quad_shaded` makes the same cut and also carries one RGB555
Gouraud color per corner. A cut corner blends each 5-bit channel along the
edge with the same fixed endpoint order as the position, so reversed shared
edges agree. Submit each piece with a material whose `vertex_gouraud` points at
the piece's four colors. This lets a shaded solid, such as a lit sphere, cross
the waterline without losing its shading. Textured faces still need their UVs
interpolated and are not covered.

`sat_water_volume_sample` is a pure query on an axis-aligned volume. Use
`sat_water_volume_apply` to blend velocity toward the current and add
buoyancy in proportion to immersion. The camera uses point hysteresis
independently of the player's partial immersion. This example uses a flat
gameplay surface and cosmetic waves in the native modes.

## Wave simulation

`saturn/water_sim.h` holds a caller-owned square height field with no graphics
dependency. Each step exchanges height between neighbours through an
odd-symmetric flux with reflective edges, damps velocity and removes the
velocity mean, so the summed height is exactly unchanged. `sat_water_sim_bump`
adds an exact volume around a point with a smooth kernel, even where an edge cuts it, and
`sat_water_sim_displace` raises the water at a body's old position and lowers
it at the new one.

`sat_water_sim_shade` and `sat_water_sim_caustics` turn slopes and curvature
into VDP1 Gouraud entries at each node. `sat_water_sim_sample_volume` is
`sat_water_volume_sample` with the surface following the waves, so buoyancy
tracks them. Reflection, refraction and caustics are therefore per-vertex
approximations: VDP1 half-transparency is a fixed 50/50 blend and Fresnel can
only shift color. The [Water Pool](../examples/water_pool/README.md) example uses
all of it. It draws the surface as mesh, because a distorted half-transparent
quad plots some pixels twice and a wave mesh then shows a double-blended
hatch, and it bends the floor against the surface slope for refraction.

## Big water

For levels with large bodies of water the SDK has five small pieces; the
[Water Lake](../examples/water_lake/README.md) example uses them together and the
[plan](BIG_WATER_PLAN.md) records the design and the measurements.

- `saturn/water_regions.h`: several water boxes with their own surface height,
  and queries for the region over a point, a sampled segment and the camera.
- `saturn/water_plane.h`: a horizontal surface as camera-centred rings of
  quads, finer near the camera, with an optional wobble that fades to flat at
  the finest ring's edge. Positions are 32-bit, so the world and the rings
  must stay within 8192 units.
- `saturn/water_prepare.h`: classifies a static mesh against the regions at
  load, cuts faces that cross the surface when their material is plain RGB,
  tints submerged vertices by depth, and lists the above faces before the
  below ones so a game draws them as two instances in two passes. A textured
  face cannot be cut and joins the side holding its middle.
- `saturn/water_actor.h`: the same split per frame for a moving mesh. Only the
  faces that cross the surface are cut.
- `saturn/water_view.h`: the camera's region with hysteresis, a fade of a VDP2
  colour offset over a fixed number of frames, and the painter-pass order,
  which mirrors under water.

Draw order is exact for one plane at a time: submerged geometry, then the
water, then everything above. With the camera under the surface the order
mirrors. A frame with two bodies of water at different heights sorts the
second by depth only.

[Water Lab](../examples/water_lab/README.md) · [Transparency and alpha](TRANSPARENCY_ALPHA.md) · [Water geometry](../include/saturn/water.h) · [Water volume](../include/saturn/water_volume.h)
