#include <cstdio>
#include "saturn/sprite_anim.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); return 1; } } while (0)

int main() {
    sat_texture_t texture{3u, 1u};
    sat_rect_t frames[2] = {{0, 0, 8u, 8u}, {8, 0, 8u, 8u}};
    sat_sprite_region_anim_t anim{};
    sat_rect_t out{};
    OK(sat_sprite_region_anim_init(&anim, texture, frames, 2u) == SAT_OK);
    OK(sat_sprite_region_anim_source(&anim, &out) == SAT_OK && out.width == 8u);
    OK(sat_sprite_region_anim_set(&anim, 1u) == SAT_OK);
    OK(sat_sprite_region_anim_source(&anim, &out) == SAT_OK && out.x == 8);
    OK(sat_sprite_region_anim_set(&anim, 2u) == SAT_ERR_INVALID_ARG);
    std::puts("sprite region animator: OK");
    return 0;
}
