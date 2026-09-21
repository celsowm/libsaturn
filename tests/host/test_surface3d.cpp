#include <cstdio>

#include "saturn/surface3d.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); return 1; } } while (0)

int main() {
    const sat_surface3d_rect_t hole = { -2, 2, -4, 4 };
    const sat_surface3d_desc_t surface = {0, 0, 100, 10, 10, 20, &hole, 1u};
    sat_surface3d_rect_t slices[4] = {};
    int32_t y = 0;
    OK(sat_surface3d_height(&surface, 0, 10, &y) == SAT_OK && y == 120);
    OK(sat_surface3d_split(&surface, slices) == 4u);
    OK(sat_surface3d_supports_footprint(&surface, -7, 0, 2, 2) != 0u);
    OK(sat_surface3d_supports_footprint(&surface, 0, 0, 1, 1) == 0u);
    std::puts("surface3d: OK");
    return 0;
}
