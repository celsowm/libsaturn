# SAVE / Backup RAM exploration plan

## Goal

Add first-class persistent save support to LibSaturn without coupling game code
to raw Backup RAM layout, BIOS vector addresses, or a specific storage device.

This work is intentionally separate from the volatile RAM expansion cartridge.
Saturn save storage (internal Backup RAM and Backup Memory cartridges) and RAM
expansion cartridges solve different problems and should remain separate
subsystems.

## Primary hardware/software contract

The Sega Backup Library is the source of truth for save storage.

Important constraints from the Sega Backup Library manual:

- applications should use the Backup Library instead of manipulating Backup RAM
  as a private raw filesystem;
- the built-in backup memory is 32 KiB;
- device 0 is built-in backup memory;
- device 1 is a memory cartridge / parallel interface;
- device 2 is a serial interface;
- devices may expose partitions, so storage capacity must not be hard-coded;
- callers should select the partition and query status/free space before writes;
- `BUP_Init` expands the Boot ROM backup library into a caller-provided 16 KiB
  program area and requires an 8 KiB work area during initialization;
- reset-button NMI must be disabled around operations that can leave persistent
  data inconsistent: `BUP_Init`, `BUP_Format`, `BUP_Write` and
  `BUP_Delete`;
- `BUP_Verify` exists and should be used after writes when correctness matters.

The public LibSaturn API must not expose the Boot ROM vector table or require
games to know those workspace details.

## BIOS entry points

The standard Sega `SEGA_BUP.H` interface resolves the Boot ROM library from
the system vector table rooted at `0x06000350`:

- library entry pointer: vector root + 8;
- BUP vector table pointer: vector root + 4.

The BUP function order is:

1. `BUP_Init`
2. `BUP_SelPart`
3. `BUP_Format`
4. `BUP_Stat`
5. `BUP_Write`
6. `BUP_Read`
7. `BUP_Delete`
8. `BUP_Dir`
9. `BUP_Verify`
10. `BUP_GetDate`
11. `BUP_SetDate`

LibSaturn should wrap these in `src/hal/bup.*` rather than copying SGL into
the public API.

## Public API direction

Add `include/saturn/save.h`.

Suggested types:

```c
typedef enum sat_save_device {
    SAT_SAVE_INTERNAL = 0,
    SAT_SAVE_BACKUP_CARTRIDGE = 1
} sat_save_device_t;

typedef enum sat_save_language {
    SAT_SAVE_JAPANESE = 0,
    SAT_SAVE_ENGLISH = 1,
    SAT_SAVE_FRENCH = 2,
    SAT_SAVE_GERMAN = 3,
    SAT_SAVE_SPANISH = 4,
    SAT_SAVE_ITALIAN = 5
} sat_save_language_t;

typedef struct sat_save_entry {
    char name[12];
    char comment[11];
    uint8_t language;
    uint32_t date;
    uint32_t data_size;
    uint16_t block_size;
} sat_save_entry_t;

typedef struct sat_save_storage_info {
    uint32_t total_size;
    uint32_t total_blocks;
    uint32_t block_size;
    uint32_t free_size;
    uint32_t free_blocks;
    uint32_t entry_count;
} sat_save_storage_info_t;
```

Suggested functions:

```c
sat_result_t sat_save_init(void);
sat_result_t sat_save_select_partition(sat_save_device_t device, uint16_t partition);

sat_result_t sat_save_storage_info(
    sat_save_device_t device,
    uint32_t prospective_data_size,
    sat_save_storage_info_t* out_info);

sat_result_t sat_save_list(
    sat_save_device_t device,
    const char* prefix,
    sat_save_entry_t* entries,
    uint16_t capacity,
    uint16_t* out_count);

sat_result_t sat_save_read(
    sat_save_device_t device,
    const char* name,
    void* destination,
    uint32_t capacity,
    uint32_t* out_size);

sat_result_t sat_save_write(
    sat_save_device_t device,
    const sat_save_entry_t* metadata,
    const void* data,
    uint8_t overwrite);

sat_result_t sat_save_verify(
    sat_save_device_t device,
    const char* name,
    const void* data);

sat_result_t sat_save_delete(
    sat_save_device_t device,
    const char* name);

sat_result_t sat_save_format(
    sat_save_device_t device);
```

### Safety decisions

- `sat_save_init()` must **never auto-format** an unformatted device.
  Formatting is destructive and must remain an explicit call.
- `sat_save_write()` queries status first and translates BUP errors into
  normal `sat_result_t` values. Final fit remains authoritative in
  `BUP_Write`, because overwriting can reclaim the previous record's blocks.
- The BIOS write flag is counterintuitive: `0` means overwrite an existing
  record and non-zero means fail with BUP_FOUND. LibSaturn maps its public
  `overwrite` boolean explicitly instead of exposing that quirk.
- The reset button should be disabled only for the smallest critical window and
  re-enabled on every exit path.
- The high-level API should preserve Sega metadata (name, comment, language,
  date, byte size, block size) so saves remain visible and useful in the Saturn
  BIOS Memory Manager.

## Layering

```text
Game
 |
 v
include/saturn/save.h
 |
 v
src/core/save_api.cpp
 |
 v
src/hal/bup.hpp / src/hal/bup.cpp
 |
 +--> SMPC RESDISA / RESENAB
 |
 v
Boot ROM BUP library
 |
 v
internal Backup RAM / Backup Memory cartridge
```

Do not map raw `0x00180000` Backup RAM into the public API. Raw access would
force LibSaturn to own Sega's on-media allocation format, fragmentation,
directory handling and compatibility rules that the BIOS library already
implements.

