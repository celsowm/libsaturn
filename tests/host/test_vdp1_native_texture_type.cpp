#define SATURN_DISABLE_LEGACY_TEXTURE_TYPE
#include "saturn/saturn.h"

#include <cstdint>
#include <type_traits>

static_assert(std::is_standard_layout_v<sat_vdp1_texture_t>);
static_assert(sizeof(sat_vdp1_texture_t) == 12u);
static_assert(std::is_same_v<decltype(sat_sprite_cmd_t::texture), const sat_vdp1_texture_t*>);
static_assert(std::is_same_v<decltype(sat_scaled_sprite_cmd_t::texture), const sat_vdp1_texture_t*>);
static_assert(std::is_same_v<decltype(sat_distorted_sprite_cmd_t::texture), const sat_vdp1_texture_t*>);
static_assert(std::is_same_v<decltype(sat_mesh_draw_t::textures), const sat_vdp1_texture_t*>);
static_assert(std::is_same_v<decltype(sat_ascii_font_t::glyphs[0]), sat_vdp1_texture_t>);

int main() {
    sat_vdp1_texture_t texture{};
    texture.valid = 1u;
    return texture.valid == 1u ? 0 : 1;
}
