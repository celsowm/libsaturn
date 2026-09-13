// png_writer.hpp — GPL-3.0 (see harness/LICENSE / harness/README.md).
//
// Minimal, dependency-free PNG writer for harness screenshots.
//
// The harness links Ymir and nothing else; pulling in libpng or zlib just to
// save a screenshot would add a build dependency to a tool whose whole point
// is to be easy to run. PNG allows a zlib stream made entirely of *stored*
// (uncompressed) deflate blocks, so a valid file needs only CRC-32 and
// Adler-32 — about sixty lines, and the result opens in anything.
//
// Files are therefore roughly as large as the raw pixels. At 320x224 that is
// ~215 KB per screenshot, which is irrelevant for a debugging artefact.

#pragma once

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace harness::png {

inline uint32_t crc32_of(const uint8_t* data, size_t len, uint32_t crc = 0xFFFFFFFFu) {
    static uint32_t table[256];
    static bool built = false;
    if (!built) {
        for (uint32_t n = 0; n < 256; ++n) {
            uint32_t c = n;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            }
            table[n] = c;
        }
        built = true;
    }
    for (size_t i = 0; i < len; ++i) {
        crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
    }
    return crc;
}

inline void put_be32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>(v >> 24));
    out.push_back(static_cast<uint8_t>(v >> 16));
    out.push_back(static_cast<uint8_t>(v >> 8));
    out.push_back(static_cast<uint8_t>(v));
}

inline void write_chunk(std::ofstream& f, const char type[4], const std::vector<uint8_t>& data) {
    std::vector<uint8_t> header;
    put_be32(header, static_cast<uint32_t>(data.size()));
    f.write(reinterpret_cast<const char*>(header.data()), static_cast<std::streamsize>(header.size()));
    f.write(type, 4);
    if (!data.empty()) {
        f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    }
    uint32_t crc = crc32_of(reinterpret_cast<const uint8_t*>(type), 4);
    crc = crc32_of(data.data(), data.size(), crc) ^ 0xFFFFFFFFu;
    std::vector<uint8_t> crcbuf;
    put_be32(crcbuf, crc);
    f.write(reinterpret_cast<const char*>(crcbuf.data()), 4);
}

// `pixels` is width*height words as produced by Ymir's software renderer.
//
// Note the channel order: Ymir's Color888 declares r, g, b as bitfields in
// that order, so on a little-endian host RED sits in the LOW byte and the word
// reads 0x00BBGGRR -- not the 0xAARRGGBB the name "RGB" suggests. Reading it
// the intuitive way swaps red and blue, which produces a perfectly plausible
// picture in the wrong colours rather than anything that looks like an error.
inline bool write_rgb(const std::string& path, const uint32_t* pixels, uint32_t width, uint32_t height) {
    if (pixels == nullptr || width == 0 || height == 0) {
        return false;
    }
    std::ofstream f(path, std::ios::binary);
    if (!f) {
        return false;
    }

    static const uint8_t kSig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    f.write(reinterpret_cast<const char*>(kSig), 8);

    std::vector<uint8_t> ihdr;
    put_be32(ihdr, width);
    put_be32(ihdr, height);
    ihdr.push_back(8);  // bit depth
    ihdr.push_back(2);  // colour type 2 = truecolour RGB
    ihdr.push_back(0);  // deflate
    ihdr.push_back(0);  // adaptive filtering
    ihdr.push_back(0);  // no interlace
    write_chunk(f, "IHDR", ihdr);

    // Raw scanlines, each prefixed with filter type 0 (None).
    std::vector<uint8_t> raw;
    raw.reserve(static_cast<size_t>(height) * (static_cast<size_t>(width) * 3u + 1u));
    for (uint32_t y = 0; y < height; ++y) {
        raw.push_back(0);
        const uint32_t* row = pixels + static_cast<size_t>(y) * width;
        for (uint32_t x = 0; x < width; ++x) {
            const uint32_t c = row[x];
            raw.push_back(static_cast<uint8_t>(c & 0xFFu));          // red
            raw.push_back(static_cast<uint8_t>((c >> 8) & 0xFFu));   // green
            raw.push_back(static_cast<uint8_t>((c >> 16) & 0xFFu));  // blue
        }
    }

    // zlib stream: 0x78 0x01 header, stored deflate blocks, Adler-32 trailer.
    std::vector<uint8_t> z;
    z.push_back(0x78);
    z.push_back(0x01);
    const size_t kMaxBlock = 65535u;
    for (size_t off = 0; off < raw.size(); off += kMaxBlock) {
        const size_t n = (raw.size() - off < kMaxBlock) ? (raw.size() - off) : kMaxBlock;
        const bool last = (off + n >= raw.size());
        z.push_back(last ? 1u : 0u);
        z.push_back(static_cast<uint8_t>(n & 0xFFu));
        z.push_back(static_cast<uint8_t>((n >> 8) & 0xFFu));
        z.push_back(static_cast<uint8_t>(~n & 0xFFu));
        z.push_back(static_cast<uint8_t>((~n >> 8) & 0xFFu));
        z.insert(z.end(), raw.begin() + static_cast<long>(off), raw.begin() + static_cast<long>(off + n));
    }
    uint32_t a = 1, b = 0;
    for (uint8_t byte : raw) {
        a = (a + byte) % 65521u;
        b = (b + a) % 65521u;
    }
    put_be32(z, (b << 16) | a);

    write_chunk(f, "IDAT", z);
    write_chunk(f, "IEND", {});
    return static_cast<bool>(f);
}

}  // namespace harness::png
