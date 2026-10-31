#ifndef SATURN_HAL_VIDEO_MODE_LOGIC_HPP
#define SATURN_HAL_VIDEO_MODE_LOGIC_HPP

#include <stdint.h>

#include "saturn/core.h"

/* Pure TV-mode rules shared by sat_init(), sat_video_mode_supported() and the
 * host tests. Nothing here reads the console or writes a register.
 *
 * The VDP2 manual's TV screen mode register (2.4) defines HRESO bits 2-0 as
 * 320/352/640/704, VRESO bits 5-4 as 224/240/256 (11 is setting-prohibited,
 * and 256 needs a PAL TV), and LSMD bits 7-6 as non-interlaced / prohibited /
 * single-density / double-density. This library implements non-interlaced,
 * single-density interlace, and double-density interlace in one shape: height
 * 448 at width 640 or 704. There VDP1 draws full-resolution coordinates split
 * into alternating fields (src/hal/vdp1/vdp1.cpp, FBCR DIE=1). Double density
 * at any other size has no drawing path, so it is reported as unsupported
 * rather than written and left to tear. */

namespace saturn::hal::video_mode {

inline bool supported_width(uint16_t width) {
    return width == 320u || width == 352u || width == 640u || width == 704u;
}

inline bool supported_height(uint16_t height) {
    return height == 224u || height == 240u || height == 256u;
}

/* HRESO as the manual numbers it. Caller has validated the width. */
inline uint16_t hreso_bits(uint16_t width) {
    switch (width) {
    case 352u: return 1u;
    case 640u: return 2u;
    case 704u: return 3u;
    default: return 0u;
    }
}

/* VRESO: 0 = 224, 1 = 240, 2 = 256. Caller has validated the height. */
inline uint16_t vreso_bits(uint16_t height) {
    return height == 240u ? 1u : (height == 256u ? 2u : 0u);
}

/* LSMD: 0 non-interlaced, 2 single-density, 3 double-density. */
inline uint16_t lsmd_bits(uint8_t interlace) {
    return interlace == SAT_VIDEO_INTERLACE_DOUBLE ? 3u
         : (interlace == SAT_VIDEO_INTERLACE_SINGLE ? 2u : 0u);
}

/* Height 448 is the whole-picture spelling of double density: VRESO 224 and
 * two different fields. A config with interlace 0 selects it too. */
inline bool double_density_height(uint16_t height) {
    return height == 448u;
}

inline sat_result_t validate(uint16_t width, uint16_t height, uint8_t interlace,
                             bool console_pal) {
    if (double_density_height(height)) {
        if (!supported_width(width) || interlace == SAT_VIDEO_INTERLACE_SINGLE ||
            interlace > SAT_VIDEO_INTERLACE_DOUBLE) {
            return SAT_ERR_INVALID_ARG;
        }
        return hreso_bits(width) >= 2u ? SAT_OK : SAT_ERR_UNSUPPORTED;
    }
    if (!supported_width(width) || !supported_height(height) ||
        interlace > SAT_VIDEO_INTERLACE_DOUBLE) {
        return SAT_ERR_INVALID_ARG;
    }
    if (height == 256u && !console_pal) {
        /* VDP2 manual table in 2.4: 256 lines is "PAL TV" only. */
        return SAT_ERR_UNSUPPORTED;
    }
    if (interlace == SAT_VIDEO_INTERLACE_DOUBLE) {
        return SAT_ERR_UNSUPPORTED;
    }
    return SAT_OK;
}

inline sat_result_t validate(const sat_video_config_t* config, bool console_pal) {
    if (config == nullptr) return SAT_ERR_INVALID_ARG;
    return validate(config->width, config->height, config->interlace, console_pal);
}

/* Bits 7-0 of TVMD for a validated mode. DISP and BDCLMD (bits 15 and 8) are
 * preserved by the caller. */
inline uint16_t mode_bits(uint16_t width, uint16_t height, uint8_t interlace) {
    const uint8_t scan = double_density_height(height) ? SAT_VIDEO_INTERLACE_DOUBLE : interlace;
    return static_cast<uint16_t>((lsmd_bits(scan) << 6u) |
                                 (vreso_bits(height) << 4u) |
                                 hreso_bits(width));
}

}  // namespace saturn::hal::video_mode

#endif /* SATURN_HAL_VIDEO_MODE_LOGIC_HPP */
