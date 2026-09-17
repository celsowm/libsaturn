#ifndef SATURN_CORE_SURFACE_LOGIC_HPP
#define SATURN_CORE_SURFACE_LOGIC_HPP

#include <stddef.h>
#include <stdint.h>
#include "saturn/surface.h"

namespace saturn::core::surface_logic {

inline uint8_t bytes_per_pixel(sat_pixel_format_t format) {
    switch (format) {
        case SAT_PIXEL_INDEX8: return 1u;
        case SAT_PIXEL_RGB555:
        case SAT_PIXEL_ARGB1555:
        case SAT_PIXEL_RGB565: return 2u;
        case SAT_PIXEL_RGBA8888: return 4u;
        default: return 0u;
    }
}

inline uint8_t expand5(uint16_t value) {
    const uint8_t v = static_cast<uint8_t>(value & 0x1Fu);
    return static_cast<uint8_t>((v << 3u) | (v >> 2u));
}

inline uint8_t expand6(uint16_t value) {
    const uint8_t v = static_cast<uint8_t>(value & 0x3Fu);
    return static_cast<uint8_t>((v << 2u) | (v >> 4u));
}

inline uint16_t load_u16(const uint8_t* p) {
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8u) | p[1]);
#else
    return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8u));
#endif
}

inline void store_u16(uint8_t* p, uint16_t value) {
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    p[0] = static_cast<uint8_t>(value >> 8u);
    p[1] = static_cast<uint8_t>(value & 0xFFu);
#else
    p[0] = static_cast<uint8_t>(value & 0xFFu);
    p[1] = static_cast<uint8_t>(value >> 8u);
#endif
}

inline sat_color_t decode_rgb555(uint16_t word) {
    return sat_color_rgba(
        expand5(word),
        expand5(static_cast<uint16_t>(word >> 5u)),
        expand5(static_cast<uint16_t>(word >> 10u)),
        255u);
}

inline uint16_t encode_rgb555(sat_color_t color, bool alpha_bit) {
    uint16_t word = static_cast<uint16_t>(
        ((static_cast<uint16_t>(color.b) >> 3u) << 10u) |
        ((static_cast<uint16_t>(color.g) >> 3u) << 5u) |
        (static_cast<uint16_t>(color.r) >> 3u));
    if (!alpha_bit || color.a >= 128u) {
        word = static_cast<uint16_t>(word | 0x8000u);
    }
    return word;
}

inline sat_color_t decode_rgb565(uint16_t word) {
    return sat_color_rgba(
        expand5(word),
        expand6(static_cast<uint16_t>(word >> 5u)),
        expand5(static_cast<uint16_t>(word >> 11u)),
        255u);
}

inline uint16_t encode_rgb565(sat_color_t color) {
    return static_cast<uint16_t>(
        ((static_cast<uint16_t>(color.b) >> 3u) << 11u) |
        ((static_cast<uint16_t>(color.g) >> 2u) << 5u) |
        (static_cast<uint16_t>(color.r) >> 3u));
}

inline bool valid_palette(const sat_surface_t* surface) {
    return surface->palette_rgb555 != nullptr && surface->palette_count > 0u && surface->palette_count <= 256u;
}

