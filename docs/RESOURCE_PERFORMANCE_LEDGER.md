# Current resource and performance ledger

This is a measured baseline→refactor comparison collected with the same
`build-example.ps1` and toolchain on 2026-09-20. Baseline artifacts were built
from commit `2dbbd94` in a temporary clean worktree; refactor artifacts are the
current working tree. ELF section sizes come from `sh2eb-elf-size -A`;
`WRAM-L` is the linker section size, not free memory. Values are
`baseline → refactor (delta)`.

| Example | `.text` | `.data` | `.bss` | `WRAM-L` | ELF total |
| --- | ---: | ---: | ---: | ---: | ---: |
| `skybridge_3d` | 295,880 → 299,892 (+4,012) | 44 → 44 (0) | 165,916 → 169,460 (+3,544) | 804,852 → 819,876 (+15,024) | 1,304,784 → 1,327,364 (+22,580) |
| `pacman_3d` | 182,028 → 194,324 (+12,296) | 56 → 56 (0) | 304,396 → 320,348 (+15,952) | 578,620 → 578,620 (0) | 1,099,583 → 1,127,831 (+28,248) |
| `infinite_explorer` | 769,888 → 781,608 (+11,720) | 40 → 40 (0) | 159,556 → 203,272 (+43,716) | 570,428 → 570,428 (0) | 1,538,004 → 1,593,440 (+55,436) |
| `runtime_2d` | 71,440 → 71,952 (+512) | 44 → 44 (0) | 136,104 → 136,104 (0) | 570,428 → 570,428 (0) | 812,499 → 813,011 (+512) |
| `runtime_3d` | 122,240 → 132,272 (+10,032) | 36 → 36 (0) | 138,288 → 164,380 (+26,092) | 570,428 → 570,428 (0) | 865,475 → 901,599 (+36,124) |
| `cd_streaming_jukebox` | 68,292 → 69,292 (+1,000) | 36 → 36 (0) | 129,516 → 129,756 (+240) | 570,428 → 570,428 (0) | 802,755 → 803,995 (+1,240) |
| `save_backup_demo` | 66,264 → 67,336 (+1,072) | 36 → 36 (0) | 153,152 → 153,164 (+12) | 570,428 → 570,428 (0) | 824,363 → 825,447 (+1,084) |

The modified Ymir PC sampler also ran 120 program frames for `skybridge_3d`
(73 unique sampled PCs) and `pacman_3d` (100 unique sampled PCs). These are
sampling observations, not cycle counts; they are retained as a reproducible
qualification signal rather than presented as frame-time measurements.

For `runtime_3d`, the modified Ymir debug tracer ran the same 90 BIOS + 180
program-frame scenario against both artifacts. Master SH-2 instruction counts
over the 180 program frames were `43,273,727 → 46,077,313` (`+2,803,586`,
`+6.48%`), with the slave SH-2 disabled in both runs. The emulator's absolute
system-cycle budget was identical at `80,856,720` in both runs (449,204/frame
on average); this is a fixed frame timing budget, not a CPU-work measurement.
The instruction count is therefore the useful CPU-work signal here, and the
delta is recorded as an accepted cost of the new runtime ownership layers until
the update/render hot path is further optimized.

The same modified-Ymir VDP write counter was then run against baseline and
refactor artifacts with identical BIOS, boot frames and program-frame scripts.
Values below are total 16-bit words submitted during the program frames, in the
order `VDP1 VRAM / VDP2 VRAM / VDP2 CRAM`:

| Example / scenario | Baseline | Refactor | Delta |
| --- | ---: | ---: | ---: |
| `runtime_3d`, 180 frames | 355,350 / 176 / 24,832 | 355,350 / 176 / 24,832 | 0 / 0 / 0 |
| `skybridge_3d`, 120 frames | 108,778 / 103,858 / 26,416 | 111,482 / 103,858 / 26,416 | +2,704 / 0 / 0 |
| `pacman_3d`, fixed 120-frame probe window* | 181,802 / 36,865 / 24,832 | 212,810 / 36,865 / 24,832 | +31,008 / 0 / 0 |
| `pacman_3d`, 16-angle script, 340 frames | 1,498,090 / 36,865 / 24,832 | 1,524,826 / 36,865 / 24,832 | +26,736 / 0 / 0 |
| `pacman_3d`, no-input gameplay-aligned tail, 27 frames | 172,080 / — / — | 171,712 / — / — | **-368 / — / —** |
| `pacman_3d`, 16-angle gameplay-aligned tail, 247 frames | 1,488,400 / — / — | 1,485,424 / — / — | **-2,976 / — / —** |
| `infinite_explorer`, 120 frames | 386,770 / 105,202 / 26,112 | 386,770 / 105,202 / 26,112 | 0 / 0 / 0 |
| `runtime_2d`, 120 frames | 189,818 / 113 / 256 | 189,914 / 113 / 256 | +96 / 0 / 0 |
| `cd_streaming_jukebox`, 240 frames | 292,618 / 80 / 24,576 | 288,906 / 79 / 24,576 | -3,712 / -1 / 0 |
| `save_backup_demo`, 120 frames | 180,490 / 114 / 24,576 | 180,490 / 114 / 24,576 | 0 / 0 / 0 |

The counter is attached at Ymir's central VDP memory-write boundary, so it
counts submitted words rather than only final words that differ in a snapshot.
*The fixed-window rows are intentionally retained as an audit warning, not as
an apples-to-apples performance claim: the refactor's bounded `sat_view_cache`
sort completes the startup bake about five program frames earlier, so that
window contains extra gameplay frames and its scripted button edges reach
different startup states. The aligned rows shift the baseline-only startup
offset and shift the baseline input script by the same five frames; they compare
the same gameplay tail. Both aligned comparisons are lower for the refactor
(-0.21% and -0.20%), and the inspected 320×224 captures retain the complete
maze, actors, pellets and HUD.

The newly canonicalized `physics_3d` route was also sampled independently in
the modified Ymir for 120 program frames: `215,610` VDP1 VRAM words, `59`
VDP2 VRAM words and `24,576` CRAM words. This is a current bounded-scene
observation, not a baseline delta; the B-triggered capture retained all balls
and the HUD after moving floor rendering into the explicit background pass.

The `distance_fade_3d` scene migration was sampled for 180 program frames as
well: `340,890` VDP1 VRAM words, `6,145` VDP2 VRAM words and `25,088` CRAM
words. The two inspected captures show the stepped fade slots and HUD; this is
also a current observation rather than a baseline delta.

The post-cutover camera/cache ownership changes were re-run in modified Ymir
after the final rebuild. `runtime_3d` used `494,278` VDP1, `176` VDP2 and
`24,832` CRAM write words over 180 program frames, with `55,990,492` master
SH-2 instructions and the slave disabled; its 320×224 orbit capture retained
the model and HUD. `pacman_3d` used `1,524,826` VDP1, `36,865` VDP2 and
`24,832` CRAM write words over the 340-frame 16-angle replay; inspected frames
90, 180 and 300 retained the maze, pellets, actors and HUD. These are current
emulator observations, not stock-Saturn timing claims.

The rebuilt Skybridge binary was then exercised through the modified-Ymir
course grid: a Course 1 movement/jump replay captured the fixed-second-platform
regression at frames 160/185/212, while Course 2's elevator presentation,
Course 3's real hole piers and Course 4's tilting-ramp presentation all
produced 320×224 captures with the pig, sea/sky layers and HUD. A camera-only
six-tap replay produced distinct before/after captures while retaining the same
deck and player position.

The optional `ram_cart_demo` acceptance was rerun against the modified Ymir
cartridge models after rebuilding the probe with all pinned Ymir submodules
present. The `none`, `1m` and `4m` configurations all passed detection and
capacity checks, including the eight-byte bank-edge read/write assertion. The
guest screens also reported status `0` and `BANK CROSSING: PASS` for the
expanded-cart modes. RAM cartridges remain optional and are not a core
prerequisite for the runtime refactor.

Runtime counters already available for deeper per-frame qualification:

- `sat_vdp1_command_stats`: used commands, capacity and protected HUD quota;
- `sat_scene_stats`: submitted/flushed/rejected faces and overlay reservation;
- `sat_audio_stream_stats`: buffered frames, refills and underruns;
- modified Ymir: frame-indexed PC samples, VDP1/VDP2 snapshots and SCSP trace.

The ELF comparison, deterministic CPU-work comparison and representative
VRAM/CRAM transfer measurement are complete. Broader input-script coverage and
hardware-specific transfer timing remain outside this emulator ledger; no
performance win is claimed from the fixed emulator cycle budget.
