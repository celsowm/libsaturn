# Backup Memory cartridge support plan

Status: PLANNED / NOT IMPLEMENTED. Scope: persistent Saturn Backup Memory cartridge
through the BIOS Backup Library (BUP), **not** the volatile 1 MiB/4 MiB RAM
expansion cartridge and not a generic A-Bus memory allocator.

## Goal and existing baseline

Expose the already-reserved `SAT_SAVE_BACKUP_CARTRIDGE` device in
`include/saturn/save.h` by reusing the public internal-save operations:
status, directory, read, write/overwrite, verify, delete, and explicit format.
Cartridge-specific code belongs in the BUP HAL/device resolver, not in
per-operation copies of `save_api.cpp`. Keep BIOS save records visible to
the Saturn BIOS Memory Manager and preserve existing user saves.

Current pieces to reuse:

- `include/saturn/save.h`: public logical-device enum and record API;
- `src/core/save_api.cpp`: arguments, lengths, status and error mapping;
- `src/hal/bup.hpp` / `src/hal/bup.cpp`: BIOS vector calls, `Config[3]`,
  `BUP_SelPart`, and critical-section guards;
- `examples/save_backup_demo` and the file-backed Ymir harness, currently
  internal-memory only;
- `tests/host/test_save_api.cpp`: fake BUP transport.

Do not begin cartridge writes until the Phase 0 prerequisites below pass.

## Primary sources and an important index distinction

Sega's *Saturn System Library User's Guide*, Backup Library section,
ST-162-R1-092994, describes **BUP function `device` arguments** as
`0 = built-in backup memory`, `1 = memory cartridge/parallel interface`,
`2 = serial interface`. The `BupConfig.unit_id` values are a different
namespace: `0 = disconnected`, `1 = internal`, `2 = external cartridge`.
Do not use `unit_id` as the `device` argument without verification.

- Sega guide: https://antime.kapsi.fi/sega/files/ST-162-R1-092994.pdf
- SDK structures/prototypes: https://github.com/johannes-fetz/joengine/blob/556d081146211b6a1cfa6591d70f9487d406758b/Compiler/COMMON/SGL_302j/INC/SEGA_BUP.H
- A separate BIOS disassembly describes a cart-specific `R4 == 2` dispatch,
  conflicting with the guide's logical-device table. Treat this as an
  unresolved ROM/API detail to settle with a controlled probe, **not** as a
  reason to assume all BIOS revisions use the same cart index:
  https://github.com/user-none/erings/blob/96e2cbab8ba8cf3e89bec3954f96c2b14b79f1dd/docs/bios/backup_library.md

The internal path was corrected to pass **BUP device index 0** in
`src/core/save_api.cpp`; host tests now assert that index separately from
`Config[0].unit_id == 1`. This is a verified *source/host-test correction*,
not yet BIOS-backed runtime acceptance. Establish the external index from
`Config[3]`, the guide, the BIOS probe and emulator/hardware observations.
Record the resulting index table and ROM version in a short BUP contract note.

Ymir's **pinned** core supports external backup cartridges: construct a
`ymir::bup::BackupMemory` and insert it with
`Saturn::InsertCartridge<ymir::cart::BackupMemoryCartridge>(...)`.
Unlike `SystemMemory::LoadInternalBackupMemoryImage`, the external cartridge
does not use an internal-memory loader. The pinned cartridge class accepts
512 KiB, 1 MiB, 2 MiB, or 4 MiB backup images. Start with a fresh **512 KiB**
image; let BIOS `BUP_Stat` determine its usable capacity and block geometry.

- Pinned Ymir guide: https://github.com/ymir-emu/Ymir/blob/5c571f680e3606de08813af4de43dc00e90cc6e3/libs/ymir-core/docs/mainpage.hpp
- Pinned cartridge: https://github.com/ymir-emu/Ymir/blob/5c571f680e3606de08813af4de43dc00e90cc6e3/libs/ymir-core/include/ymir/hw/cart/cart_impl_bup.hpp
- Pinned image API: https://github.com/ymir-emu/Ymir/blob/5c571f680e3606de08813af4de43dc00e90cc6e3/libs/ymir-core/include/ymir/sys/backup_ram.hpp

## Phase 0 — make the existing internal path trustworthy (hard gate)

1. Correct the device-index/unit-ID conflation in both
   `src/core/save_api.cpp` and `tests/host/test_save_api.cpp`. Assert the
   actual BIOS arguments in the fake HAL, separately from `Config.unit_id`.
   Do **not** blindly change the cartridge mapping to 2; establish it by probe.
