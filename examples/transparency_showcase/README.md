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

A cycles sprite alpha 64/128/192. B steps through two more pages. The
second shows the other blend modes:

- Middle: the same sprite with `SAT_BLEND_ADD` -- sprite + NBG0 per channel,
  saturating (VDP2 colour-calculation add mode, CCCTL CCMD). Add and ratio
  alpha cannot share a frame: the mode is one bit for the whole screen, and
  the second of the two in one frame returns `SAT_ERR_BUSY`.
- Lower left: the sprite with `SAT_BLEND_SUBTRACT` -- the VDP1 shadow. It
  draws nothing itself; the RGB rectangle under its opaque texels drops to
  half brightness, while the part over bare NBG0 is untouched (shadow only
  darkens RGB framebuffer pixels).

The third page applies VDP2 colour offset A = -64 to NBG0 alone
(`sat_vdp2_color_offset_set` / `_enable`): every background pixel loses 64
per channel, clamped at 0, while the VDP1 rectangles keep their colour.

START exits.

Build: `make IP_TEMPLATE_KIND=sbl EXAMPLE=transparency_showcase all`.
Run in a Saturn emulator with your own BIOS, then compare with real hardware.
The Ymir harness can run the built example, but it does not automatically
judge visual correctness without an explicit capture/acceptance fixture.

Caveats: only the shape 50% path blends two VDP1 RGB surfaces; VDP2 alpha
does not blend two overlapping VDP1 sprites. Neither effect is per-pixel RGBA.
Read `docs/TRANSPARENCY_ALPHA.md` before using these APIs in a renderer.
