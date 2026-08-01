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

void init(uint16_t width, uint16_t height, uint16_t clear_color);
void set_clear_color(uint16_t rgb555);
void set_erase_transparent();
void set_erase_enabled(bool enable, uint16_t width, uint16_t height);
void begin_frame(Command* command_buffer, uint16_t capacity);
sat_result_t push_sprite(const SpriteRequest& req);
void submit();

sat_result_t upload_palette(const uint16_t* palette_rgb555, uint16_t palette_index);
sat_result_t upload_texture_indexed8(const uint8_t* pixels, uint16_t width, uint16_t height, uint16_t* out_srca);

}  // namespace saturn::hal::vdp1

#endif

