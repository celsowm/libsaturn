#include <cstdio>
#include <cstdlib>

#include "saturn/render3d.h"
#include "src/core/runtime_state.hpp"
#include "src/hal/vdp1.hpp"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

namespace {
saturn::hal::vdp1::PolygonRequest last{};
unsigned flat_calls = 0u;
unsigned shaded_calls = 0u;
}

namespace saturn::hal::vdp1 {
sat_result_t push_polygon(const PolygonRequest& req) {
    last = req;
    ++flat_calls;
    return SAT_OK;
}
sat_result_t push_polygon_gouraud(const PolygonRequest& req, const uint16_t*) {
    last = req;
    ++shaded_calls;
    return SAT_OK;
}
}

extern "C" sat_result_t sat_draw_sprite_distorted(const sat_distorted_sprite_cmd_t*) {
    return SAT_OK;
}

int main() {
    saturn::core::g_state = {};
    sat_quad2_t quad{};
    OK(sat_draw_quad2_polygon_effects(&quad, 0x801Fu,
       SAT_SPRITE_FLAG_HALF_TRANSPARENT) == SAT_ERR_NOT_INITIALIZED);
    saturn::core::g_state.initialized = true;
    saturn::core::g_state.config.width = 320u;
    saturn::core::g_state.config.height = 224u;
    quad.x[0] = -10; quad.x[1] = 10; quad.x[2] = 10; quad.x[3] = -10;
    quad.y[0] = -10; quad.y[1] = -10; quad.y[2] = 10; quad.y[3] = 10;
    OK(sat_draw_quad2_polygon_effects(
        &quad, 0x801Fu, SAT_SPRITE_FLAG_HALF_TRANSPARENT) == SAT_OK);
    OK(flat_calls == 1u && last.flags == SAT_SPRITE_FLAG_HALF_TRANSPARENT);
    OK(last.color == 0x801Fu && last.xa == -10 && last.yc == 10);
    uint16_t corners[4] = {0x4210u, 0x4210u, 0x4210u, 0x4210u};
    OK(sat_draw_quad2_polygon_gouraud_effects(
        &quad, 0x801Fu, corners, SAT_SPRITE_FLAG_MESH) == SAT_OK);
    OK(shaded_calls == 1u && last.flags == SAT_SPRITE_FLAG_MESH);
    OK(sat_draw_quad2_polygon(&quad, 0x801Fu) == SAT_OK);
    OK(flat_calls == 2u && last.flags == 0u);
    OK(sat_draw_quad2_polygon_effects(nullptr, 0x801Fu, 0u) == SAT_ERR_INVALID_ARG);

    /* sat_quad2_area2: A(-10,-10) B(10,-10) C(10,10) D(-10,10), a 20x20
     * square, shoelace = (Cx-Ax)*(Dy-By) - (Dx-Bx)*(Cy-Ay) = 800. */
    OK(sat_quad2_area2(&quad) == 800);
    OK(sat_quad2_area2(nullptr) == 0);

    /* sat_gouraud_lambert_highlight: a normal turned away from the highlight
     * threshold matches plain sat_gouraud_lambert exactly (the highlight term
     * only ever adds, never subtracts); a normal facing the light dead-on
     * pushes past it and comes out brighter than the plain version. */
    {
        const sat_vec3_t light = {0, SAT_FX16_ONE, 0};
        const sat_vec3_t glancing = {SAT_FX16_ONE, 0, 0};
        const sat_vec3_t facing = {0, SAT_FX16_ONE, 0};
        const sat_fx16_t ambient = SAT_FX16_ONE / 4;
        const sat_fx16_t highlight_start = (SAT_FX16_ONE * 4) / 5;
        uint16_t plain = 0u;
        uint16_t highlight = 0u;

        OK(sat_gouraud_lambert(&glancing, 1u, &light, ambient, &plain) == SAT_OK);
        OK(sat_gouraud_lambert_highlight(
            &glancing, 1u, &light, ambient, highlight_start, 3, &highlight) == SAT_OK);
        OK(plain == highlight);

        OK(sat_gouraud_lambert(&facing, 1u, &light, ambient, &plain) == SAT_OK);
        OK(sat_gouraud_lambert_highlight(
            &facing, 1u, &light, ambient, highlight_start, 3, &highlight) == SAT_OK);
        OK(highlight > plain);

        OK(sat_gouraud_lambert_highlight(
            nullptr, 1u, &light, ambient, highlight_start, 3, &highlight)
            == SAT_ERR_INVALID_ARG);
        OK(sat_gouraud_lambert_highlight(
            &facing, 1u, nullptr, ambient, highlight_start, 3, &highlight)
            == SAT_ERR_INVALID_ARG);
    }

    std::puts("render3d effects: OK");
    return 0;
}
