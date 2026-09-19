# transparency_showcase

Visual regression/example comparing the three **different** Saturn effects,
with no external assets:

- Left: VDP1 50% RGB compositing over an earlier VDP1 RGB rectangle
  (`sat_fill_rect` with `color.a = 128`).
- Middle: 8-bit indexed sprite with a stepped alpha ratio blended into the
  VDP2 NBG0 tiled background (`SAT_BLEND_ALPHA` + color-calc preset).
- Right: VDP1 mesh/checkerboard; it does not mix colors.

The NBG0 layer is deliberately high contrast so alpha is distinguishable from
source-only brightness changes. The VDP1 erase is transparent, allowing the
VDP2 background to show. The middle sample must reveal NBG0 colors through
the sprite, whereas the left sample only blends two VDP1 RGB primitives.

A cycles sprite alpha 64/128/192. START exits.

Build: `make IP_TEMPLATE_KIND=sbl EXAMPLE=transparency_showcase all`.
Run in a Saturn emulator with your own BIOS, then compare with real hardware.
The Ymir harness can run the built example, but it does not automatically
judge visual correctness without an explicit capture/acceptance fixture.

Caveats: only the shape 50% path blends two VDP1 RGB surfaces; VDP2 alpha
does not blend two overlapping VDP1 sprites. Neither effect is per-pixel RGBA.
Read `docs/TRANSPARENCY_ALPHA.md` before using these APIs in a renderer.
