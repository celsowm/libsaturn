#include "saturn/movie.h"

/* The picture decoder behind saturn/movie.h. Chunk, flag and colour rules follow Cinepak as ffmpeg's
 * cinepak decoder reads it; a codebook entry is converted to a 2x2 patch of VDP2 dots when it is loaded. */

namespace {

constexpr uint32_t kFrameHeader = 10u;
constexpr uint32_t kStripHeader = 12u;
constexpr uint32_t kChunkHeader = 4u;
constexpr uint16_t kChunkV4Full = 0x20u;
constexpr uint16_t kChunkV4Update = 0x21u;
constexpr uint16_t kChunkV1Full = 0x22u;
constexpr uint16_t kChunkV1Update = 0x23u;
constexpr uint16_t kChunkIntra = 0x30u;
constexpr uint16_t kChunkInter = 0x31u;
constexpr uint16_t kChunkV1Only = 0x32u;

inline uint32_t be16(const uint8_t* p) { return (static_cast<uint32_t>(p[0]) << 8) | p[1]; }
inline uint32_t be24(const uint8_t* p) { return (be16(p) << 8) | p[2]; }
inline uint32_t be32(const uint8_t* p) { return (be16(p) << 16) | be16(p + 2); }

constexpr uint32_t kEntryBytes = 6u;   /* four luma, then u and v as signed bytes */
constexpr int32_t kClipBias = 384;     /* y + 2v, y - u/2 - v and y + 2u reach -384 .. 640 */

/* A colour 0..255 reduced to five bits, for sums that may lie outside it. */
inline uint8_t clip5(const sat_movie_decoder_t* d, int32_t v) { return d->clip5[v + kClipBias]; }

/* ffmpeg's cinepak rule: r = y + 2v, g = y - u/2 - v (u/2 truncates toward zero), b = y + 2u; then the
 * VDP2 dot 1BBBBBGGGGGRRRRR. */
void convert_entry(const sat_movie_decoder_t* d, uint16_t* out, const uint8_t* p) {
    const int32_t u = static_cast<int8_t>(p[4]);
    const int32_t v = static_cast<int8_t>(p[5]);
    const int32_t radd = 2 * v;
    const int32_t gadd = -(u / 2) - v;
    const int32_t badd = 2 * u;
    for (uint32_t i = 0u; i < 4u; ++i) {
        const int32_t y = p[i];
        out[i] = static_cast<uint16_t>(0x8000u | (static_cast<uint32_t>(clip5(d, y + badd)) << 10) |
                                       (static_cast<uint32_t>(clip5(d, y + gadd)) << 5) | clip5(d, y + radd));
    }
}

sat_result_t codebook(const sat_movie_decoder_t* d, uint16_t (*book)[4], bool update, const uint8_t* p,
                      const uint8_t* end) {
    uint32_t flags = 0u;
    uint32_t mask = 0u;
    for (uint32_t i = 0u; i < SAT_MOVIE_CODEBOOK_ENTRIES; ++i) {
        if (update) {
            mask >>= 1;
            if (mask == 0u) {
                if (end - p < 4) break;
                flags = be32(p);
                p += 4;
                mask = 0x80000000u;
            }
            if ((flags & mask) == 0u) continue;
        }
        if (end - p < static_cast<int32_t>(kEntryBytes)) break;
        convert_entry(d, book[i], p);
        p += kEntryBytes;
    }
    return SAT_OK;
}

struct Picture {
    uint16_t* base;
    uint32_t stride;   /* pixels */
};

/* Four patches to one 4x4 block, a pixel at a time. */
inline void put_v4(uint16_t* block, uint32_t stride, const uint16_t* a, const uint16_t* b,
                   const uint16_t* c, const uint16_t* d) {
    uint16_t* r0 = block;
    uint16_t* r1 = block + stride;
    uint16_t* r2 = block + 2u * stride;
    uint16_t* r3 = block + 3u * stride;
    r0[0] = a[0]; r0[1] = a[1]; r0[2] = b[0]; r0[3] = b[1];
    r1[0] = a[2]; r1[1] = a[3]; r1[2] = b[2]; r1[3] = b[3];
    r2[0] = c[0]; r2[1] = c[1]; r2[2] = d[0]; r2[3] = d[1];
    r3[0] = c[2]; r3[1] = c[3]; r3[2] = d[2]; r3[3] = d[3];
}

inline void put_v1(uint16_t* block, uint32_t stride, const uint16_t* p) {
    uint16_t* r0 = block;
    uint16_t* r1 = block + stride;
    uint16_t* r2 = block + 2u * stride;
    uint16_t* r3 = block + 3u * stride;
    r0[0] = r0[1] = r1[0] = r1[1] = p[0];
    r0[2] = r0[3] = r1[2] = r1[3] = p[1];
    r2[0] = r2[1] = r3[0] = r3[1] = p[2];
    r2[2] = r2[3] = r3[2] = r3[3] = p[3];
}

sat_result_t vectors(const sat_movie_decoder_t* d, uint16_t id, const uint8_t* p, const uint8_t* end,
                     Picture pic) {
    const bool has_skip = (id & 0x01u) != 0u;
    const bool v1_only = (id & 0x02u) != 0u;
    uint32_t flags = 0u;
    uint32_t mask = 0u;
    for (uint32_t y = 0u; y < d->height; y += 4u) {
        uint16_t* row = pic.base + y * pic.stride;
        for (uint32_t x = 0u; x < d->width; x += 4u, row += 4) {
            if (has_skip) {
                mask >>= 1;
                if (mask == 0u) {
                    if (end - p < 4) return SAT_ERR_IO;
                    flags = be32(p);
                    p += 4;
                    mask = 0x80000000u;
                }
                if ((flags & mask) == 0u) continue;
            }
            bool v1 = v1_only;
            if (!v1_only) {
                mask >>= 1;
                if (mask == 0u) {
                    if (end - p < 4) return SAT_ERR_IO;
                    flags = be32(p);
                    p += 4;
                    mask = 0x80000000u;
                }
                v1 = (flags & mask) == 0u;
            }
            if (v1) {
                if (end - p < 1) return SAT_ERR_IO;
                put_v1(row, pic.stride, d->v1[*p++]);
            } else {
                if (end - p < 4) return SAT_ERR_IO;
                put_v4(row, pic.stride, d->v4[p[0]], d->v4[p[1]], d->v4[p[2]], d->v4[p[3]]);
                p += 4;
            }
        }
    }
    return SAT_OK;
}

}  // namespace

