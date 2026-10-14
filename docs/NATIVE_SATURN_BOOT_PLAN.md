# Native Sega Saturn Boot Plan (LibSaturn)

**Status:** local implementation underway; the optical tester, native candidate and validation tooling are implemented. Physical acceptance and promotion remain pending. See [execution evidence](NATIVE_SATURN_BOOT_EXECUTION.md).
**Target:** `main`, after validation gates.
**Scope:** Satiator regression after an early reportedly bootable Dino image, persistent SAROO incompatibility across tested Dino releases, a permanent optical-emulator diagnostic example, and a reusable independently owned LibSaturn disc-boot pipeline.
**Related investigation:** [Dino Satiator boot investigation](DINO_SATIATOR_BOOT_INVESTIGATION.md).

## 1. Objective and non-goals

Replace the dependency on YAUL/Jo Engine/SBL *application bootstrap code* with a small, auditable LibSaturn-owned Application Initial Program (AIP), deterministic IP metadata tooling, and an end-to-end BIOS disc-boot test path. Preserve the security and area-code requirements of an authentic Saturn boot; "native" does **not** mean bypassing the Sega BIOS or manufacturing the proprietary authentication payload.

Required outcome: the same distribution build starts reliably on a real Saturn through a Satiator and a SAROO, while the Dino Demo still renders and plays audio as before. **Implement and keep `examples/optical_emulators_tester/` as a supported diagnostic example, not a one-off harness fixture.** The pipeline must explain or explicitly bracket failures between the licensed screen and `main()`, including cases in which no application UI can run.

Non-goals:
- Rewriting the game's renderer, progress bar, audio engine or animation as a presumed boot fix.
- Replacing the console BIOS, patching Satiator/SAROO firmware, or assuming that an emulator is definitive hardware proof.
- Shipping unlicensed Sega BIOS, CD Block firmware, or security assets.
- Removing the current YAUL/SBL boot profiles before a proven native alternative exists.

## 2. Established observations and uncertainties

