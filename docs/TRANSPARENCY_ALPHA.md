# Transparency and alpha blending on Saturn

Status: VDP1 native effects, discrete 2D shape alpha and VDP2 indexed-sprite
alpha via eight shared hardware ratio slots are implemented. Automatic
per-pixel RGBA asset conversion is not implemented; do not infer shader
semantics from the presence of `SAT_BLEND_ALPHA` in an enum.

## Four distinct mechanisms

1. **Color key**: for indexed8 sprites, pixel index 0 can remain transparent,
   unless `SAT_SPRITE_FLAG_OPAQUE` turns on SPD. Importers reserve index 0
   when the source image requires transparent pixels.
2. **VDP1 mesh**: `SAT_SPRITE_FLAG_MESH` sets CMDPMOD bit 8; hardware leaves
   alternating framebuffer pixels untouched, producing a fixed grid rather
   than RGB mixing. It works with palette sprites and RGB polygons.
3. **VDP1 RGB-only arithmetic**: `SAT_SPRITE_FLAG_HALF_TRANSPARENT` sets
   CMDPMOD mode 3 (or mode 7 with Gouraud). When a previously drawn pixel in
   the VDP1 framebuffer has RGB-code MSB=1, its value is averaged with the
   new RGB color. If the destination pixel is transparent/MSB=0, the new
   color is **replaced**, and the VDP2 background is NOT blended.
   `SAT_SPRITE_FLAG_HALF_LUMINANCE` sets mode 2 (mode 6 + Gouraud): half
   brightness of the source, not alpha. Both require an RGB-coded
   polygon/polyline/line color (bit 15 set); the indexed8 palette-sprite
   path explicitly returns SAT_ERR_UNSUPPORTED for either flag.
4. **VDP2 sprite color calculation**: `sat_vdp2_sprite_color_calc_configure`
   and the three `sat_draw_sprite*_color_calc` functions combine an indexed
   sprite with a lower-priority VDP2 layer via one of eight shared
   5-bit color-ratio registers. This is the mechanism used by
   `distance_fade_3d`. It does not alpha-blend two arbitrary overlapping
   VDP1 primitives.

## Shape alpha API

```c
sat_rect_t rect = {8, 8, 32, 32};
sat_fill_rect(&rect, sat_color_rgba(255, 0, 0, 255)); // opaque RGB
sat_fill_rect(&rect, sat_color_rgba(0, 0, 255, 128)); // VDP1 RGB 50%
sat_fill_rect(&rect, sat_color_rgba(0, 0, 255, 0));   // no draw
```

Shape alpha supports only 0, 128, 255; values such as 32 or 192 return
SAT_ERR_UNSUPPORTED, rather than silently rounding. Transparent-background
and overlapping-sprite caveats apply. `sat_draw_params_t.flags` accepts
`SAT_SPRITE_FLAG_MESH` on indexed textures; it is a checkerboard, **not**
ordinary per-pixel alpha. Direct VDP1 callers can set the three native effect
flags on polygon commands, including Gouraud variants.

## High-level texture alpha (VDP2, not VDP1-on-VDP1)

```c
#include "saturn/vdp2_color_calc.h"
sat_vdp2_sprite_color_calc_configure_alpha(6u); /* call once, after sat_init */
sat_draw_params_t p = sat_draw_params_default();
p.blend_mode = SAT_BLEND_ALPHA;
p.tint.a = 128u;
sat_draw_texture(texture, NULL, &dst, &p);
```

The convenience preset uses ratios 0,4,8,12,16,20,24,31 and reserves sprite
priority selector 1 for color calculation. Priority selector 0 remains opaque.
Keep the relevant VDP2 background below both sprite priority selectors.
`sat_draw_texture` chooses the nearest already-configured ratio without
rewriting global state per sprite; it can coexist with the 3D distance-fade
table **if** that table offers a ratio near the requested alpha. The table
has only eight shared entries, so configuring it for 2D alpha may change
the appearance of preexisting distance-fade sprites. No configuration yields
SAT_ERR_NOT_INITIALIZED and missing ratios yield SAT_ERR_UNSUPPORTED.
Alpha 0 skips and 255 draws normally, independent of VDP2 config.

VDP2 color calculation cannot blend one VDP1 sprite over another sprite
already in the same VDP1 framebuffer. This path is for the lower-priority
VDP2 background and supported layer arrangements.

## Constraints and later work

- Source color calculation can take several times as long as simple replace
  (the Sega VDP1 manual documents around six times for its half-transparency
  path). Keep large transparent regions and overdraw bounded.
- No depth buffer, order-independent transparency or portable RGBA8 pipeline.
- Preserve opacity/color-key semantics when converting image/model assets.
- A later generalized rendering API must distinguish VDP1 RGB-on-RGB
  compositing from VDP2 sprite-on-background color calculation.
- Manually test overlapping objects, transparent framebuffer erase, palette
  sprites, clip rectangles, Gouraud and VDP2 backgrounds in emulator and on
  hardware, not just CMDPMOD bits.

References already vendored in this repo:
`docs/sega_saturn_hardware/hard/vdp1/hon/p06_30.md`,
`p06_33.md`, `p06_37.md`, and
`docs/sega_saturn_hardware/hard/vdp2/hon/p12_10.md`.

## World-space polygon effects

`sat_draw_world_polygon_effects(view_proj, quad, RGB, flags)` and
`sat_draw_world_polygon_gouraud_effects(..., flags)` propagate VDP1 mesh,
half-transparency and half-luminance flags through existing 3D projection.
The preprojected variants are `sat_draw_quad2_polygon_effects` and
`sat_draw_quad2_polygon_gouraud_effects`. No fake arbitrary alpha is offered:
the half-transparency mode is 50% against an **earlier RGB VDP1 pixel** only.

For multiple translucent quads, submit the opaque surfaces first, then
translucent surfaces from back to front. Sort across separate meshes yourself:
`SAT_MESH_SORT` orders the faces of one mesh, not all objects globally.
Indexed8 textured meshes can use `SAT_SPRITE_FLAG_MESH`; their VDP1
half-transparency is unsupported because the palette sprite mode is not RGB.
