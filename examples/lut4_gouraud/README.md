# LUT4 Gouraud comparison

A minimal VDP1 color mode 1 example. Both panels share one 64x64 texture
(2,048 bytes) and one 16-entry RGB555 CLUT (32 bytes). The right panel adds
four animated RGB corrections through an eight-byte Gouraud table; pixels
and CLUT stay unchanged. A pauses animation and B resets its phase.

```powershell
.\build-example.ps1 lut4_gouraud
.\run-example.ps1 lut4_gouraud -MednafenCart none
.\harness\run-lut4-gouraud.ps1 -Bios bios/saturn_bios_us.bin
```

The public entry point is `sat_draw_sprite_distorted_gouraud`. It stages the
four corrections, selects Gouraud in CMDPMOD and sets CMDGRDA to the table
address divided by eight. CMDCOLR remains the RGB CLUT address and CMDSRCA
remains the packed character address. Color-bank textures and the 8-bit
high-resolution framebuffer are rejected. Visible CLUT entries must have
the RGB identification bit set.

Primary source: SEGA, *VDP1 User's Manual*, ST-013-R3-061694,
[complete original manual](https://manualzilla.com/doc/5653132/vdp1-user-s-manual).
See the local [color calculation section](../../docs/sega_saturn_hardware/hard/vdp1/hon/p06_37.md)
and [Gouraud table section](../../docs/sega_saturn_hardware/hard/vdp1/hon/p05_30.md).

The Ymir gate checks shared CMDCOLR/CMDSRCA, color mode 1, Gouraud mode 4,
CMDGRDA, distinct corrections and identical paused images. Mednafen also
boots the generated disc and displays both panels. Hardware acceptance is pending.
