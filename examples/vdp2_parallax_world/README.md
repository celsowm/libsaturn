# VDP2 Parallax Coast (E10)

An original dawn coast drawn by four NBGs and RBG0: a per-line sky gradient, a
cloud bank, mountain silhouettes, a repeating RBG0 sea, a line-scrolled shore
and foreground leaves. The SH-2 only advances tables and the rotation
parameters; no background pixel is computed on the CPU.

```powershell
.\build-example.ps1 vdp2_parallax_world
.\run-example.ps1 vdp2_parallax_world
.\harness\run-vdp2-parallax-world.ps1 -Bios bios\saturn_bios_us.bin
.\harness\run-vdp2-parallax-world.ps1 -Bios bios\saturn_bios_us.bin -Long
```

| Input | Action |
| --- | --- |
| A / R | Toggle the pre-rendered VDP1 reference image |
| START | Exit |

The example hides the VDP2 layers around frames 600-660 and draws a
comparable 320x224 background from one indexed8 VDP1 texture. The HUD and
telemetry report the per-field table-update cost and the VDP1 command count
of both paths. The reference is a static image: it measures the cost of
building one comparable frame in software, not a claim that VDP1 is free.

## What is hardware-side

- `sat_vdp2_back_screen_set_lines`: per-line sky.
- NBG3 cloud banks, clipped above the horizon by window W0; NBG2 mountains.
- RBG0 repeating sea through rotation parameter set A; the map's transparent
  rows expose the sky.
- NBG1 shore with `sat_vdp2_line_scroll_fill_wave`; NBG0 foreground leaves
  with a second, slower line-scroll table.
- Add-mode colour calculation on RBG0 (sea over sky) and NBG1 (shore over the
  line-colour screen).

## Evidence and limits

The harness checks the layer manager's verdicts, the refused line scroll and
undefined window, VDP1 background command accounting, the reference path, and
pixel families for sky, sea, shore and leaves. `-Long` runs the 60-second
(3,600-field) replay. Measured on the pinned Ymir revision: about 3 ms per
field for the VDP2 table updates and a one-time 138 ms software build for the
reference image on the harness host. Those timings are emulator-side only.
Mednafen 1.32.1 was also launched through `run-example.ps1`; its window
capture shows the live coast at frame 310 with 2 VDP1 commands. Physical
hardware remains **PENDING**.
