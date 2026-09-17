#include "saturn/saturn.h"

#include <cstdint>
#include <type_traits>

static_assert(std::is_standard_layout_v<sat_vdp1_texture_t>);
static_assert(std::is_standard_layout_v<sat_texture_t>);
static_assert(sizeof(sat_vdp1_texture_t) == 12u);
static_assert(sizeof(sat_texture_t) == 4u);
static_assert(!std::is_same_v<sat_texture_t, sat_vdp1_texture_t>);
static_assert(std::is_same_v<decltype(sat_sprite_cmd_t::texture), const sat_vdp1_texture_t*>);
static_assert(std::is_same_v<decltype(sat_scaled_sprite_cmd_t::texture), const sat_vdp1_texture_t*>);
static_assert(std::is_same_v<decltype(sat_distorted_sprite_cmd_t::texture), const sat_vdp1_texture_t*>);
static_assert(std::is_same_v<decltype(sat_mesh_draw_t::textures), const sat_vdp1_texture_t*>);
static_assert(std::is_same_v<std::remove_extent_t<decltype(sat_ascii_font_t::glyphs)>, sat_vdp1_texture_t>);

int main() {
    sat_vdp1_texture_t native{};
    sat_texture_t logical{};
    native.valid = 1u;
    logical.slot = 2u;
    logical.generation = 3u;
    return (native.valid == 1u && logical.slot == 2u && logical.generation == 3u) ? 0 : 1;
}
