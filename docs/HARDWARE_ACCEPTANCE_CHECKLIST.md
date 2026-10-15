# Hardware acceptance checklist

For a real Saturn (NTSC or PAL). Everything below was run on the Ymir probe
(harness/) and on Mednafen where the table says so; none of it has run on
hardware. Emulators do not model bus timing, DSP and DMA speed, the CD drive or
the analogue output faithfully, and neither emulates the MPEG card or the
NetLink at all. This list says what to run, what you should see or hear, what
counts as a pass, and what to write down.

## Getting a disc

```powershell
.\build-example.ps1 -Example <name>      # -> build\examples\<name>.bin and .cue
```

Copy or burn the matching `.bin` and `.cue` together (a Saturn-bootable disc
or an ODE such as Saroo/Satiator/Rhea/Phoebe). The `.bin` is a raw
`MODE1/2352` track with EDC/ECC; the similarly named `.iso` is only the
2048-byte/sector intermediate for inspection and harness use. The examples boot through the
regular IPL, need no cartridge unless the table says so, and print their
counters on screen. Use a fresh backup RAM / cartridge for the save items: they
format and erase.

Where a row says "record", write the value down next to the date, the console
model (VA0/VA1/VA2...), the region and the video cable. A pass on one console
does not say much about another.

## Acceptance matrix

`Ymir` = probe wrapper passes in NTSC and PAL; `Med` = also checked on Mednafen
(by screenshot or recording) — `-` means the check needs hardware Mednafen does
not model.

