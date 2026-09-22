#include <cstdio>
#include <cstdlib>

#include "saturn/collide3d.h"
#include "src/physics/3d/collision_grid.hpp"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); std::exit(1); } } while (0)

static sat_fx16_t F(int x) { return static_cast<sat_fx16_t>(x * 65536); }

int main() {
    using namespace saturn::core::collide3d;

    sat_vec3_t vertices[8] = {
        {F(-2),0,F(-2)}, {F(2),0,F(-2)}, {F(2),0,F(2)}, {F(-2),0,F(2)},
        {F(30),0,F(-2)}, {F(34),0,F(-2)}, {F(34),0,F(2)}, {F(30),0,F(2)}
    };
    uint16_t indices[8] = {0,1,2,3, 4,5,6,7};
    sat_mesh_t mesh{vertices, indices, 8u, 8u, 2u, 2u};

    uint16_t heads[16]{};
    sat_mesh3_grid_entry_t entries[64]{};
    uint16_t stamps[2]{};
    sat_mesh3_grid_t grid{};
    OK(mesh3_grid_init(grid, mesh, 3u, heads, 16u, entries, 64u, stamps, 2u) == SAT_OK);
    OK(grid.entry_count > 0u);
    OK(grid.entry_count < 64u);

    sat_sphere_t near_first{{0,F(1),0},F(2)};
    sat_contact3_t contacts[4]{};
    uint16_t count = 0u;
    OK(sphere_mesh_grid(grid, near_first, contacts, 4u, count) == SAT_OK);
    OK(count == 1u);
    OK(contacts[0].depth > 0);

    sat_sphere_t between{{F(16),F(1),0},F(2)};
    OK(sphere_mesh_grid(grid, between, contacts, 4u, count) == SAT_OK);
    OK(count == 0u);

    sat_sphere_t near_second{{F(32),F(1),0},F(2)};
    OK(sphere_mesh_grid(grid, near_second, contacts, 4u, count) == SAT_OK);
    OK(count == 1u);

    sat_mesh3_grid_t too_small{};
    sat_mesh3_grid_entry_t one_entry[1]{};
    OK(mesh3_grid_init(too_small, mesh, 3u, heads, 16u, one_entry, 1u, stamps, 2u) == SAT_ERR_CAPACITY);

    sat_vec3_t negative_vertices[4] = {
        {F(-34),0,F(-2)}, {F(-30),0,F(-2)}, {F(-30),0,F(2)}, {F(-34),0,F(2)}
    };
    uint16_t negative_indices[4] = {0,1,2,3};
    sat_mesh_t negative_mesh{negative_vertices, negative_indices, 4u, 4u, 1u, 1u};
    sat_mesh3_grid_entry_t negative_entries[32]{};
    uint16_t negative_stamps[1]{};
    OK(mesh3_grid_init(grid, negative_mesh, 3u, heads, 16u, negative_entries, 32u, negative_stamps, 1u) == SAT_OK);
    sat_sphere_t negative_sphere{{F(-32),F(1),0},F(2)};
    OK(sphere_mesh_grid(grid, negative_sphere, contacts, 4u, count) == SAT_OK);
    OK(count == 1u);

    std::puts("mesh3 collision grid: OK");
    return 0;
}
