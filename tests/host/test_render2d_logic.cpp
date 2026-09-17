#include <cstdio>
#include <cstdlib>

#include "src/core/render2d_logic.hpp"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

int main() {
    using namespace saturn::core;

    const sat_draw_params_t defaults = sat_draw_params_default();
    OK(defaults.rotation == 0);
    OK(defaults.flip == SAT_FLIP_NONE);
    OK(defaults.blend_mode == SAT_BLEND_NONE);
    OK(render2d_neutral_tint(defaults.tint));
    OK(validate_render2d_params(nullptr) == SAT_OK);
    OK(validate_render2d_params(&defaults) == SAT_OK);

    sat_draw_params_t unsupported = defaults;
    unsupported.rotation = SAT_FX16_ONE;
    OK(validate_render2d_params(&unsupported) == SAT_ERR_UNSUPPORTED);
    unsupported = defaults;
    unsupported.flip = SAT_FLIP_X;
    OK(validate_render2d_params(&unsupported) == SAT_ERR_UNSUPPORTED);
    unsupported = defaults;
    unsupported.tint.r = 254u;
    OK(validate_render2d_params(&unsupported) == SAT_ERR_UNSUPPORTED);
    unsupported = defaults;
    unsupported.blend_mode = SAT_BLEND_ALPHA;
    OK(validate_render2d_params(&unsupported) == SAT_ERR_UNSUPPORTED);

    sat_draw_params_t invalid = defaults;
    invalid.flip = 4u;
    OK(validate_render2d_params(&invalid) == SAT_ERR_INVALID_ARG);
    invalid = defaults;
    invalid.blend_mode = 99u;
    OK(validate_render2d_params(&invalid) == SAT_ERR_INVALID_ARG);
    invalid = defaults;
    invalid.flags = 1u;
    OK(validate_render2d_params(&invalid) == SAT_ERR_INVALID_ARG);

    Render2DDestination resolved{};
    const sat_rect_t identity{10, 20, 16u, 8u};
    OK(resolve_render2d_destination(&identity, 320u, 224u, 16u, 8u, &resolved) == SAT_OK);
    OK(resolved.x0 == -150 && resolved.y0 == -92);
    OK(resolved.x1 == -135 && resolved.y1 == -85);
    OK(!resolved.scaled);

    const sat_rect_t scaled{10, 20, 32u, 16u};
    OK(resolve_render2d_destination(&scaled, 320u, 224u, 16u, 8u, &resolved) == SAT_OK);
    OK(resolved.x0 == -150 && resolved.y0 == -92);
    OK(resolved.x1 == -119 && resolved.y1 == -77);
    OK(resolved.scaled);

    const sat_rect_t zero{0, 0, 0u, 8u};
    OK(resolve_render2d_destination(&zero, 320u, 224u, 16u, 8u, &resolved) == SAT_ERR_INVALID_ARG);
    OK(resolve_render2d_destination(nullptr, 320u, 224u, 16u, 8u, &resolved) == SAT_ERR_INVALID_ARG);

    const sat_rect_t overflow{32760, 0, 100u, 8u};
    OK(resolve_render2d_destination(&overflow, 1u, 224u, 16u, 8u, &resolved) == SAT_ERR_INVALID_ARG);

    std::puts("render2d logic: OK");
    return 0;
}
