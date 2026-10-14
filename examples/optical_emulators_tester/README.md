# Optical Emulators Tester

A permanent, allocation-free boot and CD diagnostic for Satiator, SAROO and
emulators. It reaches its first screen using resident text data. It does not
start audio, load models or issue CD commands during startup.

## Build and run

```powershell
./build-example.ps1 optical_emulators_tester
./run-example.ps1 optical_emulators_tester -MednafenCart none

$env:OPTICAL_TEST_PROFILE = 'full'
./build-example.ps1 optical_emulators_tester
Remove-Item Env:OPTICAL_TEST_PROFILE
```

`minimal` disables CD operations. `full` enables opt-in status/initialization,
ISO lookup, 1/2/32-sector verification and a 30-second repeated-read test.
Both start idle. D-pad selects, **A** runs/retries, **C** shows register details,
and **B** returns from details or cancels the sustained test between reads.
The menu reports completion or failure after each bounded request. During a
request the previous diagnostic frame stays visible; cancellation can take up
to the current operation deadline. Register values on screen are decimal.

Command waits have a 2-second deadline; an entire HAL operation has a
10-second deadline, including repeated successful commands that deliver no
sectors. These are named settings in the example's timed transport config.
The clock uses the library's calibrated, FRT-backed monotonic time after
`sat_init`. A clock callback supplied by another caller must keep advancing.
The legacy CD API still uses its historical iteration budget.

The fixture is exactly 32 sectors (64 KiB), begins with `LIBSATURN ODE V1`, and
is generated from an explicit byte pattern. Its CRC32/SHA256 metadata is generated
alongside it. `0.BIN` must remain the first root directory entry after dot entries;
`ODETEST.BIN` follows it. The host validator checks actual directory order and
the executable bytes, rather than trusting names or the header's size field.

## Boot profiles and packages

Native is the default. YAUL and SBL are selectable comparisons
(`-IpTemplate yaul|sbl`). The local security/region inputs are extracted
automatically from the tracked YAUL template:

```powershell
./build-example.ps1 optical_emulators_tester
./harness/build-optical-packages.ps1
./harness/make-share-bundle.ps1      # native minimal+full zip for testers
```

Extraction records origin and hashes; it does not establish redistribution rights.
The native AIP is independently authored SH-2 source. It writes an optional
application-owned trace marker and branches to `0x06004000`, with no CD commands,
video reset or vector changes. It replaces the inherited initialization code
after the region records, including the legacy CMOK hook. This is an experimental
difference, not a confirmed hardware fix.

Packages are generated under `harness/build/optical-packages/`. Minimal/full
application hashes must agree across all three boot profiles. Local disc outputs
include ISO, raw MODE1/2352 BIN/CUE, application, ELF/map, IP and hash reports.
`applications-only.zip` contains no IP/security/BIOS payload. Complete disc outputs
are marked **not cleared for redistribution** until their payload rights are
reviewed. No BIOS or CDB firmware is included.

For Satiator, copy one package's matched BIN/CUE into its own game folder. Test
the standalone ISO in a separate folder, without mixing the two formats.
For SAROO disc mode, place each matched image set in its own directory under
`/SAROO/ISO/`. For the direct-loader comparison, copy the **same** application
bytes as a `.bin` file under `/SAROO/BIN/`; the upstream loader defaults to
`0x06004000`. Select the normal binary launch, recording any alternative loader
action separately. Direct mode does not load/mount this test disc automatically;
use it primarily to compare startup and first-frame reachability.

Record the exact console/BIOS, ODE firmware/menu, card filesystem, image hash,
launch mode, cold/reset attempt, visible screen and register snapshots in
`hardware-results.csv`. A licensed-screen freeze alone does not identify the
failing CPU stage. Physical acceptance is pending until measured on each device.

## Startup tracing and emulation

The optional initialized `.boot_trace` record belongs to the application and is
resolved through the map/ELF, rather than a fixed BIOS, cartridge or shared-memory
address. crt0 preserves it while clearing BSS. Uninstrumented applications have
no trace allocation. The native AIP writes stage 1; legacy AIPs do not.

| Stage | Meaning |
| --- | --- |
| 1 | Native AIP reached |
| 2 | crt0 entry; incoming SR/VBR/SP captured |
| 3 | crt0 memory setup complete |
| 4 | `main` entered |
| 5 | Before `sat_init` |
| 6 / 7 | Before / after early hardware initialization |
| 8 | `sat_init` complete |
| 9 | First VBlank/frame submission complete; panel and heartbeat visible |
| 10 / 11 | CD handshake / ISO test |
| 12 / 13 | Sector verification / sustained test |

The record includes current/completed stage, reached bitmap and result. Stages
10–13 complete only on success. Pre-AIP security/region execution is observable
only externally; absent observations are **UNKNOWN**. RAM is volatile and this
record does not survive a reload of the application.

```powershell
# Existing direct-injection runtime test, explicitly labeled as injection:
./harness/run-optical-tester.ps1 -Bios bios/saturn_bios_us.bin -TestProfile full -RunCdTests

# Authentic BIOS execution, with no application injection or forced PC:
./harness/run-optical-tester.ps1 -Bios bios/saturn_bios_us.bin -BootMode bios
# Add -CdbRom <your-local-dump> to select Ymir's optional low-level CD model.
```

Mednafen is the default interactive emulator and boots the real disc through the
BIOS. The Ymir BIOS mode records executed entry instructions, per-field Master
PC/SR/VBR/SP, HIRQ/CR1–CR4 and the current marker. It forbids the injection-only
Slave reset-vector patch. Ymir CD HLE failures must remain failures/unknowns;
injection success cannot replace a BIOS result. No firmware is downloaded by the
runner. Public CI runs firmware-free tests; BIOS/CDB runs require local inputs.

## Evidence and remaining acceptance

The baseline preserves original v0.2/v0.4 downloads separately from rebuilt Dino
images. A local v0.3 ZIP is archived with its source path; its original download
provenance remains unverified. No recovered version is asserted to be the first
Satiator-working image. Header comparisons independently change reserved `0xE4`,
Slave stack `0xEC`, or both, with identical application bytes.

The early initializer no longer writes to `0x20C00000` as though it were SCU.
SEGA's SCU map places IMS at `0x25FE00A0` and prohibits writes to IST. CPU
interrupts are masked by crt0; the existing SCU HAL configures interrupts later.
`SAT_LEGACY_EARLY_SCU=1` retains the original two writes solely for isolated
diagnostic builds. This correction does not establish the ODE failure's cause.

Promotion still requires ten cold boots on each device, reset checks, minimal/full
diagnostics, another example and five-minute Dino stability on both devices.
See [Native Saturn Boot Plan](../../docs/NATIVE_SATURN_BOOT_PLAN.md).

Primary references: [SEGA Disc Format Standards, boot system](https://www.infochunk.com/saturn/segahtml_en/prgg/sofg/disc/hon/p04_10.htm),
[local SCU register map](../../docs/sega_saturn_hardware/hard/scu_/hon/p01_03.md),
[SAROO loader source](https://github.com/tpunix/SAROO/blob/master/Firm_Saturn/main.c).
