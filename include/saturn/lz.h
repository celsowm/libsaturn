#ifndef SATURN_LZ_H
#define SATURN_LZ_H

#include <stdint.h>

#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* SLZ1, a byte-oriented LZ format (LZ4-style sequences) for disc data. The
 * CD drive reads ~300 KB/s; tile maps and sprite tiles shrink to 20-35% and
 * the SH-2 decodes them several times faster than the drive reads, so a
 * compressed file loads in a fraction of the time. Files are made by the
 * host tool tools/satlz.py (installed in share/libsaturn/tools), which also
 * documents the layout.
 *
 * Decoding works in place: with the whole file placed at the END of a
 * destination of at least decoded + margin bytes (both in the header), the
 * output written from the start never overtakes the input still to be read.
 * sat_load_lz (saturn/loader.h) and sat_cart_overlay_load_lz
 * (saturn/cart_overlay.h) load that way, so they need no extra buffer. */

#define SAT_LZ_HEADER_BYTES 16u

typedef struct sat_lz_header {
    uint32_t decoded;  /* bytes after decoding */
    uint32_t margin;   /* in-place decoding needs decoded + margin bytes */
    uint32_t payload;  /* bytes after the header */
} sat_lz_header_t;

/* Parses the first SAT_LZ_HEADER_BYTES of a file. SAT_ERR_VERSION when it is
 * not SLZ1. */
sat_result_t sat_lz_read_header(const void* file, sat_lz_header_t* out_header);

/* Decodes the SLZ1 file at `file` (`file_bytes` long: header and payload)
 * into `destination`. SAT_ERR_CAPACITY when the decoded size exceeds
 * `capacity`; SAT_ERR_IO when the payload is corrupt (a match before the
 * start of the output, a length past either end), with the output partly
 * written. Source and destination may overlap only in the in-place layout
 * above. */
sat_result_t sat_lz_decode(const void* file, uint32_t file_bytes,
                           void* destination, uint32_t capacity, uint32_t* out_bytes);

#ifdef __cplusplus
}
#endif
#endif /* SATURN_LZ_H */
