# Colored light cubes

Two procedural 3D cubes share one LUT4 checker texture and RGB CLUT. The
left cube is the reference. Moving red/blue spotlights apply per-vertex
Gouraud corrections to the right cube using the arena's fixed-point cone
and Lambert calculation. No imported models, audio or mesh coverage is used.

A radial INDEX8 sprite adds red light to the NBG0 checker through VDP2
color calculation. The checker is a simple screen-space test background,
not a perspective floor. The sprite footprint is projected from the 3D
cone/ground intersection. Cubes draw after the footprint and preserve
their RGB pixels; their surface lighting comes from Gouraud.

Controls: A pauses motion, B toggles cube lighting, C toggles the footprint.

```powershell
.\build-example.ps1 colored_light_cubes
.\run-example.ps1 colored_light_cubes -MednafenCart none
.\harness\run-colored-light-cubes.ps1 -Bios bios/saturn_bios_us.bin
```

`SAT_VDP2_SPRITE_ADD` selects saturating addition for native indexed sprite
and 3D material draws. Addition and ratio blending share a screen-global
mode; incompatible draws in one frame return `SAT_ERR_BUSY`. Addition
combines the sprite with the VDP2 image below it; it cannot restore or
accumulate previously overwritten VDP1 pixels.

Primary sources: SEGA's [VDP2 normal color calculation](../../docs/sega_saturn_hardware/hard/vdp2/hon/p12_11.md)
and [sprite priorities](../../docs/sega_saturn_hardware/hard/vdp2/hon/p09_20.md).

Validation: the 360-field stock-RAM Ymir run completes 355 submissions with
zero panics/errors, three reference faces, three LUT4 Gouraud faces sharing
the same texture/CLUT, and one non-mesh additive sprite. The checker is
visible and the two paused captures are identical. B/C toggles are exercised.
Mednafen disc boot is also checked; the capture is
`harness/build/light_cubes_mednafen.png`.
