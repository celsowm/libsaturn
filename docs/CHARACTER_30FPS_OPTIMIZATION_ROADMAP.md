# 30 FPS character rendering roadmap (pre-Tekken 3 port)

Status: **planned / not yet implemented** (2026-10-14). This is a performance-gated roadmap, not a claim that the renderer achieves 30 unique presented FPS with MEDIUM or HIGH character models.

## Goal and constraints

Before attempting a fighting-game port, make the *generic* LibSaturn two-character rendering and asset stack predictable at a sustained NTSC two-field presentation cadence. The deterministic gameplay simulation stays at 60 Hz even if presentation falls behind; never delete simulation ticks, reduce collision precision or silently downgrade visual detail to meet a counter target.

Targets:

- Benchmark three content tiers: (A) the stock-RAM blue/red humanoids in `fighter_arena_3d` / `skinning_stress_3d` at LOW, MEDIUM and HIGH; (B) the two 66-joint characters in `killer_clown_demo`; (C) **real Tekken 3 character models and motion data**, extracted privately from a personally supplied original PlayStation disc using the Tekken3Remake converter. Tier C is the decisive port-readiness benchmark, not a stretch goal that can be replaced by a synthetic substitute.
- Aim for 30 **distinct** presented frames/s on physical NTSC hardware for MEDIUM; investigate HIGH without claiming its attainment in advance. Treat 30 *submissions*/s measured in the emulator as an intermediate gate only.
- Preserve the no-expansion-cartridge configuration, bounded memory, independent 60 Hz simulation, VDP1 command capacity, deterministic gameplay, and stable Master/Slave output.
- No change to the library's fixed-point semantics, interface ABI, cache publication contract, global painter order, texture ownership, or frustum safety without exact regression coverage.
- Keep Tekken-specific logic, proprietary data, move formats and PS1-GTE rounding in a future separate consumer repository. LibSaturn should expose generic primitives only. A **render-only benchmark consumer** may use real Tekken assets before gameplay is ported; this does not authorize bundling proprietary data, copying converter assets into LibSaturn, or claiming a gameplay port.
- Resolve a **persistent local disk/volume with the most usable free space** before downloading any disc archive or copying disc images; all third-party inputs, downloads, conversion intermediates and working caches remain outside the LibSaturn checkout.

## Existing measurements and already completed work

Baseline source: [Character presentation and PAL acceptance](CHARACTER_PRESENTATION_ACCEPTANCE.md), together with [paired geometry](CHARACTER_GEOMETRY_PERFORMANCE.md), [initial readiness](FIGHTING_GAME_READINESS.md), [preparation](CHARACTER_PREPARATION_PERFORMANCE.md) and [handoff](CHARACTER_HANDOFF_PERFORMANCE.md).

Measured sustained NTSC examples (Ymir, **submission** rate, not presentation rate):

| Detail | Simulation steps/s | Submissions/s | CPU median | Geometry preparation median | Emission median |
| --- | ---: | ---: | ---: | ---: | ---: |
| LOW | 60.00 | 29.66 | 26 ms | measure with matched profiling | measure with matched profiling |
| MEDIUM | 60.00 | 19.775 | 36 ms | 12.121 ms | 5.960 ms |
| HIGH | 60.00 | 19.747 | 42 ms | 15.450 ms | 7.067 ms |

PAL and separately captured detail-switch measurements must not be mixed with those sustained NTSC runs. A 33.33 ms submission interval is only a necessary interim target: real VDP1 rendering and display-field ownership must also be assessed for **unique** frames.

Do not reimplement optimizations already in main: immutable scene binding, precomputed per-vertex projection, the SH-2-assisted 64/32 projection divide, paired Master/Slave geometry preparation, source-cache publication, RGB hardware clipping when safe, CPU command transfer selected over slower DMA, scene-wide painter sorting, no per-frame heap allocations and the existing measured LOW tier.

## Three-tier model acceptance matrix (mandatory)

Optimizing only the stock blue/red humanoids does **not** establish readiness for Tekken 3. Run the same profiling and regression gates against **all three tiers**; report each tier separately without substituting one for another.