extern "C" sat_result_t sat_movie_decoder_init(sat_movie_decoder_t* decoder, uint16_t width, uint16_t height) {
    if (decoder == nullptr || width == 0u || height == 0u || (width & 3u) != 0u || (height & 3u) != 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    decoder->width = width;
    decoder->height = height;
    for (uint32_t i = 0u; i < SAT_MOVIE_CODEBOOK_ENTRIES; ++i) {
        for (uint32_t k = 0u; k < 4u; ++k) decoder->v1[i][k] = decoder->v4[i][k] = 0u;
    }
    for (int32_t i = 0; i < static_cast<int32_t>(sizeof decoder->clip5); ++i) {
        const int32_t value = i - kClipBias;
        decoder->clip5[i] = static_cast<uint8_t>((value < 0 ? 0 : value > 255 ? 255 : value) >> 3);
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_movie_decode_frame(sat_movie_decoder_t* decoder, const void* frame, uint32_t bytes,
                                               uint16_t* destination, uint32_t stride_pixels) {
    if (decoder == nullptr || frame == nullptr || destination == nullptr || decoder->width == 0u ||
        stride_pixels < decoder->width || (stride_pixels & 1u) != 0u ||
        (reinterpret_cast<uintptr_t>(destination) & 3u) != 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    const uint8_t* p = static_cast<const uint8_t*>(frame);
    if (bytes < kFrameHeader + kStripHeader) return SAT_ERR_IO;
    const uint32_t declared = be24(p + 1u);
    if (declared < bytes) bytes = declared;
    if (be16(p + 4u) != decoder->width || be16(p + 6u) != decoder->height) return SAT_ERR_INVALID_ARG;
    if (be16(p + 8u) != 1u) return SAT_ERR_UNSUPPORTED;
    const uint8_t* const frame_end = p + bytes;
    p += kFrameHeader;

    const uint32_t strip_bytes = be16(p + 2u);
    if (strip_bytes < kStripHeader || strip_bytes > static_cast<uint32_t>(frame_end - p)) return SAT_ERR_IO;
    const uint8_t* const strip_end = p + strip_bytes;
    p += kStripHeader;

    const Picture picture{destination, stride_pixels};
    while (strip_end - p >= static_cast<int32_t>(kChunkHeader)) {
        const uint16_t id = static_cast<uint16_t>(p[0]);   /* the type is the high byte of a 16-bit id; the low byte is 0 */
        const uint32_t size = be16(p + 2u);
        if (size < kChunkHeader || size > static_cast<uint32_t>(strip_end - p)) return SAT_ERR_IO;
        const uint8_t* const body = p + kChunkHeader;
        const uint8_t* const end = p + size;
        sat_result_t rc = SAT_OK;
        switch (id) {
            case kChunkV4Full:
            case kChunkV4Update:
                rc = codebook(decoder, decoder->v4, id == kChunkV4Update, body, end);
                break;
            case kChunkV1Full:
            case kChunkV1Update:
                rc = codebook(decoder, decoder->v1, id == kChunkV1Update, body, end);
                break;
            case kChunkIntra:
            case kChunkInter:
            case kChunkV1Only:
                rc = vectors(decoder, id, body, end, picture);
                break;
            default:
                if (id >= 0x24u && id <= 0x27u) return SAT_ERR_UNSUPPORTED;
                break;   /* unknown chunks are skipped, as Cinepak decoders do */
        }
        if (rc != SAT_OK) return rc;
        p = end;
    }
    return SAT_OK;
}