2. Revalidate the SH-2 save-specific build. At run
   https://github.com/celsowm/libsaturn/actions/runs/35417196472,
   `examples/save_backup_demo/main.c` failed because the freestanding
   toolchain could not find `string.h`. The demo now uses a freestanding-safe
   local byte comparison; build validation still must be checked on the
   resulting commit, including any subsequent compiler/linker errors.
3. Run the existing *two-process* internal persistence test with a locally
   supplied legal Saturn BIOS; verify `LIBSAT_DEMO` counts 1 then 2 and
   that the BIOS's own directory agrees with the Ymir-side record. No BIOS
   binaries or private save images may be committed.
4. Before the first external mutation, make a raw checksum/backup of the
   intended test cartridge and verify that only the explicitly selected
   throwaway image/device can be formatted or deleted.

**Exit:** corrected host arguments, save-specific SH-2 ISO build, and real
internal runtime/persistence acceptance all pass. If the BIOS mapping remains
ambiguous, stop before enabling external writes.

## Phase 1 — discover device and partition contract (read-only)

1. Expose an internal discovery function over `Config[3]` captured by
   `BUP_Init`. Save both configuration **array index** and `unit_id`;
   identify the cartridge from observed populated entries, but validate its
   actual callable BIOS device selector separately. Missing cart, unsupported
   cartridge type, unformatted storage, and transport failure are distinct
   outcomes.
2. Query `BUP_Stat` using only verified selectors and log raw results plus
   `total_size`, `total_blocks`, `block_size`, `free_size`,
   `free_blocks`, and `fit_count`. Do not guess size, mapping or block
   geometry from cart ID, RAM address, or nominal product capacity.
3. Document partition count from the selected `Config`, zero-vs-one-based
   `BUP_SelPart` numbering, and whether part selection is global or
   per-device on each tested BIOS. Never assume that a successful `BUP_Stat`
   on one partition describes the whole cartridge.
4. For `SAT_SAVE_BACKUP_CARTRIDGE`, reject missing/wrong-type cartridges
   without touching storage. Read-only enumeration must not trigger format.

**Exit:** absent/inserted/foreign-cart and partition behavior observed in
the pinned emulator and documented with the BIOS version. Status calls never
rewrite the cartridge image.

## Phase 2 — expose the cartridge in the common save API

1. Replace the reserved-unsupported mapping of
   `SAT_SAVE_BACKUP_CARTRIDGE` with a verified BIOS selector; keep the
   public enum stable and independent of BIOS numbers. Continue using
   `BUP_Init` only once per session unless a documented device-change event
   requires reinitialization.
2. Add a small public device-info query, if needed, for connected state and
   partition count; keep the existing `sat_save_storage_info` for current
   partition capacity. Document that the cart need not have internal-BRAM
   geometry or capacity.
3. Provide `sat_save_select_partition(device, partition)` only after its
   numbering is proven; validate against the discovered count. Define one
   serialized selection policy: operations resolve/select the intended device
   and partition immediately before a BUP call, and either restore prior
   selection or maintain an explicit active partition with predictable rules.
   Never let an internal-BRAM call accidentally target a selected cart.
4. Reuse `sat_save_list/read/write/verify/delete/format` without duplicate
   public paths. Keep the existing `ResetGuard` around BIOS mutators.
   Match errors for absent, unformatted, read-only, no-space, verification
   mismatch, and transport failure. Preserve file metadata.
5. No automatic selection of the cartridge as a fallback when internal BRAM
   is full. No implicit copy, migration, erase, partition change, or format.

**Exit:** the same save API handles internal and cartridge media, with
separate reported capacities and no cross-device side effects.

## Phase 3 — host and fake-BUP regression matrix

Create a fake BUP transport configurable with distinct internal/cart records,
different geometries and multiple partitions. Add tests for:

- internal vs external device-argument mapping independent of `unit_id`;
- no cartridge, wrong cartridge type, disconnected-after-init and corrupted
  configuration;
- zero/one/multiple partitions, invalid index and selection failures;
- interleaved internal/cart operations with **same filename** but different
  payloads: neither record is changed by operations on the other device;
- list patterns, truncation/out-total, short destination and no-space;
- overwrite off/on, verify mismatch, deletion of only the target record;
- read-only/unformatted errors with **zero** calls to format/write/delete;
- reset-disable/reset-enable pairing and recovery after failures;
- a failed write must not silently delete the previous good save.