1. **The developer confirms two distinct histories:** the **first Dino Demo build booted on a Satiator**, but **none of the Dino builds tested so far booted on SAROO**. These are first-hand developer reports about particular tests, not exhaustive claims about all ODE firmware, BIOS or console revisions. The original Satiator-working binary has not yet been identified and independently reproduced. Do **not** call the later v0.2 or v0.4 ZIP a confirmed Satiator pass.
2. The author's public [Dino Demo itch.io release](https://celsowm.itch.io/dino-demo-saturn-version) lists **`dino_demo_v0.2.zip` (703 kB), `dino_demo_v0.3.zip` (2.4 MB), and `dino_demo_v0.4.zip` (2.4 MB)**. The reported SAROO failures also appear in the [October 13, 2026 r/SegaSaturn discussion](https://www.reddit.com/r/SegaSaturn/comments/1x4edil/dino_demo_saturn_version_04/); the exact devices and firmware revisions must be logged. Preserve the original release ZIPs and their hashes separately from reconstructed builds.
3. The user already replaced the original fixed `0x900` (2304-iteration) `HIRQ.CMOK` poll in the default YAUL template with an unbounded CMOK wait. **That change did not fix the physical boot failure.** Do not propose "wait for CMOK" again as if it were untried.
4. `assets/boot/ip_sbl_template.bin` still contains the bounded CD-command wait. `assets/boot/ip_yaul_template.bin` contains the subsequent unconditional wait. Both rely on inherited region/boot code; neither is an independent native boot implementation.
5. `examples/dino_demo/main.c` first displays the progress bar *after* `sat_init()`. A frozen licensed screen does **not** prove that the progress bar itself executed, nor prove that application execution never started.
6. The earlier Dino source commits [`b56522f`](https://github.com/celsowm/libsaturn/commit/b56522f5d541f0076494623b8a7f5bb0d6261759) (initial demo, September 23), [`a3a58c2`](https://github.com/celsowm/libsaturn/commit/a3a58c2b29f488cb4e6ef65a864c092e7a5c7162) (hi-res work, September 24), and [`83ae192`](https://github.com/celsowm/libsaturn/commit/83ae19266df5c5181bb7988f04d004ef40f1bb56) (CD music, September 27) predate the progress bar. None is confirmed to be the exact hardware-passing image.
7. [`920023e`](https://github.com/celsowm/libsaturn/commit/920023e4674a448e928bf780413198b984a7963c) introduced the Dino loading bar along with changes to hi-res mode, frame submission, CRAM and VDP2 lighting. Use controlled A/B builds; do not attribute the regression to a single diff hunk.
8. The default YAUL template was first *tracked* in Git in [`4dddbf1`](https://github.com/celsowm/libsaturn/commit/4dddbf1340f70978d898a6c6977ecf8a5fda3dee) on October 2. Older checkouts might not reproduce their original template bytes.
9. The current Ymir harness directly injects the `.app.bin` into Work RAM. Its downstream runtime results cannot certify BIOS -> IP -> 1st-read -> AIP -> executable handoff. The existing investigation also documents BIOS boot runs using Mednafen and triage images, but **there is no confirmed Satiator/SAROO pass**.
10. `src/core/startup/early_init.c` writes to `0x20C00000` while labeling it SCU. Audit that address and its offsets against the official memory map before changing it; an incorrect hardware access during `sat_init()` is a separate possible early-runtime failure.
11. The generated IP header and ISO validators currently check limited fields and comparisons against templates. Passing them does not prove ISO9660 1st-read ordering, full BIOS handoff, or legal/provenance validity of the security payload.
12. Inspection of the supplied original v0.2 and v0.4 archives reported that their bootstrap/region bytes matched, apart from application-size metadata, and both used the old 2304-iteration CMOK poll. Both also showed candidate IP header anomalies: `0x0E4 = 0x060FFFFC` (reserved field) and `0x0EC = 0x06001000` (Slave stack). **Re-extract and independently verify the exact bytes against the Sega System ID specification before calling them invalid or treating them as a root cause.** These shared values could explain persistent SAROO trouble but cannot, without further evidence, explain a later Satiator regression.
13. The planned `optical_emulators_tester` does not exist yet. The previously proposed minimal `boot_probe` is insufficient as a permanent diagnostics tool; it must be incorporated into the dedicated example and a pre-application tracing mechanism.

### Working hypotheses (not findings)

- H1: the BIOS/area-code CD command sequence fails or waits forever on real hardware/ODE timing.
- H2: first-read file discovery, file size, load location, IP sizing, or boot handoff differs from the verified emulator path.
- H3: the app enters `_start` but faults, waits for video/interrupts, or alters invalid MMIO before displaying its first frame.
- H4: a particular ODE's disc/menu/region behavior interacts with boot code, even if both devices exhibit visually similar symptoms.
- H5: a later runtime/configuration change, rather than the loading bar itself, explains the known-good/known-bad difference.
- H6: incorrect or unusual IP System ID reserved/Slave-stack values may affect SAROO's BIOS-hook path, independently of Satiator's later regression.

**Maintain two investigation tracks:** (S) identify the first Satiator passing build and bisect the regression; (R) isolate why the earliest tested SAROO builds already fail. Do not merge these into one presumed root cause. Treat H1-H6 as independent until measurements exclude them.

## 3. Architecture and boundaries

```text
Physical Saturn BIOS / supported ODE
    |
    | reads IP system identification and authenticates disc
    v
Reviewed security/area-code payload (separate provenance and policy)
    |
    | BIOS loads the first-read application using ISO9660 directory order
    v
LibSaturn-owned AIP (src/core/startup/aip.s)
    |
    | minimal, deterministic handoff; no unexplained CD command hooks
    v
0x06004000 : _start (existing crt0.s)
    |
    | stack, BSS/WRAM-L, vectors, main()
    v
sat_init() -> early graphics/SCU -> game UI -> CD/music
```

Ownership rules:

- **LibSaturn owns:** metadata serialization, AIP source, entry/handoff contract, link/load checks, image validation, boot diagnostics, Make and CMake integration, test fixtures.
- **Security/area payload:** sourced separately from a legally distributable, documented compatible source or provided by the end user; track origin, license, region coverage and exact hashes. Never casually replace authenticated security code with zero-filled bytes or an arbitrary reimplementation. Code in region payloads **may execute before the AIP**, so audit its control flow and CD commands separately.
- **BIOS owns:** disc authentication and first-read loading. Do not duplicate CD I/O in the AIP unless an isolated test proves it is essential and it is implemented with checked status and bounded error handling.
- **Existing application ABI remains:** first-read destination `0x06004000`, `_start` at the first byte, SH-2 big-endian application binary, established WRAMH/WRAML and stack contracts. Any ABI adjustment requires an explicit linker, startup, loader and test migration.

The native AIP should normally perform only the proven-required entry setup and an explicit jump to `_start`. Do not reset VDP/SCU, change vector bases, or issue CD commands speculatively while the BIOS is handing off. Each necessary register write needs a manual reference and a hardware test.

## 4. Delivery phases

### Phase 0 — Preserve a reproducible baseline (no behavior changes)

Tasks:
- Capture the tested Satiator/SAROO console revision, region, BIOS revision, ODE firmware/menu versions, SD card format, cold/warm start and exact observed screen. Record whether the licensed text animates or video timing continues.
- Archive the **user-supplied** `dino_demo_v0.2 (1).zip` and `dino_demo_v0.4.zip` and acquire the original public itch.io v0.3 ZIP. Record SHA-256, CUE/BIN/ISO, IP sector, directory records, first-read payload and artifact provenance for each. Treat the original user-supplied ZIPs as immutable inputs; do not silently substitute recompiled images. Seek the separate **first Satiator-working build**, without guessing its version. The user's report is a valuable pass observation, but the exact build/firmware combination remains to be reproduced.
- Produce new **byte-identifiable** current outputs from a pinned commit. Save `.elf`, `.map`, `.app.bin`, `.IP.BIN`, `.iso`, `.bin`, `.cue`, compiler versions and SHA-256 manifests.
- Preserve and invoke `harness/build-dino-satiator-triage.ps1` and the findings in `docs/DINO_SATIATOR_BOOT_INVESTIGATION.md`. Keep existing `with_bar`, `without_bar`, `cmok_wait` and `legacy_poll` artifacts traceable; an image name is not evidence of different behavior.
- Record all bootstrap bytes changed between the oldest recovered physical Satiator pass and a current failure, not only source-level diffs.
- Maintain independent `Satiator: pass -> fail` and `SAROO: fail from first tested build` timelines. Never transfer success/failure conclusions between devices or firmware revisions.

**Gate P0:** a reproducible old/new artifact manifest exists; hardware-passing status is marked **unverified** unless a tester actually reproduces it.

### Phase 1 — Expose the *real* boot path

Tasks:
- Add an **actual BIOS disc boot mode** to the test tooling, separate from the existing Ymir direct-injection probe. Neither injecting `0.BIN` into RAM nor forcing PC to `0x06004000` counts as a boot pass.
- Start with the already documented Mednafen launch path for real BIOS -> disc handoff. Explore Ymir's low-level CD Block mode with **user-provided**, legally acquired CDB firmware; note that emulator CDB timing does not model Satiator or SAROO firmware.
- At stable checkpoints log Master SH-2 PC/SR/VBR/R15, IP/AIP address range, first-read file location and size, CD HIRQ/CR1-CR4, interrupt masks, first instructions executed at `0x06004000`, and first write of a bootstrap stage marker.
- Classify failure into exactly one stage: (A) security/region, (B) 1st-read transfer, (C) AIP branch, (D) crt0, (E) `sat_init`, (F) video/progress initialization, (G) first disc/audio read.
- When the emulator cannot supply trustworthy information, classify as **unknown**, not pass. Keep traces alongside ROM/build hashes.

### Phase 1A — Mandatory `examples/optical_emulators_tester/` diagnostic example

This is a **required deliverable before replacing the default boot template**. Implement a reusable, low-dependency Saturn example and keep its tests as part of the supported LibSaturn CI/hardware acceptance suite. The name is **`optical_emulators_tester`** (not `boot_probe`); it subsumes the earlier minimal-probe idea.

Proposed deliverables:
- `examples/optical_emulators_tester/Makefile.inc` — two build profiles: `minimal` (no CD I/O until explicitly requested) and `full` (CD diagnostics). Both produce valid ISO + raw BIN/CUE images.
- `examples/optical_emulators_tester/main.c` — low-level, controller-driven diagnostic menu, built with standard LibSaturn APIs wherever available; no model, 3D assets, music, loading bar or dynamic allocation required to reach the first frame.
- `examples/optical_emulators_tester/diagnostics.[ch]` — table-driven steps, bounded CD Block requests, structured statuses and failure snapshots. Factor in the repository's preferred API boundaries rather than duplicating a CD driver.
- `examples/optical_emulators_tester/README.md` — exact Satiator and SAROO launch/test instructions, controls, interpretation and limitations.
- An opt-in, **pre-video boot-stage marker** in startup/AIP/runtime plumbing (e.g. an agreed writable RAM/mailbox location, selected only after checking linker/BIOS/ODE memory ownership); marker storage and lifetime must not collide with BIOS vectors, IP loading, stacks, RAM expansion or CD transfers.
- `harness/tests/test_optical_emulators_tester.py` plus a full-BIOS boot test that actually loads the disc. The existing injection harness remains a *separate* test.

Stage contract (exact numbers and storage to be finalized after memory-map validation):

| Stage | Marker/location | Observable result |
| --- | --- | --- |
| 00 | Host/emulator trace only: security and region/IP execution | IP entered, region code ran, or stopped before AIP; **the application cannot report this stage** |
| 01 | AIP handoff (when present) | Branch toward first-read executable |
| 02 | `crt0.s` | Entry and basic memory setup reached |
| 03 | `main()` entry | Application control acquired |
| 04 | Before/after `sat_init()` | Early hardware configuration isolated |
| 05 | First VBlank and visible frame | Display usable; on-screen diagnostic panel becomes available |
| 06 | CD Block status/command handshake | Capture HIRQ/CMOK and CR1–CR4 without hanging |
| 07 | ISO9660 first-file/metadata check | Disc root/first-read metadata reported consistently |
| 08 | Bounded sector read and content check | Sector data/CRC or known test signature matches |
| 09 | Opt-in sustained CD/stream read | Throughput, timeout and recovery counters recorded |

Operational requirements:
1. **Never make progress depend on an unbounded CD wait.** Each command and polling loop must have a documented deadline, status/error mapping and a readable failure snapshot. Use a monotonic hardware-safe time base; do not guess raw loop iteration timing.
2. The example must display the *last completed step*, current step, failure category and HIRQ/CR1–CR4 when available. Provide controller actions for next test/retry/details, and an idle start state that does not automatically issue CD commands.
3. A screen that never appears is **not** evidence that stage 00 failed. Pre-video milestones are obtained from debugger/RAM trace where supported; without them classify the stage as **unknown**.
4. Do not detect Satiator/SAROO through undocumented register probes or write into arbitrary cartridge/MMIO windows. Ask the tester to label the ODE and firmware in results; hardware identity may remain unknown.
5. Ship a tiny known-sector test file with an explicit signature/hash. Keep it in the ISO *after* the first-read application, and validate its location; never allow a test asset to accidentally become file ID 2.
6. Offer **SAROO disc mode** (`/SAROO/ISO/` with a validated CUE/BIN or ISO) and **SAROO direct-binary mode** (`/SAROO/BIN/`) as a controlled differential with the **same application payload**. The direct loader does not reproduce BIOS/IP startup state; it cannot validate the IP itself.
7. Log at minimum: test image SHA-256, build/profile ID, BIOS region, ODE/firmware/menu version supplied by tester, launch mode, stage marker, HIRQ/CR snapshot, elapsed wait, LBA/FAD, bytes requested/transferred and error/timeout count. Export machine-readable JSON/CSV **on the host side** using captured logs, screenshots, serial output or memory dumps; do not assume the Saturn can write a JSON file to the optical image.
8. Keep diagnostics recoverable after failed CD commands when hardware permits; a failed optional test must not prevent access to results. A true CPU hang still requires external PC/RAM trace and must be marked as such.

**Gate P1A:** normal toolchains build the minimal and full example with stable hashes, the minimal image reaches a first rendered diagnostic frame in full-BIOS emulation, each opt-in command has a tested timeout/error path, and the SAROO direct-BIN/ISO comparison is documented. **Physical ODE results remain pending** until testers run it.

**Gate P1:** the CI/local harness can distinguish injection success from authentic BIOS boot; an early-PC/last-stage report can identify where execution stopped.

### Phase 2 — Implement a LibSaturn-owned IP/AIP

Proposed files (names may be adjusted before implementation):
- `src/core/startup/aip.s` — independently authored, minimal SH-2 AIP and documented register/stack assumptions.
- `tools/build_native_ip.py` — deterministic 32 KiB IP composer; explicit metadata, layout bounds and input payload hashes.
- `tools/check_native_ip.py` — structural and behavioral validators; reject incompatible payloads or overlap.
- `assets/boot/README.md` — component provenance, regions, third-party licenses and reproducible inputs.
- Tests for binary layout, jump target, instruction decoding and ABI invariants.

Implementation rules:
1. Use the original Sega system-disc/IP specification and repository hardware manuals to verify **all** header offsets, sizes, endianness, character fields, region information, IP load sizing, initial program entry and stack semantics. Explicitly check `0x0E4` (reserved field) and `0x0EC` (Slave SH-2 stack), including the observed v0.2/v0.4 values, and construct controlled header-only variants; do not silently fix other fields at the same time. Do not carry over unexplained YAUL fields.
2. Keep security and region code as separately versioned payloads, with reproducible hashes, documented provenance and a clear license/distribution decision. If suitable rights cannot be established, support a user-supplied local payload and do not bundle it.
3. Inspect control flow **within the region payload**, including the historical `0xE00–0xEFF` area and HIRQ.CMOK handler. A native AIP placed after this code cannot fix a hang before the AIP. Never execute a CD command and then consume CR results without confirmed completion; a timeout should record diagnostic state rather than silently proceed.
4. Explicitly define where the AIP bytes are mapped inside the IP and their runtime address. Compile with the SH-2 toolchain and assert alignment, branch reach, endian encoding, section bounds and relocation constraints. The simple `jmp 0x06004000` sketch is a **design sketch**, not a validated drop-in loader.
5. Minimize pre-handoff register/MMIO touches. Keep existing `crt0.s` entry behavior until targeted evidence supports a change.
6. Require byte-for-byte reproducible output for the same inputs; fail closed on missing security data, unknown profile, invalid range, overlap, or first-read overflow.
7. Preserve a named `yaul`/`sbl` comparison mode during migration; never make the new profile default merely because a synthetic ROM or direct-injection harness passes.

**Gate P2:** the native IP can be generated and disassembled from source; it contains no copied YAUL/SBL bootstrap code, and each non-LibSaturn security/area component has an auditable provenance/license policy.

### Phase 3 — Audit disc packaging and early startup

- Extend `tools/check_iso.py` to parse ISO9660 primary volume descriptor and root directory **in on-disc order**; confirm the first-read directory identifier is the intended `0.BIN`, verify its starting LBA, byte length and contents against the built `.app.bin`.
- Ensure the proper System ID/IP is present at the correct first logical sectors; check the AIP address/size against actual generated IP, physical sector packing, raw MODE1/2352 EDC/ECC and CUE metadata. Validate both Make and installed-CMake disc builds produce the same logical disc layout.
- Assert `_start == 0x06004000` and that the actual first-read file covers all initialized loadable bytes but not `.bss`/`.wram_l`. Check linker map, `objcopy -O binary` output, WRAM-H end, stack reserve and WRAM-L boundaries.
- Review `src/core/startup/early_init.c`, `crt0.s`, `saturn.ld`, `src/hal/scu/irq.cpp` and `src/hal/scu/scu.cpp` against the official SCU register map. Investigate the use of `0x20C00000` and indexed writes before correcting them; no unreviewed "base address substitution".
- Check BIOS-to-runtime vector handoff and interrupt prerequisites, SH-2 cached/uncached aliases, Master/Slave stack overlap and whether `sat_init()` can leave the last BIOS frame onscreen while the game is already spinning.
- Add a **tiny bootstrap marker**, as safe pre-video instrumentation, plus optional phase-specific screens after VDP initialization. Stage markers should be inspectable from RAM or debugger even when no pixels change.

**Gate P3:** image validation catches a deliberately reordered root, mismatched executable, malformed IP, invalid memory span and wrong image profile. Startup is observable independently of the progress-bar implementation.

### Phase 4 — Controlled regression experiment

Construct immutable, hash-labeled test artifacts with one variable changed at a time:

| Set | Variables | Interpretation |
| --- | --- | --- |
| A | Original itch.io v0.2, v0.3, and VC Decide-tested v0.4 binary images versus the contemporary build (preserve each original IP) | Reproduce the *reported failures* and establish exactly what these releases contain; none is assumed known-good. Compare separately against any independently obtained, proven working early Satiator image |
| B | Same minimal `0.BIN`, YAUL vs SBL vs native IP | Isolate pre-application boot behavior |
| C | Same IP, minimal no-video `0.BIN` -> first marker -> hello-world video | Separate handoff, crt0, sat_init and display |
| D | Same native IP, early Dino source vs current source | Narrow runtime changes |
| E | Same current source/native IP, progress bar off/on | Isolate progress UI without changing boot |
| F | Same current source/native IP, CD/audio disabled/enabled | Isolate post-boot disc reads |
| G | Same current source/native IP, hi-res, ON_SUBMIT, VDP2 light independently toggled | Separate the simultaneous changes introduced around `920023e` |
| H | Same executable/IP payload, change only `0x0E4`, then only `0x0EC`, then both (after manual validation) | Test suspected System ID defects individually on SAROO and Satiator; reject unsupported values instead of declaring a fix |
| I | Exactly the same `optical_emulators_tester` application through SAROO `/SAROO/BIN/` versus `/SAROO/ISO/`, then ISO versus validated CUE/BIN | Separate SAROO BIOS-hook/first-read or image-format problems from post-entry application issues |
| J | Minimal/full `optical_emulators_tester`, current Satiator and SAROO firmware versions documented separately | Determine earliest failure stage and whether CD command/streaming causes hangs |

For every pair record IP SHA, executable SHA, ISO9660 file ordering, CUE/BIN SHA, boot-stage markers, last Master PC, HIRQ state and observed output. Keep the compiled executable identical for IP-only comparisons. Do not confuse a resized/generated model or changed music asset with an isolated progress-bar toggle.

Do not assume an "early without bar" source commit is physically proven good. Use a genuine recovered image or a fresh test from a physical Satiator user to establish that fact.

**Gate P4:** at least one failing/passing pair narrows the regression to a specific pre-application or application stage; if not, report that the root cause remains open.

### Phase 5 — Physical acceptance: Satiator and SAROO

Create a read-only **`optical_emulators_tester`** package with paired minimal/full `.app.bin`, `.iso`, and matched `.bin`/`.cue`, short device-specific READMEs, image hashes, a host-side results template and tester instructions. Provide only legally shareable artifacts. Keep the Dino releases as separate regression fixtures; the optical tester is the primary reusable hardware diagnostic.

Record for each result:
- Console motherboard revision, BIOS/region, NTSC/PAL; ODE model, firmware/menu version; loader entry mode, card/SD filesystem and image format.
- Cold boot, reset/reboot, repeatability, time on licensed screen, first bootstrap stage reached, first rendered frame, pad response and whether gameplay stays responsive.
- For the Dino: progress bar off/on comparison, animation, sound/music initialization and a five-minute stability run.
- Whether failures occur before or after `0x06004000`. Where precise PC is not available, report visual observations without inferring the PC.
- SAROO cartridge mode may conflict with programs needing a physical 4 MB expansion cartridge (e.g. `city_walk`); classify such results separately from basic boot compatibility.
- Perform a SAROO-only differential: run a known minimal `.app.bin` through the documented `/SAROO/BIN/` direct-binary launcher and compare it to the **same application payload** inside a normal `/SAROO/ISO/` disc. The direct loader skips the normal ISO/IP handoff and is a diagnostic, **not** a drop-in equivalent to Saturn BIOS state.
- For ISO mode, inspect [`Firm_Saturn/game_load.c`](https://github.com/tpunix/SAROO/blob/master/Firm_Saturn/game_load.c) (`bios_cd_cmd`, `cdc_read_file(0, 2, 0)`, `read_1st`) and MCU [`Firm_MCU/Saturn/saturn_cdc.c`](https://github.com/tpunix/SAROO/blob/master/Firm_MCU/Saturn/saturn_cdc.c) (CD commands/CMOK). Log failures before any binary handoff and avoid treating SAROO's BIOS hooks as identical to Satiator's.
- Verify the tested SAROO firmware/menu revision with VC Decide rather than claiming all firmware revisions were tested from a generalized recollection. Test `.cue`/raw BIN and `.iso` packaging explicitly, and treat `play_delay` / `pend_delay` / `sector_delay` configuration as controlled hypotheses, never presumed cures.

Minimum promotion criteria:
- **10 consecutive cold boots on one Satiator-equipped Saturn and one SAROO-equipped Saturn** with the *same native boot artifact*, plus a soft reset and five-minute Dino stability run on each.
- The **`optical_emulators_tester` minimal and full optical-disc variants** must start and report stage/status/error snapshots on both devices, with opt-in CD/ISO/sector tests recorded; a separate non-Dino LibSaturn example must also start.
- The SAROO direct-BIN versus disc-mode A/B test must be run and reported even if either path fails. Report stage reachability, not just a pass/fail screenshot.
- Normal ROM stays compatible with full-BIOS Mednafen boot, as well as the existing injection harness (the two results must be labeled separately).
- No new early-stage MMIO faults, interrupted CD operations, stack overruns or regression in Make/CMake/ISO checks.
- A confirmed hardware test result is mandatory. No emulator screenshot alone allows the claim "fixed on Satiator/SAROO".

**Gate P5:** physical results and identifiers are recorded in a machine-readable acceptance table and attached to an issue/commit.

### Phase 6 — Make native boot the default, safely

- Add an explicit `IP_TEMPLATE_KIND=native` (or a separately named `BOOT_PROFILE=native`) with a documented feature gate. Wire it consistently through `Makefile`, `build-example.ps1`, `cmake/LibSaturnBuildDisc.cmake`, installed package content, Conan and consumer tests.
- Keep `yaul` and `sbl` as selectable legacy profiles for comparison during a defined deprecation period. Warn when a legacy template has a known unbounded or unsafe wait.
- Make the native profile default only after Gate P5 **and Gate P1A**; write a concise migration note and update `docs/HARDWARE_ACCEPTANCE_CHECKLIST.md`, `docs/PACKAGING.md`, the harness and README. Keep `optical_emulators_tester` buildable as a public example across subsequent releases.
- Release binaries with clear build IDs; report precisely whether native AIP, bootstrap/area payload, first-read validation, or runtime initialization produced the fix. Do not attribute causality to the last change alone.
- Rollback is a one-variable switch to the previously passing profile, retaining artifact hashes and test evidence. Do not rewrite Git history or delete known-good boot images.

**Gate P6:** default and legacy modes are reproducible; CI, two hardware paths and downstream installed consumers pass.

## 5. Highest-value first implementation batch

The first engineering PR/commit batch should stop short of changing the default:
1. Implement **`examples/optical_emulators_tester/`** with `minimal` and `full` variants, documented boot-stage markers and opt-in bounded CD/ISO/sector diagnostics. Make it a permanent public example, not a disposable `boot_probe`.
2. Implement a pure parser/report tool for generated IP + ISO9660 first-read metadata and create single-field `0x0E4` / `0x0EC` A/B fixtures; require manual-spec validation before calling a value invalid.
3. Add a true BIOS boot runner (Mednafen first) whose output cannot be mistaken for the injection-based harness. Save PC/HIRQ trace when possible; run the optical tester through it.
4. Run both legacy boot templates against the **same** minimal tester executable. On SAROO, compare direct-BIN and disc mode; on Satiator, preserve the reported earliest working build and bisect the later failure.
5. Run the `minimal` and `full` hardware diagnostic images on both ODEs with tester-supplied firmware details, retaining failures and stage evidence.
6. Only then implement the native AIP/IP composer based on a validated IP specification and provenance-reviewed security/area inputs.

This ordering can distinguish an ODE/BIOS failure from a broken application startup before large-scale boot rewrites.

## 6. Tests, evidence and definition of done

Host tests:
- IP metadata field encoding, strict allowed lengths, big-endian integers, reserved bytes, region coverage, security asset digest/provenance.
- AIP ELF link and relocation assertions, binary signature/jump target, address alignment, length bounds.
- ISO9660 root-order fixture: wrong first file fails; wrong LBA/size/content fails.
- In-place sector/padding checks, fresh Make/CMake parity, invalid/oversized `0.BIN` failure.
- CMOK command-flow regression tests on **each** legacy boot payload, including the bounded SBL and unbounded YAUL variants.
- `optical_emulators_tester` host tests for finite timeouts, CD response decoding, status transitions, test-fixture hash, stage marker sequencing, minimum mode with CD commands disabled by default and dual build modes.
- Header-only `0x0E4` and `0x0EC` fixtures, including isolated mutations and a test that detects changes in any unintended IP bytes.

Emulator tests:
- Full BIOS boot reaches `_start` and a stage marker without manual PC or RAM injection.
- Optional user-provided CDB firmware handled without including it in the repository.
- Existing injection-based Ymir and Dino runtime tests continue passing but are never relabeled "disc boot".
- `optical_emulators_tester` startup in full-BIOS emulation, with a check that stage 05 was reached, and a separate opt-in full-mode test with captured HIRQ/CR responses.

Physical tests:
- The exact matrix from Phase 5, with photos/videos and a hash-backed result file.
- Failure results kept alongside passing results, with confidence levels.
- `optical_emulators_tester` minimal/full mode and SAROO direct-BIN/ISO results, carrying a build hash, stage reachability, BIOS/ODE version and observable CD status.

**Definition of done:** every shipped image has an auditable and independently owned AIP, properly sourced authenticated payload, deterministic disc layout, genuine full-BIOS emulator coverage and demonstrated repeatable boot on both Satiator and SAROO; `examples/optical_emulators_tester/` remains usable as a buildable, documented and hardware-accepted permanent diagnostic. The Satiator Dino regression and SAROO early incompatibility are *individually explained*, not merely hidden by turning off the progress bar.

## 7. Source map

Repository:
- [Current Dino/Satiator investigation](DINO_SATIATOR_BOOT_INVESTIGATION.md)
- [Historical IP-template notes](../assets/boot/README.md)
- [IP generator](../tools/gen_ip_bin.py), [IP validator](../tools/check_ip_bin.py), [ISO validator](../tools/check_iso.py), [raw image validator](../tools/check_disc_image.py)
- [IP boot stub](../src/core/startup/ip_stub.s), [crt0](../src/core/startup/crt0.s), [linker script](../src/core/startup/saturn.ld), [early init](../src/core/startup/early_init.c)
- [Dino source](../examples/dino_demo/main.c), [Dino build profile](../examples/dino_demo/Makefile.inc), [disc CMake helper](../cmake/LibSaturnBuildDisc.cmake)
- [Harness limitation](../harness/README.md), [physical acceptance checklist](HARDWARE_ACCEPTANCE_CHECKLIST.md)
- [Imported Sega hardware manuals](sega_saturn_hardware/)

External references to inspect and cite during implementation:
- Sega Saturn *Disc Format Standards* and *System Program Library/boot* documentation (use authoritative scanned manuals, confirm version and offsets).
- Sega Saturn *SCU User's Manual* (MMIO addresses, interrupt mask/status), plus the SH-2 hardware manual.
- [Satiator official software/firmware notes](https://wiki.satiator.net/start#software_updates) and [official menu](https://github.com/satiator/satiator-menu).
- [Public Dino Demo itch.io release and its v0.2/v0.3/v0.4 archive listing](https://celsowm.itch.io/dino-demo-saturn-version); [physical failure reports for these versions](https://www.reddit.com/r/SegaSaturn/comments/1x4edil/dino_demo_saturn_version_04/).
- [SAROO project](https://github.com/tpunix/SAROO).
- [Ymir](https://github.com/StrikerX3/Ymir) and [Mednafen](https://mednafen.github.io/).

Implementation work starts with diagnostics and reproducibility, *not* an unverified rewrite of the working gameplay.
