# Volatile RAM expansion cartridge (1 MiB / 4 MiB)

Implementation: `include/saturn/ram_cart.h`, `src/core/ram_cart_api.cpp`,
`src/hal/ram_cart.cpp`, `examples/ram_cart_demo`.

This is **not** the Backup Memory Cartridge; it never calls BIOS BUP,
saves games, formats storage, or writes to an unrecognized cartridge.

## Hardware contract

Primary reference: Sega STN-47, Expansion RAM Cartridge Manual v1.02
(1996-10-01), https://www.infochunk.com/saturn/segahtml_en/info/hon/stn47.htm .
See also `docs/sega_saturn_hardware/hard/scu_/hon/p01_03.md`.
Some reproductions misprint A-Bus addresses as 0x25FE0080 (a DSP port).

- ID byte `0x24FFFFFF`: `0x5A` = 1 MiB, `0x5C` = 4 MiB.
- Only after ID recognized: **word** write 1 to `0x257EFFFE`, then
  0x23301FF0 to `0x25FE00B0` (CS0/CS1 only), 0x13 to `0x25FE00B8`
  (refresh). Never alter CS2/reserved.
- P2 uncached bank bases: `0x22400000` and `0x22600000`. Each is
  512 KiB (1 MiB cart) or 2 MiB (4 MiB cart); no contiguous 4 MiB pointer.
  1 MiB cart banks are mirrored in the otherwise wider address windows.
- Only data, never code. SCU DMA can **read** cart memory but cannot
  **write** to it. SH-2 DMA may write; this module uses CPU copies.
- Cart data is volatile. After system clock changes, reinitialize/reload
  cart contents. Do not hot-plug a cartridge.

## Native API

```c
#include "saturn/ram_cart.h"
sat_ram_cart_info_t info;
if (sat_ram_cart_init() == SAT_OK &&
    sat_ram_cart_info(&info) == SAT_OK) {
    void* one_bank = sat_ram_cart_alloc(64 * 1024, 32);
    sat_ram_cart_buffer_t logical;
    if (sat_ram_cart_buffer_alloc(3 * 1024 * 1024, 32, &logical) == SAT_OK) {
        /* May span two banks: use read_at/write_at, not bank[0][offset]. */
    }
}
```

`sat_ram_cart_alloc` returns a contiguous region inside **one** physical
bank only (max 2 MiB on 4 MiB cart). `sat_ram_cart_buffer_alloc` is
transactional and produces at most two slices for logical offset-based
reads/writes. There is no heap or individual free. `sat_ram_cart_init` is
idempotent and does not zero physical RAM. `sat_ram_cart_reset()`
invalidates allocations; release every cart-backed consumer before calling.

## Optional asset cache and VFS

The default cache is 4 × 64 KiB internal blocks. To place 2 MiB of asset
cache in **one bank** of a 4 MiB cart (on 1 MiB carts, at most 8 blocks):

```c
#include "saturn/asset.h"
#include "saturn/ram_cart.h"
/* Call after sat_init and sat_ram_cart_init. */
void* cache = sat_ram_cart_alloc(32u * SAT_ASSET_CACHE_BLOCK_BYTES, 32u);
if (cache != NULL) {
    (void)sat_asset_cache_configure(cache, 32u);
}
```

The cache supports up to 32 blocks and preserves the existing no-heap
design. No cart means the default internal cache remains available.
Reconfiguring clears cached data and refuses to run while any prefetch is
pending; restore with `sat_asset_cache_configure(NULL, 0u)` before
`sat_ram_cart_reset()`. The backing memory must outlive cache use.

To expose a previously filled **logical cart buffer** to read-only VFS:

```c
sat_file_register_backend("CART/LEVEL.BIN", buffer.size,
                          sat_ram_cart_file_read_at, &buffer);
```

The buffer object/storage must outlive the mount. Registered logical assets
may use "CART/LEVEL.BIN" as a physical source path through normal APIs.

## Tests and limitations

`make test` includes host tests covering no cart, 1 MiB / 4 MiB geometry,
allocation boundaries/rollback, bank-crossing copies, stale handles and
cache relocation. The executable `examples/ram_cart_demo` probes both banks.
Run on Ymir or another supported emulator with no cart, 1 MiB and 4 MiB,
and validate on real hardware before claiming hardware acceptance.
