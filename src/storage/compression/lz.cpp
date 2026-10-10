#include "saturn/lz.h"

#include "src/storage/compression/lz_stream.hpp"

/* SLZ1 decoder (layout: tools/satlz.py). Byte loops on purpose: in the
 * in-place layout the output runs up behind the input, and a match may
 * overlap its own output, so neither copy may become a block move. */

namespace {

constexpr uint8_t kMagic[4] = {'S', 'L', 'Z', '1'};
constexpr uint32_t kMinMatch = 4u;

uint32_t be32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) | p[3];
}

/* Adds the extension bytes of a 15 nibble; false when they run past `end`. */
bool extend(const uint8_t*& in, const uint8_t* end, uint32_t& n) {
    uint8_t b;
    do {
        if (in >= end) return false;
        b = *in++;
        n += b;
    } while (b == 255u);
    return true;
}

}  // namespace

extern "C" sat_result_t sat_lz_read_header(const void* file, sat_lz_header_t* out_header) {
    if (file == nullptr || out_header == nullptr) return SAT_ERR_INVALID_ARG;
    const uint8_t* p = static_cast<const uint8_t*>(file);
    for (uint32_t i = 0u; i < 4u; ++i) {
        if (p[i] != kMagic[i]) return SAT_ERR_VERSION;
    }
    out_header->decoded = be32(p + 4u);
    out_header->margin = be32(p + 8u);
    out_header->payload = be32(p + 12u);
    return SAT_OK;
}

namespace saturn::storage::lz {

sat_result_t begin(Stream& stream, const void* file, uint32_t file_bytes,
                   void* destination, uint32_t capacity) {
    if (file == nullptr || destination == nullptr || file_bytes < SAT_LZ_HEADER_BYTES) {
        return SAT_ERR_INVALID_ARG;
    }
    sat_lz_header_t h;
    SAT_TRY(sat_lz_read_header(file, &h));
    if (h.payload > file_bytes - SAT_LZ_HEADER_BYTES || h.payload == 0u) return SAT_ERR_IO;
    if (h.decoded > capacity) return SAT_ERR_CAPACITY;
    stream.in = static_cast<const uint8_t*>(file) + SAT_LZ_HEADER_BYTES;
    stream.in_end = stream.in + h.payload;
    stream.out_start = static_cast<uint8_t*>(destination);
    stream.out = stream.out_start;
    stream.out_end = stream.out_start + h.decoded;
    return SAT_OK;
}

#if defined(__GNUC__) && !defined(__clang__)
__attribute__((optimize("no-tree-loop-distribute-patterns")))
#endif
sat_result_t step(Stream& stream, const void* available) {
    const uint8_t* const in_end = stream.in_end;
    const uint8_t* limit = static_cast<const uint8_t*>(available);
    if (limit > in_end) limit = in_end;
    if (limit <= stream.in && limit != in_end) return SAT_OK;
    /* A sequence cut by `limit` waits for more input; with the whole file
     * present the same cut means the payload is corrupt. */
    const sat_result_t cut = limit == in_end ? SAT_ERR_IO : SAT_OK;
    const uint8_t* in = stream.in;
    uint8_t* out = stream.out;
    uint8_t* const out_start = stream.out_start;
    uint8_t* const out_end = stream.out_end;
    sat_result_t status = SAT_OK;
    while (in < limit) {
        /* Parse the whole sequence before writing anything of it. */
        const uint8_t* p = in;
        const uint8_t token = *p++;
        uint32_t lits = token >> 4;
        if (lits == 15u && !extend(p, limit, lits)) { status = cut; break; }
        if (lits > static_cast<uint32_t>(limit - p)) { status = cut; break; }
        if (lits > static_cast<uint32_t>(out_end - out)) { status = SAT_ERR_IO; break; }
        const uint8_t* const literals = p;
        p += lits;
        uint32_t offset = 0u;
        uint32_t length = 0u;
        if (p != in_end) {
            if (limit - p < 2) { status = cut; break; }
            offset = (static_cast<uint32_t>(p[0]) << 8) | p[1];
            p += 2;
            length = token & 15u;
            if (length == 15u && !extend(p, limit, length)) { status = cut; break; }
            length += kMinMatch;
            if (offset == 0u || offset > static_cast<uint32_t>(out + lits - out_start) ||
                length > static_cast<uint32_t>(out_end - out - lits) || p == in_end) {
                status = SAT_ERR_IO;
                break;
            }
        }
        for (uint32_t i = 0u; i < lits; ++i) out[i] = literals[i];
        out += lits;
        const uint8_t* from = out - offset;
        for (uint32_t i = 0u; i < length; ++i) out[i] = from[i];
        out += length;
        in = p;
    }
    stream.in = in;
    stream.out = out;
    if (status == SAT_OK && in == in_end && out != out_end) status = SAT_ERR_IO;
    return status;
}

}  // namespace saturn::storage::lz

extern "C" sat_result_t sat_lz_decode(const void* file, uint32_t file_bytes,
                                      void* destination, uint32_t capacity, uint32_t* out_bytes) {
    namespace lz = saturn::storage::lz;
    if (out_bytes != nullptr) *out_bytes = 0u;
    lz::Stream stream;
    SAT_TRY(lz::begin(stream, file, file_bytes, destination, capacity));
    SAT_TRY(lz::step(stream, stream.in_end));
    if (out_bytes != nullptr) *out_bytes = static_cast<uint32_t>(stream.out_end - stream.out_start);
    return SAT_OK;
}
