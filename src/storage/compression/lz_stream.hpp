#ifndef SATURN_STORAGE_COMPRESSION_LZ_STREAM_HPP
#define SATURN_STORAGE_COMPRESSION_LZ_STREAM_HPP

#include "saturn/lz.h"

namespace saturn::storage::lz {

/* An SLZ1 decode that can run while its file is still arriving: each step
 * decodes the sequences lying wholly below the end of what has arrived, so
 * sat_load_lz decodes behind the CD transfer instead of after it. */
struct Stream {
    const uint8_t* in;
    const uint8_t* in_end;
    uint8_t* out;
    uint8_t* out_start;
    uint8_t* out_end;
};

/* Reads the header at `file` (its first SAT_LZ_HEADER_BYTES must be there)
 * and checks the payload fits `file_bytes` and the output `capacity`. */
sat_result_t begin(Stream& stream, const void* file, uint32_t file_bytes,
                   void* destination, uint32_t capacity);

/* Decodes up to `available` (one past the last byte of the file present).
 * SAT_ERR_IO when the payload is corrupt; a sequence cut by `available`
 * waits for the next step unless the whole file is present. */
sat_result_t step(Stream& stream, const void* available);

inline bool done(const Stream& stream) { return stream.in == stream.in_end; }

}  // namespace saturn::storage::lz

#endif
