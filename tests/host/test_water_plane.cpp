#include <cassert>
#include <cstdint>
#include <initializer_list>
#include "saturn/water_plane.h"

namespace {

constexpr sat_fx16_t one = SAT_FX16_ONE;

sat_water_plane_config_t base() {
    sat_water_plane_config_t c{};
    c.min_x = -1000 * one; c.max_x = 1000 * one; c.min_z = -1000 * one; c.max_z = 1000 * one;
    c.surface = -one; c.cell = 2 * one; c.cells = 8; c.rings = 4;
    c.near_color = c.far_color = 0x4210u;
    return c;
}

struct Fixture {
    sat_water_plane_t plane;
};

uint16_t faces_of(const sat_water_plane_t& p) {
    uint16_t n = 0;
    assert(sat_water_plane_face_count(&p, &n) == SAT_OK);
    return n;
}

void validation() {
    static sat_water_plane_t plane;
    sat_water_plane_config_t c = base();
    assert(sat_water_plane_build(nullptr, &c, 0, 0) == SAT_ERR_INVALID_ARG);
    assert(sat_water_plane_build(&plane, nullptr, 0, 0) == SAT_ERR_INVALID_ARG);
    c.cells = 6;
    assert(sat_water_plane_build(&plane, &c, 0, 0) == SAT_ERR_INVALID_ARG);
    c = base(); c.rings = 0;
    assert(sat_water_plane_build(&plane, &c, 0, 0) == SAT_ERR_INVALID_ARG);
    c = base(); c.rings = 5;
    assert(sat_water_plane_build(&plane, &c, 0, 0) == SAT_ERR_INVALID_ARG);
    c = base(); c.cell = 0;
    assert(sat_water_plane_build(&plane, &c, 0, 0) == SAT_ERR_INVALID_ARG);
    c = base(); c.min_x = c.max_x;
    assert(sat_water_plane_build(&plane, &c, 0, 0) == SAT_ERR_INVALID_ARG);
    c = base(); c.wobble = one;
    assert(sat_water_plane_build(&plane, &c, 0, 0) == SAT_ERR_INVALID_ARG);
    // Positions are 32-bit: a world past 8192 units, or rings that reach it, are refused.
    c = base(); c.max_x = 9000 * one;
    assert(sat_water_plane_build(&plane, &c, 0, 0) == SAT_ERR_INVALID_ARG);
    c = base();
    assert(sat_water_plane_build(&plane, &c, 9000 * one, 0) == SAT_ERR_INVALID_ARG);
    c = base(); c.cell = 600 * one;
    assert(sat_water_plane_build(&plane, &c, 0, 0) == SAT_ERR_INVALID_ARG);
    c = base(); c.cell = 50 * one;
    assert(sat_water_plane_build(&plane, &c, 0, 0) == SAT_OK);
}

/* Each ring covers a square that doubles in size, and the holes remove
 * exactly the inner ring's footprint: the faces tile the outer square. */
void rings_tile_the_outer_square() {
    static sat_water_plane_t plane;
    for (unsigned cells : {4u, 8u}) {
        for (unsigned rings = 1; rings <= 4; ++rings) {
            sat_water_plane_config_t c = base();
            c.cells = static_cast<uint8_t>(cells);
            c.rings = static_cast<uint8_t>(rings);
            assert(sat_water_plane_build(&plane, &c, 37 * one + 5, -12 * one) == SAT_OK);
            int64_t area = 0;
            uint16_t total = 0;
            for (unsigned k = 0; k < rings; ++k) {
                const int64_t size = static_cast<int64_t>(c.cell) << k;
                const uint16_t f = plane.ring[k].mesh.face_count;
                const unsigned expect = k == 0 ? cells * cells : cells * cells - (cells / 2) * (cells / 2);
                assert(f == expect);
                area += static_cast<int64_t>(f) * (size / one) * (size / one);
                total = static_cast<uint16_t>(total + f);
            }
            const int64_t side = static_cast<int64_t>(cells) * ((c.cell << (rings - 1)) / one);
            assert(area == side * side);
            assert(faces_of(plane) == total);
        }
    }
}

/* Faces never overlap, and every point of the outer square lies in one:
 * sample the plan view and count the faces covering each point. */
void no_overlap_and_no_gap() {
    static sat_water_plane_t plane;
    sat_water_plane_config_t c = base();
    c.cells = 4; c.rings = 3;
    const sat_fx16_t cam_x = 3 * one + 11, cam_z = 9 * one;
    assert(sat_water_plane_build(&plane, &c, cam_x, cam_z) == SAT_OK);
    // The last ring is snapped to 2 * 8 units and covers 4 cells of 8 units each side.
    const int64_t size = static_cast<int64_t>(c.cell) << 2, step = 16 * static_cast<int64_t>(one);
    const int64_t centre_x = (cam_x / step) * step, centre_z = (cam_z / step) * step;
    const int64_t half = size * 2;
    for (int64_t z = centre_z - half; z < centre_z + half; z += one / 2 + 1) {
        for (int64_t x = centre_x - half; x < centre_x + half; x += one / 2 + 3) {
            int covered = 0;
            for (unsigned k = 0; k < c.rings; ++k) {
                const sat_mesh_t& m = plane.ring[k].mesh;
                for (uint16_t f = 0; f < m.face_count; ++f) {
                    const sat_vec3_t& a = m.vertices[m.indices[4 * f]];
                    const sat_vec3_t& d = m.vertices[m.indices[4 * f + 2]];
                    const int64_t x0 = a.x < d.x ? a.x : d.x, x1 = a.x < d.x ? d.x : a.x;
                    const int64_t z0 = a.z < d.z ? a.z : d.z, z1 = a.z < d.z ? d.z : a.z;
                    if (x >= x0 && x < x1 && z >= z0 && z < z1) ++covered;
                }
            }
            assert(covered == 1);
        }
    }
}

void snapping_keeps_vertices_still() {
    static sat_water_plane_t a, b;
    sat_water_plane_config_t c = base();
    assert(sat_water_plane_build(&a, &c, 100 * one, 100 * one) == SAT_OK);
    assert(sat_water_plane_build(&b, &c, 100 * one + one / 2, 100 * one + one - 1) == SAT_OK);
    // Within one snap step of 2 * cell, ring 0 does not move.
    for (unsigned v = 0; v < a.ring[0].mesh.vertex_count; ++v) {
        assert(a.ring[0].vertices[v].x == b.ring[0].vertices[v].x);
        assert(a.ring[0].vertices[v].z == b.ring[0].vertices[v].z);
    }
    assert(sat_water_plane_build(&b, &c, 100 * one + 2 * c.cell, 100 * one) == SAT_OK);
    assert(b.ring[0].vertices[0].x == a.ring[0].vertices[0].x + 2 * c.cell);
    // Every vertex lies on a multiple of its ring's cell size.
    for (unsigned k = 0; k < c.rings; ++k) {
        const int64_t size = static_cast<int64_t>(c.cell) << k;
        for (unsigned v = 0; v < a.ring[k].mesh.vertex_count; ++v) {
            assert(a.ring[k].vertices[v].x % size == 0 && a.ring[k].vertices[v].z % size == 0);
        }
    }
    // Negative coordinates snap down (to -2 * cell), not toward zero.
    assert(sat_water_plane_build(&a, &c, -one / 4, -one / 4) == SAT_OK);
    assert(a.ring[0].vertices[4 * 9 + 4].x == -2 * c.cell && a.ring[0].vertices[4 * 9 + 4].z == -2 * c.cell);
}

void region_clamps_and_drops() {
    static sat_water_plane_t plane;
    sat_water_plane_config_t c = base();
    c.min_x = -3 * one; c.max_x = 7 * one; c.min_z = -5 * one; c.max_z = 5 * one;
    assert(sat_water_plane_build(&plane, &c, 2 * one, 0) == SAT_OK);
    int64_t area = 0;
    for (unsigned k = 0; k < c.rings; ++k) {
        const sat_mesh_t& m = plane.ring[k].mesh;
        for (unsigned v = 0; v < m.vertex_count; ++v) {
            assert(m.vertices[v].x >= c.min_x && m.vertices[v].x <= c.max_x);
            assert(m.vertices[v].z >= c.min_z && m.vertices[v].z <= c.max_z);
        }
        for (uint16_t f = 0; f < m.face_count; ++f) {
            const sat_vec3_t& a = m.vertices[m.indices[4 * f]];
            const sat_vec3_t& d = m.vertices[m.indices[4 * f + 2]];
            assert(a.x != d.x && a.z != d.z);
            const int64_t w = a.x < d.x ? d.x - a.x : a.x - d.x;
            const int64_t h = a.z < d.z ? d.z - a.z : a.z - d.z;
            area += (w / one) * (h / one) + ((w % one) * (h % one) / one) / one;
        }
    }
    // The whole 10 x 10 region is covered exactly once (the rings reach 32 units).
    assert(area == 100);
    // A region smaller than a cell still yields faces, never a degenerate one.
    c.min_x = 0; c.max_x = one / 8; c.min_z = 0; c.max_z = one / 8;
    assert(sat_water_plane_build(&plane, &c, 0, 0) == SAT_OK);
    assert(faces_of(plane) >= 1);
}

void winding_flips_underneath() {
    static sat_water_plane_t up, down;
    sat_water_plane_config_t c = base();
    assert(sat_water_plane_build(&up, &c, 0, 0) == SAT_OK);
    c.underside = 1;
    assert(sat_water_plane_build(&down, &c, 0, 0) == SAT_OK);
    assert(faces_of(up) == faces_of(down));
    for (unsigned k = 0; k < c.rings; ++k) {
        for (uint16_t f = 0; f < up.ring[k].mesh.face_count; ++f) {
            const uint16_t* u = &up.ring[k].indices[4 * f];
            const uint16_t* d = &down.ring[k].indices[4 * f];
            assert(u[0] == d[0] && u[1] == d[3] && u[2] == d[2] && u[3] == d[1]);
        }
    }
}

void wobble_fades_to_flat_at_the_ring_edge() {
    static sat_water_plane_t plane;
    sat_water_plane_config_t c = base();
    c.wobble = one / 2; c.period = 8 * one; c.speed = 6; c.tick = 5; c.glint = 4;
    assert(sat_water_plane_build(&plane, &c, 0, 0) == SAT_OK);
    const unsigned n = c.cells, stride = n + 1;
    bool moved = false;
    for (unsigned j = 0; j <= n; ++j) {
        for (unsigned i = 0; i <= n; ++i) {
            const sat_fx16_t y = plane.ring[0].vertices[j * stride + i].y;
            const bool edge = i == 0 || j == 0 || i == n || j == n;
            if (edge) assert(y == c.surface);
            else if (y != c.surface) moved = true;
            assert(y >= c.surface - c.wobble && y <= c.surface + c.wobble);
        }
    }
    assert(moved);
    for (unsigned k = 1; k < c.rings; ++k)
        for (unsigned v = 0; v < plane.ring[k].mesh.vertex_count; ++v)
            assert(plane.ring[k].vertices[v].y == c.surface);
    c.wobble = 0;
    assert(sat_water_plane_build(&plane, &c, 0, 0) == SAT_OK);
    for (unsigned v = 0; v < plane.ring[0].mesh.vertex_count; ++v)
        assert(plane.ring[0].vertices[v].y == c.surface);
}

void light_blends_with_distance() {
    static sat_water_plane_t plane;
    sat_water_plane_config_t c = base();
    c.near_color = sat_gouraud_rgb(0, 0, 0);
    c.far_color = sat_gouraud_rgb(-10, -6, 4);
    assert(sat_water_plane_build(&plane, &c, 0, 0) == SAT_OK);
    const sat_water_plane_ring_t& r0 = plane.ring[0];
    const uint16_t centre = r0.light[4 * 9 + 4];
    assert(centre == sat_gouraud_rgb(0, 0, 0));
    const sat_water_plane_ring_t& last = plane.ring[3];
    const uint16_t edge = last.light[0];  // the far corner is at or past the outer reach
    assert(edge == c.far_color);
    // Brightness is monotone with distance along a row.
    int prev = 17;
    for (unsigned i = 4; i <= 8; ++i) {
        const int red = r0.light[4 * 9 + i] & 31;
        assert(red <= prev);
        prev = red;
    }
}

void cull_drops_cells_behind() {
    static sat_water_plane_t all, front;
    sat_water_plane_config_t c = base();
    assert(sat_water_plane_build(&all, &c, 0, 0) == SAT_OK);
    c.forward_x = 0; c.forward_z = one; c.behind = 4 * one;
    assert(sat_water_plane_build(&front, &c, 0, 0) == SAT_OK);
    assert(faces_of(front) < faces_of(all));
    for (unsigned k = 0; k < c.rings; ++k) {
        const sat_mesh_t& m = front.ring[k].mesh;
        for (uint16_t f = 0; f < m.face_count; ++f) {
            int64_t furthest = INT64_MIN;
            for (unsigned v = 0; v < 4; ++v)
                if (m.vertices[m.indices[4 * f + v]].z > furthest) furthest = m.vertices[m.indices[4 * f + v]].z;
            assert(furthest >= -4 * static_cast<int64_t>(one));  // never wholly behind the margin
        }
    }
    // The face under the camera and ahead of it stay.
    bool under = false;
    for (uint16_t f = 0; f < front.ring[0].mesh.face_count; ++f) {
        const sat_mesh_t& m = front.ring[0].mesh;
        const sat_vec3_t& a = m.vertices[m.indices[4 * f]];
        if (a.x == 0 && a.z == 0) under = true;
    }
    assert(under);
}

}

int main() {
    validation();
    rings_tile_the_outer_square();
    no_overlap_and_no_gap();
    snapping_keeps_vertices_still();
    region_clamps_and_drops();
    winding_flips_underneath();
    wobble_fades_to_flat_at_the_ring_edge();
    light_blends_with_distance();
    cull_drops_cells_behind();
}