| Tier | Source/workload | What it establishes | Pass/fail interpretation |
| --- | --- | --- | --- |
| **A — regression baseline** | Current original 24-joint blue/red humanoids, two actors, 143/260/338 vertices and 151/301/401 faces at LOW/MEDIUM/HIGH; existing `fighter_arena_3d` and `skinning_stress_3d` | Comparable historical timing, renderer/API safety, serial/Slave parity | Required to prevent regressions; **never sufficient alone** |
| **B — complexity/stress** | Existing `killer_clown_demo`, two 66-joint animated clowns with textured surfaces, arena and lighting | Larger skeletons, animation/texture complexity, submission and memory pressure | Required stress validation; not a surrogate for Tekken geometry |
| **C — definitive port-readiness** | Two **actual Tekken 3 PlayStation fighters**, initially **Jin Kazama and Paul Phoenix**, using costume models, textures and representative motion frames converted from the user's own original disc | Real part hierarchy and seams, actual face/material/texture costs, motion memory, CPU skinning, VDP1 throughput, close-contact overlap | **Decisive gate:** no declaration of Tekken readiness from tiers A/B alone |

Tier A counts are the shipped example mesh LOD counts; they are **not** claims about Tekken's vertex or polygon budgets. Discover and report tier C's actual unique vertices, face counts, part/joint count, skin influences, material count, texture/CLUT bytes and compressed/uncompressed motion footprint from the input assets. Do **not** simplify real Tekken geometry for the primary measurement. If an optimized LOD is investigated, report it as an additional variant alongside the unchanged source-derived baseline, with image comparisons.

### Tier C: real-model benchmark without porting gameplay