The test runner must also prove that no API call allocates a cartridge-sized
buffer in Work RAM and that all output buffers remain caller-owned.

**Exit:** `make test` green, targeted SH-2 cart example cross-compiles, and
the existing internal tests still pass.

## Phase 4 — isolated Ymir cartridge acceptance harness

Extend `harness/src/probe_main.cpp` with an **explicit**
`--backup-cart <test-image>` option separate from
`--backup-ram <internal-image>`. In the pinned Ymir core, load a
`ymir::bup::BackupMemory` object, check load result and image size, insert
`ymir::cart::BackupMemoryCartridge` *before* guest execution, and inspect
its actual `GetBackupMemory()` after execution. Do not treat internal BRAM
state as evidence of cartridge success.

Only create/format an image automatically in an isolated, versioned harness
fixture directory whose content is disposable; note that Ymir
`BackupMemory::CreateFrom` can create/resize/format an image. Never call it
on an arbitrary user-supplied existing cart image. For an existing image
intended to be preserved, use the non-destructive `LoadFrom` path and do not
run a mutating test without explicit fixture opt-in.

The JSON must expose *both* media independently: connected/configuration,
partition, header validity, size, used blocks, directory, payload size/hash,
and a before/after raw image hash. Add:

- `examples/save_cartridge_demo`: display discovered device/partition and
  free space, then write/verify/read a namespaced throwaway record only after
  the user requests a test write; separate explicit confirmation for format;
- `harness/run-save-cartridge-persistence.ps1`: use the same cart image
  across two **fresh** emulator processes, leaving internal BRAM at a separate
  fixture path; assert cart boot count 1 -> 2 and unchanged internal data;
- guest-side status markers to distinguish **executed BIOS BUP operations**
  from a merely present Ymir-side file (do not use emulator-side import to
  fake a guest save).

Run no-cart, cart-inserted, unformatted, formatted, near-full and protected
scenarios where supported by Ymir; mark unsupported emulator conditions as
unsupported rather than claiming they were tested.

**Exit:** a BIOS-backed two-process persistence run proves an actual guest
cartridge write, and the internal image is byte-identical across cart-only
operations.

## Phase 5 — physical-hardware compatibility and recovery

On actual Saturn hardware, use a **disposable/test cartridge**, not an
existing user's saves. Test connected status, a normal write/verify/read,
overwrite, reboot persistence, directory visibility in the BIOS Memory
Manager, unplugged-at-boot behavior, invalid partition selection, and a
near-full condition. Also test one alternative compatible cart model/size
when available; keep unsupported third-party products explicitly excluded
instead of bypassing the BIOS via raw A-Bus writes.

For power interruption, preserve the prior save where possible using the
future optional versioned/two-slot convenience layer; do not claim the raw
BUP overwrite operation is atomic. Verify post-interruption recovery
separately; do not deliberately interrupt writes on a cartridge containing
valuable data.

**Exit:** documented emulator + hardware evidence for both read and write,
no regression to internal saves, and no unapproved destructive operations.

## Implementation snapshot (2026-09-19)

- Phase 0 source corrections: internal BUP function selector is now index
  `0`, independently of `Config[0].unit_id == 1`; matching fake-HAL host
  assertions were updated. The SH-2 demo no longer includes hosted
  `string.h`. These changes **still require** confirmation of the
  save-specific SH-2 build and the BIOS-backed internal persistence test.
- Diagnostic harness groundwork: the pinned Ymir probe now accepts an existing
  external Backup Memory image through `--backup-cart`. It loads the image
  copy-on-write, inserts a separate external cartridge, and reports its
  header, capacity, directory and in-emulator before/after hashes in the
  `backup_cartridge` JSON field. This is **not** proof that LibSaturn can
  read or write a cartridge; the guest-side cartridge code is still disabled.
- Phase 1 BIOS device/partition discovery and all cartridge mutations: pending
  the Phase 0 acceptance gate; do not turn on `SAT_SAVE_BACKUP_CARTRIDGE`
  merely because Ymir can mount an image.

## Delivery order and boundaries

Order: **Phase 0 -> 1 -> 2 -> 3 -> 4 -> 5**. The first implementation slice
after Phase 0 is **read-only discovery + status**. Keep the work in small
reviewable commits and preserve an explicit acceptance note for every phase.

Not in scope: volatile RAM expansion cartridge, direct memory-mapped file
system, hot-swap support, generic A-Bus driver, external-card auto-migration,
or silently formatting a user's cartridge. Those are separate projects.
