#ifndef SATURN_HAL_VDP1_HPP
#define SATURN_HAL_VDP1_HPP

#include <stdint.h>

#include "saturn/core.h"

namespace saturn::hal::vdp1 {

struct alignas(4) Command {
    uint16_t ctrl;
    uint16_t link;
    uint16_t pmod;
    uint16_t colr;
    uint16_t srca;
    uint16_t size;
    int16_t xa;
    int16_t ya;
    int16_t xb;
    int16_t yb;
    int16_t xc;
    int16_t yc;
    int16_t xd;
    int16_t yd;
    uint16_t grda;
    uint16_t pad;
};

struct SpriteRequest {
    int16_t x;
    int16_t y;
    uint16_t width;
    uint16_t height;
    uint16_t srca;
    uint16_t palette;
    uint16_t flags;
};

struct ScaledSpriteRequest {
    int16_t x0, y0;
    int16_t x1, y1;
    uint16_t width;
    uint16_t height;
    uint16_t srca;
    uint16_t palette;
    uint16_t flags;
};

struct DistortedSpriteRequest {
    int16_t x[4];
    int16_t y[4];
    uint16_t width;
    uint16_t height;
    uint16_t srca;
    uint16_t palette;
    uint16_t flags;
};

struct PolygonRequest {
    int16_t xa, ya;
    int16_t xb, yb;
    int16_t xc, yc;
    int16_t xd, yd;
    uint16_t color;
    uint16_t flags;
};

struct LineRequest {
    int16_t x0, y0;
    int16_t x1, y1;
    uint16_t color;
    uint16_t flags;
};

void init(uint16_t width, uint16_t height, uint16_t clear_color);
void set_clear_color(uint16_t rgb555);
void set_erase_transparent();
void set_erase_enabled(bool enable, uint16_t width, uint16_t height);
void begin_frame(Command* command_buffer, uint16_t capacity);
sat_result_t push_sprite(const SpriteRequest& req);
sat_result_t push_scaled_sprite(const ScaledSpriteRequest& req);
sat_result_t push_distorted_sprite(const DistortedSpriteRequest& req);
sat_result_t push_polygon(const PolygonRequest& req);
sat_result_t push_polyline(const PolygonRequest& req);
sat_result_t push_line(const LineRequest& req);
sat_result_t push_polygon_gouraud(const PolygonRequest& req, const uint16_t* gouraud);
sat_result_t push_polyline_gouraud(const PolygonRequest& req, const uint16_t* gouraud);
sat_result_t push_line_gouraud(const LineRequest& req, const uint16_t* gouraud);
void submit();

sat_result_t upload_palette(const uint16_t* palette_rgb555, uint16_t palette_index);

/* Uploads a VDP1 indexed8 character pattern from rows separated by `pitch`
 * bytes. Width is the logical row width and must obey VDP1's multiple-of-8
 * rule; padding bytes are never copied. */
sat_result_t upload_texture_indexed8_pitched(
    const uint8_t* pixels,
    uint16_t width,
    uint16_t height,
    uint16_t pitch,
    uint16_t* out_srca);

sat_result_t upload_texture_indexed8(
    const uint8_t* pixels,
    uint16_t width,
    uint16_t height,
    uint16_t* out_srca);

/* Rewrites an already allocated character pattern in place. The caller must
 * pass the original width/height; the range is checked against the texture
 * arena already allocated by this HAL. */
sat_result_t update_texture_indexed8_pitched(
    uint16_t srca,
    const uint8_t* pixels,
    uint16_t width,
    uint16_t height,
    uint16_t pitch);

}  // namespace saturn::hal::vdp1

#endif