| # | Subsystem | Disc | Setup | Expect | Pass when | Emulator evidence |
|---|-----------|------|-------|--------|-----------|-------------------|
| 1 | Video standard and time base (limitation 23) | `irq_demo` | PAL and NTSC console; nothing in the slot | `BUSY 90 VBLANKS MS` is the busy wait's duration; `SCU INTERRUPTS: ACTIVE` | The wait reads 1500 ms on NTSC (90 frames at 60 Hz) and 1800 ms on PAL (90 frames at 50 Hz), to within 20 ms. Counters for VBLANK-IN/OUT and TIMER0 climb at the frame rate. | Ymir NTSC + PAL |
| 2 | Auto standard | `scsp_dsp_demo` | Both consoles | Audible: click, echo; timing in milliseconds | Echo spacing sounds like 100 ms on both (same as on the emulator); not sped up on PAL | Ymir NTSC + PAL |
| 3 | SCU DMA / SH-2 DMAC | `dma_demo` | - | Every route line reads `SAME AS CPU`; `CACHE OK` | All `SAME AS CPU` and `CACHE OK`. Record `CPU MS` / `DMA MS` (the emulator's speed-ups are not expected to hold) | Ymir |
| 4 | SCU DSP | `scu_dsp_demo` | - | `MISMATCHES 0` for both forms | Zero mismatches. Record `DSP TICKS`, `DMA TICKS`, `SH2 TICKS` (emulator DSP timing is approximate) | Ymir, Med (mismatch count) |
| 5 | Slave SH-2 scheduling | `parallel_sched_demo` | - | `ORDER`, `BATCHED`, `FOR MATCH` lines OK, `ERRORS 0` | All OK. Record `SIGNALS` | Ymir |
| 6 | Controllers | `input_devices_demo` | Digital pad on port 1; repeat with a 3D Control Pad, a Shuttle Mouse and a multitap if you have them | `P1 KIND` names the device; axes/mouse `DX` move | The kind is right and inputs register. START+A on port 1 runs the RTC/SMEM round trip: `RTC OK` | Ymir (pad, 3D pad, mouse, multitap) |
| 7 | Backup RAM (internal) | `save_backup_demo` | Back up your saves first | `A = FORMAT` formats; write/read/verify; `PERSISTENCE` after a power cycle | Data is there after a power cycle | Ymir |
| 8 | Backup Memory cartridge | `save_cartridge_demo` | Backup Memory cart in the slot (a blank one) | Cartridge kind detected; write/read/verify; persists | Same as 7, on the cartridge; internal RAM untouched | Ymir (fixture image) + Med |
| 9 | RAM expansion cartridge | `ram_cart_demo` | 1 MB, then 4 MB, then no cart | `CART TYPE MB` 1 / 4 / `NO EXPANSION`; `BANK CROSSING: PASS` | Correct type and capacity for each | Ymir + Med |
| 10 | A-Bus layer | `abus_demo` | Each cartridge above | `KIND` matches; `READBACK OK` on RAM carts; writes refused on a backup cart | As stated | Ymir + Med |
| 11 | MPEG card (UNVERIFIED) | `expansion_probe_demo` | Run once without the card, once with it | Without: `MPEG PRESENT 0`, `START STATUS 9`. With the card: `MPEG PRESENT 1`, `MPEG VERSION` non-zero, `START STATUS 0` | Both lines as stated. **If present reads 0 with the card installed, or start fails, record the CD Block's answers** (`g_expansion` in the map: hardware flags/version, MPEG version, authentication status). The command layouts come from an emulator core and may differ | Host model only; absent path Ymir + Med |
| 12 | NetLink / UART (UNVERIFIED) | none yet | - | There is no NetLink address in the library. To try: write a small program that calls `sat_uart_open_mmio` with the cartridge's UART base and stride | Open succeeds (`SAT_OK`), `fifo` reads 1, a byte written comes back in loopback. **Do not guess addresses on a console with a CD Block or a backup cartridge attached**: a write to the wrong window can corrupt them | Host model only |
| 13 | 68000 sound driver | `sound_driver_demo` | Audio out | `RUNNING 1`; `TICK` advances ~172 per second; `EXECUTED 11`; `MAX LATE` 0 or 1 | Tick rate 172 +/- 1 per second on NTSC and on PAL (it is a sample-rate timer, not a frame one); `MAX LATE` <= 1. Record `TICK` at 10 s intervals | Ymir NTSC + PAL |
| 14 | SCSP DSP effects | `scsp_dsp_demo` | Audio out, headphones | Repeating sequence every 3 s: a click with an echo (100 ms, decaying by half) then a click with a metallic reverb (four short echoes) | Echoes are clean (no crackle, no DC pop when the reverb starts), evenly spaced at 100 ms. The reverb start must not make a click of its own (a half-loaded DSP program does) | Ymir NTSC + PAL, Med (recording) |
| 15 | CD streaming music (limitation 23) | `cd_streaming_jukebox` | Audio out | Three public-domain melodies; `CACHE HITS/MISSES/FILLS`, `PREFETCH OK` | Five minutes with no dropout or stutter, on NTSC and on PAL, with a real drive (the emulator reads instantly). Record whether seeking (per the on-screen help) glitches. Also press Y after "NEXT READY" (the prefetched standby, `sat_music_prefetch`) and record whether the playing track glitches while "NEXT FILLING" (the prefetch moves the laser) and whether the swap is gapless. Also run the default (`JUKEBOX_DISC_STREAM=1`) build and record the HUD `MAX POLL US`, `MAX DELIVER US`, `WORST FRAME MS` and `MIN BUFFERED`: they are the first real-drive numbers for `docs/sega_saturn_hardware/hard/cdblock/index.md`. Then run `bullet_hell_stress` (and `BH_MUSIC_LIB=1`) through a stage change: the manual's average seek (500 ms at 2x) is longer than the music reserve, so this is where a hardware underrun would show | Ymir NTSC + PAL |
| 16 | Audio examples | `audio_showcase` | Audio out | A plays a sound effect, B a burst of them, C music | Balanced levels, no clipping, no dropped voices in the burst | Ymir |
| 17 | VDP2 layers | `vdp2_layers_demo` | - | Four scrolling layers, each in its own colour, text on top | All four move at their own speeds; no flicker or tearing | Ymir NTSC + PAL |
| 18 | VDP2 raster effects | `vdp2_effects_demo` | - | Wobbling curtain (line scroll), column-shifting stripes, per-line back colour | Smooth, no line shimmer | Ymir NTSC + PAL |
| 19 | VDP2 composition | `vdp2_compose_demo` | - | Window shapes cut the layers; mosaic; colour calc blends | Shapes match the description at the top of `main.c` | Ymir NTSC + PAL |
| 20 | VDP1 blending (limitation 12) | `transparency_showcase` | A cycles alpha, B pages | RGB blend, ADD, SUBTRACT (shadow), colour offset, tint | Each page shows what its label says; ADD/SUBTRACT pages leave the ratio alpha off | Ymir + Med |
| 21 | Frame budget ledger (limitation 33) | `dino_demo` | - | Walking T-Rex | Count frames over 60 s with a stopwatch: **19.5-20 fps** is the emulator figure; record the hardware number. HUD text must stay put | Ymir (frame counter) |
| 22 | Skybridge platformer | `skybridge_3d` | Pad | Playable level | Runs without hangs for 10 minutes; record the perceived frame rate. The MASTER-only and MASTER+SLAVE builds should look identical | Ymir (both, hash parity) |
| 23 | Two-CPU workload | `parallel_runtime` | A cycles the backend (Master, Slave-assisted) | The header names the backend; object and face counts stay the same | Same picture on both backends; record the timing the screen shows for each | Ymir |
| 24 | VDP2 8bpp tiles past 511 | `vdp2_tile_overflow` | - | A colour ramp over 960 distinct 256-colour tiles on two NBGs, a checkerboard behind (all three scrolled by -3,-4) | The ramp is smooth from green to magenta with no jump, no black cells after the first four, and a white cell at the bottom right | Ymir NTSC + PAL |
| 25 | VDP2 darkening limited to a window | `vdp2_window_dim` | - | Seven colour bands with two swinging wedge-shaped beams; a black colour-calculation NBG covers everything outside the beams, at half then a quarter brightness | The bands keep full colour inside the beams and are dimmed outside them, with a clean edge that follows the wedges, and the dimming deepens at frame 90 | Ymir NTSC + PAL |
| 26 | VDP2 light on hi-res sprites | `vdp2_hires_light` | A light on/off, B per-code mask on/off, C window on/off | 640x224 warm-ramp columns; a red elliptical spot holds, then sweeps left and right | Red is added only inside the spot, fades above and below its centre line, leaves the middle stripe and the blue background untouched; with B off the stripe is lit too | Ymir NTSC, Mednafen |

## Things no emulator settles

- **Timing.** The DMA, DSP and SCSP speeds above; the frame rates of 21 and 22;
  the CD drive's seek and read times for 15.
- **Audio quality.** Emulators resample and clip differently; rows 13-16 want
  listening on the real output.
- **Real cartridges.** Rows 8-12 cover parts the emulators approximate with a
  file (a backup fixture, a RAM cart mode) or omit entirely (MPEG, NetLink).
- **PAL 240/256-line modes** are not offered by the library; a PAL console runs
  the 224-line mode at 50 Hz with borders. That is by design, not a defect to
  report.

## When something fails

Note the row, what you saw, and the on-screen counters. For a row that has a
`Ymir` wrapper the same disc can be run under the probe to compare
(`harness\run-<name>.ps1`; `harness\run-acceptance-sweep.ps1` runs them all,
`-Pal` for the PAL standard). The counters live in a `g_*` struct in the map
file if a debugger or a cartridge dump is at hand.

## Status of the limitation list

| Limitation | State |
|------------|-------|
| 23. Audio and frame timing on PAL | Closed on the emulators (Ymir NTSC and PAL, all audio and frame-timing rows); **pending rows 1, 2, 13-15 on a real PAL console** |
| 27. Backup / expansion cartridges | Closed on the emulators; **pending rows 7-10 with real cartridges** |
| 33. Frame budget on the real machine | Closed on the emulator ledger; **pending rows 21-23 on hardware** |
| MPEG card, NetLink (part of the hardware subsystems) | Drivers host-tested only; **rows 11-12 need the devices** |


## Skeletal character family: pending physical acceptance

Build and burn the viewer, arena and stress examples with the stock memory
configuration. Prefer `CHARACTER_DEMO_DISC_ASSETS=1` for an arena disc-loading
check; remove the setting for comparison with embedded assets. Record the
console revision, region/video rate, BIOS, disc medium and build commit.

- Viewer: exercise all nine clips, crossfade, speed, pause, camera, skeleton and
  both shading modes. Inspect palette ownership, face detail, near clipping,
  attachment alignment and stable texture residency.
- Arena: hold movement into the opponent, then sidestep around it. Check walls,
  jump, guard, hit reaction, knockdown, timer and repeated Start reloads. Inspect
  limbs in front of the other fighter and stage/shadow painter ordering.
- Stress: run 1/2/4/8 actors in MASTER, SLAVE and AUTO; compare posed output and
  completed-frame diagnostics. Check all accepted counts, command reserve and
  bounded storage. Record simulation ticks, VBlanks, submission counts, FRT
  costs, stack high-water mark and cache/worker errors over sustained runs.
- Repeat timing and accumulator checks at PAL 50 Hz. Confirm CD reads and
  resource releases finish before storage is reused; check both costume CRAM
  banks and the indexed/RGB shading behavior on physical VDP1 output.

All of these physical checks remain pending. NTSC and PAL character emulator
checks now pass; see the [presentation/PAL report](CHARACTER_PRESENTATION_ACCEPTANCE.md).
The existing NTSC Ymir/Mednafen
evidence and measured display-rate limitations are in
[FIGHTING_GAME_READINESS.md](FIGHTING_GAME_READINESS.md).

## Optical boot diagnostics and native promotion

Use [optical_emulators_tester](../examples/optical_emulators_tester/README.md)
minimal/full packages and fill in their generated `hardware-results.csv`.
Record Satiator and SAROO separately, including exact image hashes, BIOS/console,
firmware/menu, SD format, cold/reset launch and visible stage/register evidence.
Run SAROO direct-BIN versus disc mode with identical application bytes; a direct
load does not prove IP compatibility. Preserve both failures and passes.

The native profile stays experimental until the same artifact achieves ten
consecutive cold boots on each ODE, reset checks, minimal/full diagnostics,
a separate example and five-minute Dino stability on both. Repeat progress-bar
on/off and record CD/audio initialization. Missing pre-video evidence is UNKNOWN;
a licensed-screen freeze alone cannot identify the failed stage. See the
[native boot plan](NATIVE_SATURN_BOOT_PLAN.md) for the complete acceptance matrix.
