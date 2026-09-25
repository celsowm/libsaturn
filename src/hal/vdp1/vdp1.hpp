#ifndef SATURN_HAL_VDP1_HPP
#define SATURN_HAL_VDP1_HPP

#include <stdint.h>

#include "saturn/core.h"
#include "saturn/vdp1.h"

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
    uint16_t format;
    bool user_clip;
};

struct ScaledSpriteRequest {
    int16_t x0, y0;
    int16_t x1, y1;
    uint16_t width;
    uint16_t height;
    uint16_t srca;
    uint16_t palette;
    uint16_t flags;
    uint16_t format;
    bool user_clip;
};

struct DistortedSpriteRequest {
    int16_t x[4];
    int16_t y[4];
    uint16_t width;
    uint16_t height;
    uint16_t srca;
    uint16_t palette;
    uint16_t flags;
    uint16_t format;
    bool user_clip;
};

struct PolygonRequest {
    int16_t xa, ya;
    int16_t xb, yb;
    int16_t xc, yc;
    int16_t xd, yd;
    uint16_t color;
    uint16_t flags;
    bool user_clip;
};

struct LineRequest {
    int16_t x0, y0;
    int16_t x1, y1;
    uint16_t color;
    uint16_t flags;
    bool user_clip;
};

struct UserClipRequest {
    uint16_t x0, y0;
    uint16_t x1, y1;
};

void init(uint16_t width, uint16_t height, uint16_t clear_color);
void set_clear_color(uint16_t rgb555);
void set_erase_transparent();
void set_erase_enabled(bool enable, uint16_t width, uint16_t height);
void begin_frame(Command* command_buffer, uint16_t capacity);
/* Caller-owned frame command-buffer partition: protected overlay quota. */
sat_result_t reserve_overlay_commands(uint16_t count);
sat_result_t command_checkpoint(sat_vdp1_command_checkpoint_t* out);
sat_result_t command_rollback(const sat_vdp1_command_checkpoint_t* checkpoint);
void command_stats(uint16_t& used, uint16_t& capacity,
                   uint16_t& overlay_reserved, bool& overlay_pass);
sat_result_t begin_overlay_pass();
sat_result_t push_user_clip(const UserClipRequest& req);
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

/* Allocates and writes a 16-entry color lookup table; *out_colr is the
 * CMDCOLR value (VRAM address / 8, 32-byte aligned, never 0). */
sat_result_t upload_lut(const uint16_t* lut_rgb555, uint16_t* out_colr);

/* Allocates and writes a 4 bits-per-texel character pattern. */
sat_result_t upload_texture_lut4(
    const uint8_t* pixels,
    uint16_t width,
    uint16_t height,
    uint16_t* out_srca);

/* Every check update_texture_indexed8_pitched / _rect perform before
 * writing, without writing: callers preflight a multi-transfer update so it
 * fails before its first write rather than halfway through. */
sat_result_t check_texture_indexed8_update(
    uint16_t srca, uint16_t width, uint16_t height, uint16_t pitch);
sat_result_t check_texture_indexed8_rect(
    uint16_t srca, uint16_t texture_width, uint16_t texture_height, uint16_t pitch,
    uint16_t x, uint16_t y, uint16_t width, uint16_t height);

/* Rewrites an already allocated character pattern in place. The caller must
 * pass the original width/height; the range is checked against the texture
 * arena already allocated by this HAL. */
sat_result_t update_texture_indexed8_pitched(
    uint16_t srca,
    const uint8_t* pixels,
    uint16_t width,
    uint16_t height,
    uint16_t pitch);

/* Rewrites a subrectangle of an existing INDEX8 character pattern without
 * reallocating VRAM. pixels starts at the full source origin. Odd X bounds
 * expand to the adjacent 16-bit word, using the same source for both bytes. */
sat_result_t update_texture_indexed8_rect(
    uint16_t srca, const uint8_t* pixels,
    uint16_t texture_width, uint16_t texture_height, uint16_t pitch,
    uint16_t x, uint16_t y, uint16_t width, uint16_t height);

}  // namespace saturn::hal::vdp1

#endif