inline sat_result_t validate(const sat_surface_t* surface) {
    if (surface == nullptr || surface->pixels == nullptr || surface->width == 0u || surface->height == 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    const uint8_t bpp = bytes_per_pixel(surface->format);
    if (bpp == 0u) {
        return SAT_ERR_UNSUPPORTED;
    }
    const size_t row_bytes = static_cast<size_t>(surface->width) * bpp;
    if (row_bytes > 0xFFFFu || surface->pitch < row_bytes) {
        return SAT_ERR_INVALID_ARG;
    }
    if (surface->format == SAT_PIXEL_INDEX8) {
        if (!valid_palette(surface)) {
            return SAT_ERR_INVALID_ARG;
        }
    } else if (surface->palette_rgb555 != nullptr || surface->palette_count != 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    return SAT_OK;
}

inline sat_result_t init(
    sat_surface_t* surface,
    void* pixels,
    uint16_t width,
    uint16_t height,
    uint16_t pitch,
    sat_pixel_format_t format,
    const uint16_t* palette_rgb555,
    uint16_t palette_count) {
    if (surface == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    sat_surface_t candidate = {pixels, width, height, pitch, format, palette_rgb555, palette_count};
    const sat_result_t result = validate(&candidate);
    if (result != SAT_OK) {
        return result;
    }
    *surface = candidate;
    return SAT_OK;
}

inline bool rect_inside(const sat_surface_t* surface, const sat_rect_t* rect) {
    if (surface == nullptr || rect == nullptr || rect->x < 0 || rect->y < 0 || rect->width == 0u || rect->height == 0u) {
        return false;
    }
    const uint32_t right = static_cast<uint32_t>(static_cast<uint16_t>(rect->x)) + rect->width;
    const uint32_t bottom = static_cast<uint32_t>(static_cast<uint16_t>(rect->y)) + rect->height;
    return right <= surface->width && bottom <= surface->height;
}

inline uint8_t* pixel_ptr(sat_surface_t* surface, uint16_t x, uint16_t y) {
    return static_cast<uint8_t*>(surface->pixels) + static_cast<size_t>(y) * surface->pitch +
        static_cast<size_t>(x) * bytes_per_pixel(surface->format);
}

inline const uint8_t* pixel_ptr(const sat_surface_t* surface, uint16_t x, uint16_t y) {
    return static_cast<const uint8_t*>(surface->pixels) + static_cast<size_t>(y) * surface->pitch +
        static_cast<size_t>(x) * bytes_per_pixel(surface->format);
}

inline sat_result_t subview(const sat_surface_t* source, const sat_rect_t* rect, sat_surface_t* out_surface) {
    const sat_result_t valid = validate(source);
    if (valid != SAT_OK) return valid;
    if (out_surface == nullptr || !rect_inside(source, rect)) return SAT_ERR_INVALID_ARG;
    *out_surface = *source;
    out_surface->pixels = const_cast<uint8_t*>(pixel_ptr(source, static_cast<uint16_t>(rect->x), static_cast<uint16_t>(rect->y)));
    out_surface->width = rect->width;
    out_surface->height = rect->height;
    return SAT_OK;
}

inline sat_result_t get_pixel(const sat_surface_t* surface, uint16_t x, uint16_t y, sat_color_t* out_color) {
    const sat_result_t valid = validate(surface);
    if (valid != SAT_OK) return valid;
    if (out_color == nullptr || x >= surface->width || y >= surface->height) return SAT_ERR_INVALID_ARG;
    const uint8_t* p = pixel_ptr(surface, x, y);
    switch (surface->format) {
        case SAT_PIXEL_INDEX8: {
            const uint8_t index = p[0];
            if (index >= surface->palette_count) {
                return SAT_ERR_INVALID_ARG;
            }
            *out_color = decode_rgb555(surface->palette_rgb555[index]);
            return SAT_OK;
        }
        case SAT_PIXEL_RGB555:
            *out_color = decode_rgb555(load_u16(p));
            return SAT_OK;
        case SAT_PIXEL_ARGB1555: {
            const uint16_t word = load_u16(p);
            *out_color = decode_rgb555(word);
            out_color->a = (word & 0x8000u) != 0u ? 255u : 0u;
            return SAT_OK;
        }
        case SAT_PIXEL_RGB565:
            *out_color = decode_rgb565(load_u16(p));
            return SAT_OK;
        case SAT_PIXEL_RGBA8888:
            *out_color = sat_color_rgba(p[0], p[1], p[2], p[3]);
            return SAT_OK;
        default:
            return SAT_ERR_UNSUPPORTED;
    }
}

inline sat_result_t palette_index_for_color(const sat_surface_t* surface, sat_color_t color, uint8_t* out_index) {
    if (!valid_palette(surface) || out_index == nullptr || color.a != 255u) {
        return SAT_ERR_UNSUPPORTED;
    }
    const uint16_t wanted = static_cast<uint16_t>(encode_rgb555(color, false) & 0x7FFFu);
    for (uint16_t i = 0u; i < surface->palette_count; ++i) {
        if ((surface->palette_rgb555[i] & 0x7FFFu) == wanted) {
            *out_index = static_cast<uint8_t>(i);
            return SAT_OK;
        }
    }
    return SAT_ERR_UNSUPPORTED;
}

inline sat_result_t set_pixel(sat_surface_t* surface, uint16_t x, uint16_t y, sat_color_t color) {
    const sat_result_t valid = validate(surface);
    if (valid != SAT_OK) return valid;
    if (x >= surface->width || y >= surface->height) return SAT_ERR_INVALID_ARG;
    uint8_t* p = pixel_ptr(surface, x, y);
    switch (surface->format) {
        case SAT_PIXEL_INDEX8: {
            uint8_t index = 0u;
            const sat_result_t result = palette_index_for_color(surface, color, &index);
            if (result != SAT_OK) {
                return result;
            }
            p[0] = index;
            return SAT_OK;
        }
        case SAT_PIXEL_RGB555:
            store_u16(p, encode_rgb555(color, false));
            return SAT_OK;
        case SAT_PIXEL_ARGB1555:
            store_u16(p, encode_rgb555(color, true));
            return SAT_OK;
        case SAT_PIXEL_RGB565:
            store_u16(p, encode_rgb565(color));
            return SAT_OK;
        case SAT_PIXEL_RGBA8888:
            p[0] = color.r;
            p[1] = color.g;
            p[2] = color.b;
            p[3] = color.a;
            return SAT_OK;
        default:
            return SAT_ERR_UNSUPPORTED;
    }
}

struct ClippedRect {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
};

inline bool clip_rect_to_surface(const sat_surface_t* surface, const sat_rect_t* rect, ClippedRect& out) {
    int32_t x = rect != nullptr ? rect->x : 0;
    int32_t y = rect != nullptr ? rect->y : 0;
    int32_t width = rect != nullptr ? static_cast<int32_t>(rect->width) : static_cast<int32_t>(surface->width);
    int32_t height = rect != nullptr ? static_cast<int32_t>(rect->height) : static_cast<int32_t>(surface->height);
    if (width <= 0 || height <= 0) {
        return false;
    }
    const int64_t right64 = static_cast<int64_t>(x) + width;
    const int64_t bottom64 = static_cast<int64_t>(y) + height;
    if (right64 <= 0 || bottom64 <= 0 || x >= surface->width || y >= surface->height) {
        return false;
    }
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    const int32_t right = right64 > surface->width ? surface->width : static_cast<int32_t>(right64);
    const int32_t bottom = bottom64 > surface->height ? surface->height : static_cast<int32_t>(bottom64);
    out = {x, y, right - x, bottom - y};
    return out.width > 0 && out.height > 0;
}

inline sat_result_t fill(sat_surface_t* surface, const sat_rect_t* rect, sat_color_t color) {
    const sat_result_t valid = validate(surface);
    if (valid != SAT_OK) return valid;
    ClippedRect clipped{};
    if (!clip_rect_to_surface(surface, rect, clipped)) {
        return SAT_OK;
    }
    for (int32_t y = 0; y < clipped.height; ++y) {
        for (int32_t x = 0; x < clipped.width; ++x) {
            const sat_result_t result = set_pixel(
                surface,
                static_cast<uint16_t>(clipped.x + x),
                static_cast<uint16_t>(clipped.y + y),
                color);
            if (result != SAT_OK) {
                return result;
            }
        }
    }
    return SAT_OK;
}

inline bool memory_range(const sat_surface_t* surface, uintptr_t& begin, uintptr_t& end) {
    if (validate(surface) != SAT_OK) return false;
    begin = reinterpret_cast<uintptr_t>(surface->pixels);
    const size_t row_bytes = static_cast<size_t>(surface->width) * bytes_per_pixel(surface->format);
    const size_t span = static_cast<size_t>(surface->height - 1u) * surface->pitch + row_bytes;
    if (span > static_cast<size_t>(UINTPTR_MAX - begin)) return false;
    end = begin + span;
    return true;
}

inline bool surfaces_overlap(const sat_surface_t* a, const sat_surface_t* b) {
    uintptr_t ab = 0u, ae = 0u, bb = 0u, be = 0u;
    if (!memory_range(a, ab, ae) || !memory_range(b, bb, be)) return true;
    return ab < be && bb < ae;
}

inline void move_bytes(uint8_t* destination, const uint8_t* source, size_t count) {
    if (destination == source || count == 0u) return;
    const uintptr_t dst = reinterpret_cast<uintptr_t>(destination);
    const uintptr_t src = reinterpret_cast<uintptr_t>(source);
    if (dst < src || dst >= src + count) {
        for (size_t i = 0u; i < count; ++i) destination[i] = source[i];
    } else {
        for (size_t i = count; i > 0u; --i) destination[i - 1u] = source[i - 1u];
    }
}

struct BlitRegion {
    int32_t sx;
    int32_t sy;
    int32_t dx;
    int32_t dy;
    int32_t width;
    int32_t height;
};

inline bool clip_blit(
    const sat_surface_t* destination,
    sat_point_t destination_position,
    const sat_surface_t* source,
    const sat_rect_t* src_rect,
    BlitRegion& out) {
    int32_t sx = src_rect != nullptr ? src_rect->x : 0;
    int32_t sy = src_rect != nullptr ? src_rect->y : 0;
    int32_t width = src_rect != nullptr ? static_cast<int32_t>(src_rect->width) : static_cast<int32_t>(source->width);
    int32_t height = src_rect != nullptr ? static_cast<int32_t>(src_rect->height) : static_cast<int32_t>(source->height);
    int32_t dx = destination_position.x;
    int32_t dy = destination_position.y;
    if (width <= 0 || height <= 0) return false;

    if (sx < 0) { const int32_t d = -sx; sx = 0; dx += d; width -= d; }
    if (sy < 0) { const int32_t d = -sy; sy = 0; dy += d; height -= d; }
    if (dx < 0) { const int32_t d = -dx; dx = 0; sx += d; width -= d; }
    if (dy < 0) { const int32_t d = -dy; dy = 0; sy += d; height -= d; }
    if (width <= 0 || height <= 0) return false;

    if (sx >= source->width || sy >= source->height || dx >= destination->width || dy >= destination->height) return false;
    const int32_t src_w = static_cast<int32_t>(source->width) - sx;
    const int32_t src_h = static_cast<int32_t>(source->height) - sy;
    const int32_t dst_w = static_cast<int32_t>(destination->width) - dx;
    const int32_t dst_h = static_cast<int32_t>(destination->height) - dy;
    if (width > src_w) width = src_w;
    if (height > src_h) height = src_h;
    if (width > dst_w) width = dst_w;
    if (height > dst_h) height = dst_h;
    if (width <= 0 || height <= 0) return false;
    out = {sx, sy, dx, dy, width, height};
    return true;
}

inline bool raw_copy_compatible(const sat_surface_t* destination, const sat_surface_t* source) {
    if (destination->format != source->format) return false;
    if (destination->format != SAT_PIXEL_INDEX8) return true;
    return destination->palette_rgb555 == source->palette_rgb555 && destination->palette_count == source->palette_count;
}

inline sat_result_t blit(
    sat_surface_t* destination,
    sat_point_t destination_position,
    const sat_surface_t* source,
    const sat_rect_t* src_rect) {
    const sat_result_t dst_valid = validate(destination);
    if (dst_valid != SAT_OK) return dst_valid;
    const sat_result_t src_valid = validate(source);
    if (src_valid != SAT_OK) return src_valid;
    BlitRegion region{};
    if (!clip_blit(destination, destination_position, source, src_rect, region)) return SAT_OK;

    const bool overlap = surfaces_overlap(destination, source);
    if (raw_copy_compatible(destination, source)) {
        const uint8_t bpp = bytes_per_pixel(source->format);
        const size_t row_bytes = static_cast<size_t>(region.width) * bpp;
        if (overlap && destination->pitch != source->pitch) return SAT_ERR_UNSUPPORTED;

        const uint8_t* first_src = pixel_ptr(source, static_cast<uint16_t>(region.sx), static_cast<uint16_t>(region.sy));
        uint8_t* first_dst = pixel_ptr(destination, static_cast<uint16_t>(region.dx), static_cast<uint16_t>(region.dy));
        if (overlap && reinterpret_cast<uintptr_t>(first_dst) > reinterpret_cast<uintptr_t>(first_src)) {
            for (int32_t row = region.height; row > 0; --row) {
                const int32_t offset = row - 1;
                const uint8_t* s = pixel_ptr(source, static_cast<uint16_t>(region.sx), static_cast<uint16_t>(region.sy + offset));
                uint8_t* d = pixel_ptr(destination, static_cast<uint16_t>(region.dx), static_cast<uint16_t>(region.dy + offset));
                move_bytes(d, s, row_bytes);
            }
        } else {
            for (int32_t row = 0; row < region.height; ++row) {
                const uint8_t* s = pixel_ptr(source, static_cast<uint16_t>(region.sx), static_cast<uint16_t>(region.sy + row));
                uint8_t* d = pixel_ptr(destination, static_cast<uint16_t>(region.dx), static_cast<uint16_t>(region.dy + row));
                move_bytes(d, s, row_bytes);
            }
        }
        return SAT_OK;
    }

    if (overlap) return SAT_ERR_UNSUPPORTED;
    for (int32_t y = 0; y < region.height; ++y) {
        for (int32_t x = 0; x < region.width; ++x) {
            sat_color_t color{};
            sat_result_t result = get_pixel(source, static_cast<uint16_t>(region.sx + x), static_cast<uint16_t>(region.sy + y), &color);
            if (result != SAT_OK) return result;
            result = set_pixel(destination, static_cast<uint16_t>(region.dx + x), static_cast<uint16_t>(region.dy + y), color);
            if (result != SAT_OK) return result;
        }
    }
    return SAT_OK;
}

inline sat_result_t convert(sat_surface_t* destination, const sat_surface_t* source) {
    const sat_result_t dst_valid = validate(destination);
    if (dst_valid != SAT_OK) return dst_valid;
    const sat_result_t src_valid = validate(source);
    if (src_valid != SAT_OK) return src_valid;
    if (destination->width != source->width || destination->height != source->height) return SAT_ERR_INVALID_ARG;
    if (destination->pixels == source->pixels && raw_copy_compatible(destination, source)) return SAT_OK;
    if (surfaces_overlap(destination, source)) return SAT_ERR_UNSUPPORTED;
    return blit(destination, sat_point_t{0, 0}, source, nullptr);
}

inline sat_result_t blit_scaled(
    sat_surface_t* destination,
    const sat_rect_t* destination_rect,
    const sat_surface_t* source,
    const sat_rect_t* src_rect) {
    const sat_result_t dst_valid = validate(destination);
    if (dst_valid != SAT_OK) return dst_valid;
    const sat_result_t src_valid = validate(source);
    if (src_valid != SAT_OK) return src_valid;
    if (destination_rect == nullptr || src_rect == nullptr) return SAT_ERR_INVALID_ARG;
    if (!rect_inside(source, src_rect) || destination_rect->width == 0u || destination_rect->height == 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    if (surfaces_overlap(destination, source)) return SAT_ERR_UNSUPPORTED;

    ClippedRect dst_clip{};
    if (!clip_rect_to_surface(destination, destination_rect, dst_clip)) return SAT_OK;

    const int32_t original_dx = destination_rect->x;
    const int32_t original_dy = destination_rect->y;
    for (int32_t y = 0; y < dst_clip.height; ++y) {
        const int32_t dy = dst_clip.y + y;
        const uint32_t rel_y = static_cast<uint32_t>(dy - original_dy);
        const uint32_t sy = static_cast<uint32_t>(src_rect->y) +
            (rel_y * src_rect->height) / destination_rect->height;
        for (int32_t x = 0; x < dst_clip.width; ++x) {
            const int32_t dx = dst_clip.x + x;
            const uint32_t rel_x = static_cast<uint32_t>(dx - original_dx);
            const uint32_t sx = static_cast<uint32_t>(src_rect->x) +
                (rel_x * src_rect->width) / destination_rect->width;
            sat_color_t color{};
            sat_result_t result = get_pixel(source, static_cast<uint16_t>(sx), static_cast<uint16_t>(sy), &color);
            if (result != SAT_OK) return result;
            result = set_pixel(destination, static_cast<uint16_t>(dx), static_cast<uint16_t>(dy), color);
            if (result != SAT_OK) return result;
        }
    }
    return SAT_OK;
}

}  // namespace saturn::core::surface_logic

#endif /* SATURN_CORE_SURFACE_LOGIC_HPP */
