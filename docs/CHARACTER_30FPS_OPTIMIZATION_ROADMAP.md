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

1. Complete **Phase -1**: select the highest-free-space eligible local disk, check the [psx-roms-archive](https://archive.org/download/psx-roms-archive) listing and the currently **locked** Tekken 3 `SLUS-00402` archive; download it **only if actually accessible and authorized**, otherwise use an independently verified permitted local original-disc rip. Then build a **separate optional render-only consumer/harness** that links against LibSaturn; it must not execute Godot or port Tekken's fight state/AI. It may use local generated headers/binary archives and existing LibSaturn scene/skin/asset APIs.
2. Reuse the extraction research and local conversion pipeline of [n0madic/Tekken3Remake](https://github.com/n0madic/Tekken3Remake), especially `tools/remake_import/character.py` and `motion_bake.py`. Convert source assets **offline** to bounded Saturn-native geometry/textures/pose streams; account for PS1 model parts and seam-local multi-anchor positions rather than assuming a conventional glTF rig. A source-derived 22-part skeleton must be mapped explicitly; support additional joints/attachments where actually present.
3. The reference motion workload must include at least idle, walk, sidestep, punches, kicks, guard, knockdown, and close-range/throw-like contact poses where the source provides them. Use deterministic scripted camera angles, zoom and motion pairs, including both fighters crossing/occluding one another. Real combat rules are **not** part of this benchmark.
4. Use **identical** deterministic camera, pose, resource residency and input sequences before/after every optimization. Compare the geometry, painter queue, materials, clipping decisions, captures and Master/Slave results. Maintain the baseline source meshes for pixel and geometric comparisons.
5. Run Tier C first in host validation/Ymir/Mednafen, then on physical Saturn for acceptance. Report NTSC and PAL separately, with stock-RAM as the primary configuration; 1 MiB/4 MiB cartridge runs are optional, labeled supplementary scenarios rather than a substitute for stock-RAM acceptance.
6. Protect intellectual property: original disc images, imported/converter outputs, model geometry, textures, music, generated game-specific binary assets and screenshots containing game art remain **local and untracked**, not committed to LibSaturn, not included in public CI fixtures, releases, or artifacts. Check `.gitignore`/harness cleanup rules before running; publish only original generic benchmark code, aggregate metrics and non-infringing descriptions. Observe Tekken3Remake's PolyForm Noncommercial license when reusing its code.
7. If the original disc or permitted converter inputs are unavailable, mark Tier C **BLOCKED** and report only Tier A/B progress. Do not fabricate measurements or claim full port readiness.

### Comparable workload/report contract

For each tier, record: source/asset fingerprint kept locally, fighter count, LOD (if any), vertices/faces/joints, material/texture footprint, motion bytes, stock-RAM headroom, VDP1 command occupancy, simulation steps/s, *submission* rate, unique presented frames/s when measurable, frame p50/p95/max, skinning, projection/preparation, clipping, sorting, emission and transfer/wait timings, and visual correctness findings. Track MASTER/SLAVE/AUTO under matched conditions and include cold/warm cache and close-contact frames.

**Minimum release gate:** tiers A and B pass regressions, then Tier C with the *actual two fighters* sustains the agreed frame target (initially 30 unique NTSC frames/s with 60 Hz scripted pose/game-loop updates) **or** the result is explicitly classified as below target with measured root causes. A Tier A LOW 30-submission/s result does **not** close the Tier C gate.

## Phase -1 — select the largest usable disk and acquire Tekken 3 disc data

This phase precedes Tier C extraction and Phase 0. Execute storage inspection and any permitted download **on the user's actual machine**, not through the GitHub connector. GitHub documentation edits cannot inspect local drive capacities or perform the download.

### Input source and current access status (checked 2026-10-14)

Requested source directory: **[Internet Archive — psx-roms-archive](https://archive.org/download/psx-roms-archive)**.

The listing identifies **\`tekken-3-u-slus-00402-.7z\`**, catalogued at **406.2 MB**, for the US \`SLUS-00402\` edition. As checked on 2026-10-14, its listing labels this file **locked / unavailable for download**, and direct unauthenticated retrieval could not be confirmed. A listing entry is **not evidence of downloadable access**, file integrity, or redistribution rights. **Never bypass an access lock or invent a mirror.** Recheck access at runtime; if the item remains restricted, fall back to a user-provided legitimately accessible original-disc dump and mark the archive acquisition as BLOCKED.

- Directory to inspect: \`https://archive.org/download/psx-roms-archive\`
- Specific item to identify in its file listing: \`tekken-3-u-slus-00402-.7z\`
- Exact candidate URL (conditional on **actual authorized availability**, not verified working): \`https://archive.org/download/psx-roms-archive/tekken-3-u-slus-00402-.7z\`
- Fetch **only Tekken 3 US SLUS-00402**, if available and legally authorized; do not download the whole archive or unrelated ROMs.
- Remove the previous reference-manual requirement entirely. No PDF/manual acquisition is part of this work.
- The historical listed \`406.2 MB\` is the **compressed archive size**, not the unpacked disc size or total temporary-space requirement. Revalidate the current source metadata immediately before choosing a volume.

### Storage-volume selection and work directory

1. Enumerate all **local, writable, persistent** disks/volumes on the machine running the harness. Inspect **free bytes**, not nominal total disk capacity. Windows: \`Get-CimInstance Win32_LogicalDisk -Filter 'DriveType=3'\`; Linux: \`lsblk -b -f\` and \`df -Pk\`; macOS: \`diskutil list\` and \`df -Pk\`. Reject read-only, ram-backed, system pseudo-filesystems, network mounts, inaccessible mounts and duplicates pointing at one underlying volume/APFS pool.
2. Choose the eligible volume with the **largest usable free space**, while retaining headroom for the operating system. Budget for ~406.2 MB of possible source download **plus** unpacked BIN/CUE tracks, converted meshes, motion data, textures, compiler/emulator outputs, logs, and a minimum **20% safety margin**. Validate the actual sizes after archive inspection; a 4 GiB floor is an initial preflight only, not a claim that it will always suffice.
3. Store everything outside the repository, using a work root such as \`D:\libsaturn-assets\tekken3\` on Windows or \`/mnt/large-volume/libsaturn-assets/tekken3\` on Linux. These example paths do not assert that those volumes exist. Within the work root use \`downloads/\`, \`disc/\`, \`extracted/\`, \`converted/\`, \`scratch/\`, and \`logs/\`. Avoid duplicating the original media image.
4. Set \`TEKKEN3_ASSET_ROOT\` for the whole conversion/benchmark chain. Ensure extraction scratch and subprocess temporary folders also use the selected disk (e.g. explicit \`--workdir\` options, \`TMP\`/\`TEMP\` on Windows, \`TMPDIR\` on POSIX), rather than silently exhausting the OS disk.
5. Verify write permission and reevaluate available space before downloading and before unpacking. If the largest drive still lacks enough headroom, report the blocker and stop; never overwrite arbitrary files, repartition disks, silently delete user data or use cart RAM as a substitute for host disk space.
6. Keep media inputs and extracted or converted game assets **private and outside version control**. Check cleanup scripts and ignore rules; publish only generic tooling and aggregate measurements, not \`.7z\`, BIN/CUE/ISO/CHD, models, textures, animations, proprietary screenshots or disc-derived assets.

### Executable download preflight (only when access is available and authorized)

**Windows PowerShell:** select the local fixed volume with the most free space and prepare the directory. Check the remote file's authorization/access status before transferring 406 MB. If unavailable, **stop cleanly**, without producing an empty or HTML masquerading as \`.7z\` file.

\`\`\`powershell
$volumes = @(Get-CimInstance Win32_LogicalDisk -Filter 'DriveType=3' |
    Where-Object { $_.FreeSpace -ge 4GB } |
    Sort-Object FreeSpace -Descending)
if ($volumes.Count -eq 0) { throw 'No eligible local disk with 4 GiB free' }
$drive = $volumes[0].DeviceID
$env:TEKKEN3_ASSET_ROOT = Join-Path ($drive + '\') 'libsaturn-assets\tekken3'
$downloads = Join-Path $env:TEKKEN3_ASSET_ROOT 'downloads'
New-Item -ItemType Directory -Force -Path $downloads | Out-Null
$url = 'https://archive.org/download/psx-roms-archive/tekken-3-u-slus-00402-.7z'
try {
    $head = Invoke-WebRequest -Uri $url -Method Head -MaximumRedirection 5 -ErrorAction Stop
    if ($head.StatusCode -ne 200) { throw "Unexpected HTTP status $($head.StatusCode)" }
} catch {
    throw "Archive item is unavailable/locked; use a permitted local disc rip instead. Detail: $_"
}
# Download only with authorized access. Do not bypass access controls.
$archive = Join-Path $downloads 'tekken-3-u-slus-00402-.7z'
Invoke-WebRequest -Uri $url -OutFile $archive -ErrorAction Stop
if ((Get-Item $archive).Length -lt 100000000) {
    Remove-Item -LiteralPath $archive -Force
    throw 'Archive download is truncated or not the listed game archive'
}
Get-FileHash -Algorithm SHA256 $archive
\`\`\`

**Linux/macOS:** enumerate mounts and have the agent select the largest **eligible persistent and writable** disk first (do not assume \`/tmp\`, root, or an illustrative path is appropriate). Then use the exact \`TEKKEN3_ASSET_ROOT\` on that disk:

\`\`\`sh
df -Pk
# Set this to the agent-verified highest-free-space eligible local volume:
export TEKKEN3_ASSET_ROOT="/path/on/largest-eligible-disk/libsaturn-assets/tekken3"
mkdir -p "$TEKKEN3_ASSET_ROOT/downloads"
url="https://archive.org/download/psx-roms-archive/tekken-3-u-slus-00402-.7z"
# A 403/404/other failure is a BLOCKED acquisition, not permission to bypass a lock.
curl --fail --location --silent --show-error --head "$url" ||
  { echo "Source unavailable; supply your own permitted original-disc rip" >&2; exit 1; }
curl --fail --location --retry 3 --output \
  "$TEKKEN3_ASSET_ROOT/downloads/tekken-3-u-slus-00402-.7z" "$url"
shasum -a 256 "$TEKKEN3_ASSET_ROOT/downloads/tekken-3-u-slus-00402-.7z"
\`\`\`

The snippets do **not** guarantee download success: the item currently appears locked. In automation, additionally inspect HTTP Content-Length/Content-Type and compare the response size against source metadata when available; verify the archive signature and run \`7z t\` **before** extraction. Enforce expanded-space estimates and prevent unsafe archive paths/links or extraction outside \`disc/\` on the selected volume. Record the resulting SHA-256 locally. Do not automatically fetch a different game, edition, or unverified third-party mirror when access fails.

### Disc and converter gate for Tier C

- Prefer a verified user-supplied original \`SLUS-00402\` PlayStation disc dump if the archive is locked. Do not assume a \`.7z\` filename guarantees genuine compatible BIN/CUE data: test the archive, unpack locally and inspect disc identifiers, track layout and required Tekken3Remake files. Confirm the actual converter input format before invoking it.
- Point Tekken3Remake's offline converter to a verified local disc input. Output extracted and Saturn-native assets only to \`extracted/\` and \`converted/\`; never put derived assets in the Git checkout, commits or public CI.
- If no legally accessible valid disc is present, declare **Tier C BLOCKED — game image unavailable**, and continue only Tier A/B generic optimization. Do not fabricate model measurements.

**Gate -1:** eligible largest-free-space disk chosen and space-checked; specified Archive.org game listing and its accessibility checked accurately; optional download only on actual authorized access; archive/disc validated before model conversion; no media or extracted art committed.

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