## Required SMPC extension

The current SMPC HAL already has the generic simple-command mechanism used for
sound ON/OFF. Add:

```cpp
bool reset_enable();   // RESENAB 0x19
bool reset_disable();  // RESDISA 0x1A
```

BUP write/delete/format/init guards should use an RAII-style internal helper so
reset enable is restored on every error path.

## Memory ownership

A first implementation can reserve static, aligned storage inside the BUP HAL:

- 16 KiB persistent expanded-library area;
- 8 KiB temporary initialization work area.

The 8 KiB work area can be local to initialization if stack budget is proven
safe; otherwise keep both areas static. Prefer static storage initially because
a 8 KiB automatic allocation is unnecessarily risky on a small bare-metal
stack.

Document the resulting permanent 16 KiB Work RAM cost in the API guide.

## Error mapping

Add save-specific result mapping without leaking Sega numeric BUP constants.

At minimum distinguish:

- not connected;
- unformatted;
- write protected;
- insufficient space;
- already exists;
- not found;
- verify mismatch;
- broken/corrupt data;
- invalid argument / unsupported device.

If the current global `sat_result_t` does not distinguish all of these, add
only the values that are broadly useful rather than exposing a parallel BUP
error enum to application code.

## Filename and metadata rules

The BIOS format uses:

- filename: at most 11 ASCII characters plus NUL;
- comment: at most 10 ASCII characters plus NUL;
- language field;
- packed date;
- data size in bytes;
- block size.

`BUP_Dir` accepts a filename pattern; `*` is the useful all-files wildcard.
Its return is a match count: positive when all results fit, negative when more
records matched than the provided output capacity. The absolute value is the
total match count.

LibSaturn accepts ordinary NUL-terminated strings, validates lengths, then
constructs zero-padded BIOS-facing structures internally. It never silently
truncates filenames because two logical save names could otherwise collide.

## Versioning of game payloads

The BUP record metadata describes the record, not the schema of the game data.
Games still need payload versioning.

After the raw BUP API is stable, add an optional convenience envelope:

```c
typedef struct sat_save_blob_header {
    uint32_t magic;
    uint16_t version;
    uint16_t header_size;
    uint32_t payload_size;
    uint32_t crc32;
} sat_save_blob_header_t;
```

This should be optional. The low-level save API must continue to read/write
arbitrary game-defined bytes for compatibility with existing Saturn save
formats.

## Tests

### Host unit tests

Use a fake BUP HAL, following the same pattern as the existing audio tests.

Cover:

- init success/failure;
- exact 11-character names;
- too-long names rejected;
- empty/null input validation;
- list capacity and count semantics;
- read capacity smaller than stored payload;
- overwrite off -> already exists;
- overwrite on -> successful replacement;
- no-space propagation;
- delete missing file semantics;
- explicit format only;
- verify failure propagation;
- reset-disable/reset-enable pairing around every mutating operation.

### Emulator acceptance

Add `examples/save_backup_demo`.

First boot:

1. initialize save subsystem;
2. inspect internal Backup RAM status;
3. if unformatted, display this fact instead of formatting automatically;
4. write a small versioned test record;
5. verify it;
6. read it back and display PASS.

Persistence acceptance should then launch a second emulator session with the
same Backup RAM image and verify that the record written in the previous run is
still present. A single-process read-after-write test is useful but is not
enough to prove persistence.

### Cartridge acceptance

Only after internal memory works:

- detect device 1;
- enumerate partitions/configuration;
- write/read/verify a test record on a real or emulated Backup Memory cartridge;
- keep this separate from RAM expansion-cartridge work.

## Implementation phases

### Phase 1 — BIOS BUP transport

- add `src/hal/bup.hpp` and `src/hal/bup.cpp`;
- define the BIOS structures and vector entry points privately;
- add SMPC RESDISA/RESENAB;
- implement init/select/stat/read/write/delete/dir/verify;
- no public API yet beyond what tests need.

### Phase 2 — Public save API

- add `include/saturn/save.h`;
- add `src/core/save_api.cpp`;
- add umbrella-header export;
- map BUP errors to `sat_result_t`;
- validate metadata and filename/comment lengths;
- explicit format API.

### Phase 3 — Host tests

- fake HAL;
- lifecycle/error tests;
- overwrite and capacity tests;
- reset critical-section tests.

### Phase 4 — Internal Backup RAM example

- add `examples/save_backup_demo`;
- write/read/verify/list/stat;
- never format automatically;
- visible diagnostics.

### Phase 5 — Persistent harness test

- preserve an emulator Backup RAM image between two harness invocations;
- first invocation writes a nonce/versioned record;
- second invocation proves it survived process restart.

### Phase 6 — Backup Memory cartridge

- expose device 1 only after detection is proven;
- support partitions through `BUP_SelPart`;
- test cartridge free-space and directory behavior;
- do not mix this work with the volatile RAM expansion cartridge subsystem.

### Phase 7 — Convenience layer

- optional versioned/CRC save blobs;
- helper for two-slot A/B transactional saves if useful;
- recovery policy for interrupted/corrupt writes;
- import/export tooling only after the core Saturn path is stable.

## Recommended first slice

Implement Phases 1-3 first.

The first milestone is successful host tests plus an SH-2 build that can:

```text
BUP_Init
 -> BUP_Stat(device 0)
 -> BUP_Write
 -> BUP_Verify
 -> BUP_Read
 -> BUP_Dir
 -> BUP_Delete
```

without any direct raw Backup RAM manipulation and with the reset button guarded
during every destructive/write operation.
