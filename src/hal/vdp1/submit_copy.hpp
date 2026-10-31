#ifndef SATURN_HAL_VDP1_SUBMIT_COPY_HPP
#define SATURN_HAL_VDP1_SUBMIT_COPY_HPP

#include <stdint.h>

/* The copy of a finished command list into VDP1 VRAM. No hardware access: the destination is a
 * pointer, so a host test can check the image against a reference.
 *
 * A normal sprite (command 0) reads only the first 16 bytes of its table: the vertices B, C and D
 * are "don't care" (VDP1 manual 7.4), and CMDGRDA at +1C matters only with Gouraud shading
 * (CMDPMOD bit 2). Every store to VRAM is a slow bus cycle, so those commands are copied as four
 * longwords and the rest of the slot keeps whatever it held; every other command is copied whole.
 * The words are as the SH-2 sees them (big-endian pairs of command words). */
namespace saturn::hal::vdp1::submit_copy {

inline bool reads_only_head(uint32_t ctrl_link, uint32_t pmod_colr) {
    return (ctrl_link & 0x000F0000u) == 0u && (pmod_colr & 0x00040000u) == 0u;
}

/* Copies `count` 32-byte command tables. Returns the number of longwords stored to `dst`. */
inline uint32_t copy_commands(const uint32_t* src, volatile uint32_t* dst, uint32_t count) {
    uint32_t stores = 0u;
    for (uint32_t i = 0u; i < count; ++i, src += 8u, dst += 8u) {
        const uint32_t w0 = src[0], w1 = src[1];
        dst[0] = w0;
        dst[1] = w1;
        dst[2] = src[2];
        dst[3] = src[3];
        stores += 4u;
        if (reads_only_head(w0, w1)) continue;
        dst[4] = src[4];
        dst[5] = src[5];
        dst[6] = src[6];
        dst[7] = src[7];
        stores += 4u;
    }
    return stores;
}

/* Immutable command banks use absolute VRAM links. Relocate ASSIGN/CALL
 * (including their SKIP variants), preserving NEXT/RETURN and all draw words. */
inline void copy_commands_relocated(const uint32_t* src, volatile uint32_t* dst,
                                    uint32_t count, uint32_t byte_base) {
    (void)copy_commands(src, dst, count);
    for (uint32_t i = 0u; i < count; ++i) {
        const uint32_t first = src[i * 8u];
        const uint32_t jp = (first >> 28u) & 3u;
        if (jp == 1u || jp == 2u)
            dst[i * 8u] = (first & 0xFFFF0000u) |
                static_cast<uint16_t>((first & 0xFFFFu) + byte_base / 8u);
    }
}

}  // namespace saturn::hal::vdp1::submit_copy

#endif