1. Complete **Phase -1**: select the highest-free-space eligible local disk, inspect the [RomsDL Tekken 3 US (SLUS-00402) landing page](https://romsdl.com/roms/playstation-1/tekken-3-u-iso-slus-00402-81202), and obtain a valid original-disc input only when genuinely accessible and authorized, otherwise use an independently verified permitted local original-disc rip. Then build a **separate optional render-only consumer/harness** that links against LibSaturn; it must not execute Godot or port Tekken's fight state/AI. It may use local generated headers/binary archives and existing LibSaturn scene/skin/asset APIs.
2. Reuse the extraction research and local conversion pipeline of [n0madic/Tekken3Remake](https://github.com/n0madic/Tekken3Remake), especially `tools/remake_import/character.py` and `motion_bake.py`. Convert source assets **offline** to bounded Saturn-native geometry/textures/pose streams; account for PS1 model parts and seam-local multi-anchor positions rather than assuming a conventional glTF rig. A source-derived 22-part skeleton must be mapped explicitly; support additional joints/attachments where actually present.
3. The reference motion workload must include at least idle, walk, sidestep, punches, kicks, guard, knockdown, and close-range/throw-like contact poses where the source provides them. Use deterministic scripted camera angles, zoom and motion pairs, including both fighters crossing/occluding one another. Real combat rules are **not** part of this benchmark.
4. Use **identical** deterministic camera, pose, resource residency and input sequences before/after every optimization. Compare the geometry, painter queue, materials, clipping decisions, captures and Master/Slave results. Maintain the baseline source meshes for pixel and geometric comparisons.
5. Run Tier C first in host validation/Ymir/Mednafen, then on physical Saturn for acceptance. Report NTSC and PAL separately, with stock-RAM as the primary configuration; 1 MiB/4 MiB cartridge runs are optional, labeled supplementary scenarios rather than a substitute for stock-RAM acceptance.
6. Protect intellectual property: original disc images, imported/converter outputs, model geometry, textures, music, generated game-specific binary assets and screenshots containing game art remain **local and untracked**, not committed to LibSaturn, not included in public CI fixtures, releases, or artifacts. Check `.gitignore`/harness cleanup rules before running; publish only original generic benchmark code, aggregate metrics and non-infringing descriptions. Observe Tekken3Remake's PolyForm Noncommercial license when reusing its code.
7. If the original disc or permitted converter inputs are unavailable, mark Tier C **BLOCKED** and report only Tier A/B progress. Do not fabricate measurements or claim full port readiness.

### Comparable workload/report contract

For each tier, record: source/asset fingerprint kept locally, fighter count, LOD (if any), vertices/faces/joints, material/texture footprint, motion bytes, stock-RAM headroom, VDP1 command occupancy, simulation steps/s, *submission* rate, unique presented frames/s when measurable, frame p50/p95/max, skinning, projection/preparation, clipping, sorting, emission and transfer/wait timings, and visual correctness findings. Track MASTER/SLAVE/AUTO under matched conditions and include cold/warm cache and close-contact frames.

**Minimum release gate:** tiers A and B pass regressions, then Tier C with the *actual two fighters* sustains the agreed frame target (initially 30 unique NTSC frames/s with 60 Hz scripted pose/game-loop updates) **or** the result is explicitly classified as below target with measured root causes. A Tier A LOW 30-submission/s result does **not** close the Tier C gate.

## Phase -1 — choose largest local disk and acquire Tekken 3 source data

Complete before the Tier C real-character benchmark. Perform local disk inspection and any authorized acquisition **on the machine running the harness**, not through GitHub. Merely updating the plan does not mount a disk or download anything.

### Requested source (reviewed 2026-10-14)

Use this exact **landing page**, replacing the previous Internet Archive source:

**[RomsDL — Tekken 3 [U] ISO [SLUS-00402]](https://romsdl.com/roms/playstation-1/tekken-3-u-iso-slus-00402-81202)**

The indexed page identifies the **US PlayStation 1** release, disc ID **SLUS-00402**, and describes its offered file as **`Tekken 3 (USA).7z`**. It provides **no reliable file-size figure**. A direct download URL has **not** been verified: an automated attempt to load the page encountered browser-verification/anti-bot interstitial content. Do not invent a direct file URL, assume a fixed compressed size, claim a successful download, bypass access controls, or use an unrelated title/region.

Acquisition procedure:

1. Open the **exact landing page** in a browser on the actual machine. Inspect the download action and the final filename/host. Access must be legitimately available and authorized; follow ordinary site interaction only, without attempting to evade security challenges.
2. Prefer a locally available original-disc dump if the site cannot be accessed or the download is unverified. A page saying `SLUS-00402` does not prove its contents are a valid `SLUS-00402` image.
3. If an authorized file is downloadable, transfer **only the Tekken 3 US archive**, not bulk ROM collections, to `$TEKKEN3_ASSET_ROOT/downloads/` on the chosen high-free-space persistent disk. Configure the browser download directory or save-file destination explicitly. A browser default `Downloads` folder on the OS disk is **not** acceptable.
4. Save the actual source page URL, final resolved origin URL, filename, content length if exposed, and SHA-256 in **private local** logs. Reject HTML, bot-check pages, ZIP bombs, partial archives, or wrong-region media masquerading as a `.7z` file.

### Choose the disk with the most **available** persistent space

1. Enumerate writable, **local, persistent** volumes and sort by **available free bytes**, not total capacity. Windows: `Get-CimInstance Win32_LogicalDisk -Filter 'DriveType=3'`; Linux: `lsblk -b -f` plus `df -Pk`; macOS: `diskutil list` plus `df -Pk`. Exclude read-only, ram-backed, network, pseudo/temporary, inaccessible, and duplicate mounts of one underlying volume/APFS container.
2. Select the volume with greatest usable headroom that can accommodate the actual compressed archive, disc image/tracks, converter output, scratch workspace, emulator/build data, and at least **20% additional safety margin** plus an OS-reserve allowance. **Source file size is unknown** until verified; do not reuse stale size estimates from previously considered sources.
3. Create a private external work root, e.g. `D:\libsaturn-assets\tekken3` on Windows or `/mnt/large-volume/libsaturn-assets/tekken3` on Linux (examples only). Create `downloads/`, `disc/`, `extracted/`, `converted/`, `scratch/` and `logs/`. **Do not** place these under the Git checkout or a default OS temporary directory.
4. Set `TEKKEN3_ASSET_ROOT` for all converter and benchmark processes. Redirect `TEMP`/`TMP` (Windows) or `TMPDIR` (POSIX) to the chosen volume for large temporary conversions. Recheck filesystem free bytes and write access before downloading **and** before unpacking.
5. Never silently repartition disks, delete unrelated data, download to the smaller default OS disk, or assume the largest advertised drive has the most free space. Report selected device/mount, initial available bytes, estimated peak footprint and headroom after acquisition.

**PowerShell (select the drive and prepare the download target):**

```powershell
$volumes = @(Get-CimInstance Win32_LogicalDisk -Filter 'DriveType=3' |
    Where-Object { $_.FreeSpace -ge 4GB } |
    Sort-Object FreeSpace -Descending)
if ($volumes.Count -eq 0) { throw 'No eligible persistent disk has 4 GiB free' }
$drive = $volumes[0].DeviceID
$env:TEKKEN3_ASSET_ROOT = Join-Path ($drive + '\') 'libsaturn-assets\tekken3'
@('downloads','disc','extracted','converted','scratch','logs') | ForEach-Object {
    New-Item -ItemType Directory -Force -Path (Join-Path $env:TEKKEN3_ASSET_ROOT $_) | Out-Null
}
$env:TEMP = Join-Path $env:TEKKEN3_ASSET_ROOT 'scratch'
$env:TMP = $env:TEMP
Write-Host "Source page: https://romsdl.com/roms/playstation-1/tekken-3-u-iso-slus-00402-81202"
Write-Host "Select this folder as the browser download destination: $(Join-Path $env:TEKKEN3_ASSET_ROOT 'downloads')"
# The site's direct download URL is not verified; do not automate an invented URL.
```

**Linux/macOS (inspect and set a verified eligible mount):**

```sh
df -Pk
# After verifying which persistent writable volume has the largest usable space:
export TEKKEN3_ASSET_ROOT="/path/on/selected-largest-free-disk/libsaturn-assets/tekken3"
for subdir in downloads disc extracted converted scratch logs; do
  mkdir -p "$TEKKEN3_ASSET_ROOT/$subdir"
done
export TMPDIR="$TEKKEN3_ASSET_ROOT/scratch"
printf 'Source page: %s\n' \
  'https://romsdl.com/roms/playstation-1/tekken-3-u-iso-slus-00402-81202'
printf 'Browser download destination: %s\n' "$TEKKEN3_ASSET_ROOT/downloads"
# Do not use curl against the HTML landing page as if it were a 7z archive.
```

### Validate downloaded/local disc before Tier C

- Inspect the resulting archive or authorized local dump; do not trust filename or site title. Verify nonzero length, appropriate archive magic, `7z t` (for `.7z`), SHA-256 and unpacked sizes *before* extraction; reject archive path traversal and excessive expansion. Extract only into `disc/` on the selected volume.
- Confirm the original PS1 disc identifier **SLUS-00402**, proper disc tracks/file structure and the inputs required by the Tekken3Remake extraction scripts. ISO/BIN/CUE/CHD support must be checked against the actual converter, not inferred from a web page.
- Keep all disc data, model geometry, textures, animation streams and game-specific converter outputs **private, outside Git**, and out of CI artifacts. No game archive or disc image is committed to LibSaturn.
- If access is blocked, authorization is unclear, or the archive fails validation, mark **Tier C BLOCKED — valid original-disc input unavailable**. Continue the generic Tier A/B benchmarks; never report synthetic assets as a successful Tier C test.

**Gate -1:** largest eligible writable local disk selected; the exact RomsDL page reviewed; transfer performed *only* if the correct authorized archive is genuinely available; any media input independently validated, with a recorded local hash and sufficient disk headroom. Tier C remains blocked if valid original-game data is unavailable.

## Phase 0 — reproducible profiling first

After Phase -1 confirms the selected high-free-space asset volume and real-disc preflight, run the unchanged project scripts for Tier A, the existing Killer Clown benchmark/harness for Tier B, and the optional local real-asset consumer for Tier C. Archive exact commit SHA, executable hash, local asset fingerprint (never the assets themselves), geometry/detail setting, BIOS, video mode, cache and cart configuration, Ymir and Mednafen versions, headroom from map files, complete counters and representative screenshots held locally. Build the Tier C harness and baseline **before** selecting optimizations.

```powershell
.\harness\run-character-plan.ps1 -Bios bios/saturn_bios_us.bin -Scenario rate -RateDetail low
.\harness\run-character-plan.ps1 -Bios bios/saturn_bios_us.bin -Scenario rate -RateDetail medium
.\harness\run-character-plan.ps1 -Bios bios/saturn_bios_us.bin -Scenario rate -RateDetail high
.\harness\run-character-plan.ps1 -Bios bios/saturn_bios_us.bin -Scenario materials
.\harness\run-character-plan.ps1 -Bios bios/saturn_bios_eu.bin -Pal -Scenario rate -RateDetail medium
```

Keep per-iteration FRT measurement for *same* frames: hierarchy evaluation, clip sampling, skinning, world transform, projection, face culling, near/screen fallback, material/Gouraud preparation, painter ordering, command generation, command upload, wait/swap and VDP1 command/draw completion. Wall-clock stages may overlap across SH-2s and must not be summed as independent savings. Distinguish list submissions, video fields and genuinely new displayed frames. Validate instrumentation overhead separately. No performance change is accepted without its own matched before/after comparison and a reproducible harness command.

**Gate P0:** repeated Tier A/B baselines, plus Tier C baseline if local game data is available; the largest costs identified on representative frames, no regression from instrumentation. If Tier C is blocked by missing local inputs, keep the readiness gate open.

## Phase 1 — geometry preparation and projection

Start at `src/graphics/3d/scene/faces.cpp`, `src/graphics/3d/scene/parallel.cpp`, `src/graphics/3d/rendering/logic.hpp`, and `examples/characters/demo.c`.

1. Attribute actual work between CPU skinning, per-vertex world/projection transforms, projected and world-space backface checks, material lookup, clipping preparation, and shared-memory/cache publication.
2. Benchmark transformations with the real SH-2 compiler; avoid added 64-bit software divisions, additional passes, temporary allocations or larger caller scratch arrays.
3. Keep rounded projected winding and geometric quad visibility as distinct correctness checks. Do not remove one because a visible frame looks acceptable.
4. Avoid repeatedly publishing **provably immutable** arrays only if ownership, explicit invalidation and cross-CPU visibility are maintained and tested (the existing cached binding is not a blanket exemption).
5. Compare serial and paired worker geometry **field by field**: face admission, keys, material references, culling, clipping, Gouraud, projected corners and near-plane fallback.

**Gate P1:** strict output equivalence in host and Ymir regression scenarios across A/B/C (Tier C when inputs are available); measurable reduced CPU/prepare median and p95 for two MEDIUM/HIGH baseline characters **and actual Tekken-model workload**; no extra drops, panics, unsupported task errors or RAM budget overruns. Publish separate results per tier.

## Phase 2 — VDP1 emission and frame scheduling

Measure `src/graphics/3d/scene/faces.cpp` emission and the VDP1 HAL. Separate frontend command construction/upload time from hardware draw completion, VRAM writes, and VBlank waits.

- Evaluate material/texture binding reuse and the projected-safe vs clipped fallback command paths on real two-character content. Preserve texture generation and residency validation.
- Do not change painter ordering or perform partial VDP1 scene commits after a capacity error. Check command rollback, overlay reservations, Gouraud/table bounds, and near clipping.
- Do not switch to SCU DMA by default: prior measurements found CPU uploads faster for this workload; require a new matched test before revisiting this.
- Optimize by measured cost, not by counting API calls. Track command count and near/screen fallback rates alongside frame time.

**Gate P2:** sustain Tier A LOW, materially improve Tier A MEDIUM/HIGH and Tier B, **then verify results on Tier C's unchanged source-derived models and motion sequences**. Do not trade away accurate model geometry or interpenetration/clipping behavior for a headline number. Advance to a physical-console 30-FPS claim only after *unique frame* verification using the actual Tier C fighters.

## Phase 3 — optional conservative pre-skin visibility

The existing instance AABB test runs when submitting geometry: by then the actor may already have paid the skinning cost. Investigate an **optional** per-actor, per-clip or per-joint conservative bound that can reject wholly off-screen actors *before* skinning and geometry preparation. Avoid any negative result for visible limbs, attacks, throws, extreme poses, camera movement or near-plane straddles. If validation overhead exceeds savings for two visible fighters, leave this optimization disabled.

**Gate P3:** no false-negative rejects across randomized poses and camera cases including Tier C close-contact/throw-like poses, identical visible-frame output, bounded memory and positive measured net benefit on an explicitly off-screen workload. Do not claim this improves two simultaneously visible fighters.

## Phase 4 — compact random-access animation

The existing study reports a lossless constant-channel opportunity: resident clip poses dropped analytically from 71,520 B to 16,464 B, but the decoder is **not** integrated or SH-2 benchmarked. Prototype the codec in `tools/model_pipeline/` first, then test an opt-in, versioned on-disc representation that decodes into caller-owned pose memory.

- Store constant channels once; store frame-varying channels so an arbitrary frame can be decoded in O(joints) (or O(active channels)) without walking all preceding frames.
- Preserve exact signed integers / quaternion channels, clip events, root motion, wrap behavior and arbitrary move restarts. Avoid interpreting sampled PS1 move channels as generic normalized quaternion clips in the future game adapter.
- Measure decoder CPU, bandwidth, compression ratio, worst-case size and peak working-set. Retain uncompressed SKA1 as a supported fallback until an explicit compatibility transition.
- Validate host roundtrip for every pose, random frame jumps, truncated/corrupt payload rejection and representative on-SH-2 runtime tests.

**Gate P4:** bit-exact pose reconstruction, predictable decode cost, reduced resident bytes, no measurable combat/handoff regression. Confirm separate exactness for Tier C's actual sampled motion-channel encoding and randomly selected frames; a generic skeletal quaternion codec cannot silently replace Tekken's source pose semantics.

## Phase 5 — asset residency and CD streaming

Define bounded per-fighter active-move caches; preload likely next clips during safe periods, preserve deterministic move-frame access and permit deferred release at frame boundaries. Do not make gameplay depend on CD read completion. Run the existing real CD-backed character path in Mednafen before changing cache/streaming policies, and explicitly verify optical-disc behavior on physical hardware.

Measure worst-case round restart, character switch, clip miss, cold/warm cache, VRAM texture ownership, fragmentation, loading pauses and RAM/cart independent builds.

**Gate P5:** playable two-character stock-RAM run, deterministic input trace under normal/worst-case streaming, no asset dangling pointers, no frame-time spikes outside explicitly signalled loading transitions. Measure the same policies against Tier C's real models/textures/motions when locally available.

## Exit criteria before a Tekken port

1. A reproducible, three-tier benchmark matrix: Tier A synthetic 24-joint models, Tier B 66-joint Killer Clown actors, **Tier C actual Jin/Paul Tekken 3 assets**, with baseline/optimized captures (copyrighted images local only), RAM maps, task serial/worker equivalence and 60 Hz deterministic scripted updates. Tier C must run before declaring port-readiness; missing disc inputs mean BLOCKED, not PASS.
2. An honest reported rendering tier **for each of A/B/C**: separate submission throughput, VDP1 draw completion and unique physical frames. If 30 unique NTSC frames/s for **actual Tier C models** is not attained, record measured limits and the exact next bottleneck instead of weakening the gate. No synthetic-model result substitutes for that requirement.
3. Exact and sufficiently cheap random-access animation with bounded per-character residency, or a documented uncompressed fallback.
4. The complete host/tool/build gate, Ymir automated scenarios, Mednafen disc-backed run and physical NTSC/PAL checks in [hardware acceptance](HARDWARE_ACCEPTANCE_CHECKLIST.md).
5. Portable generic code remains in LibSaturn. The optional real-model **render-only** benchmark consumer owns the local conversion path, source fingerprinting and game-specific data; the eventual gameplay port will also own PS1 4.12/GTE compatibility, fight state, 22-part model semantics, media, stages and test traces. Neither consumer may commit proprietary assets into LibSaturn.
