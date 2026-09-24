/* test_mesh3d_textured.cpp -- host tests for the native textured mesh path.
 *
 * Links the opt-in renderer and standalone geometry C API while stubbing VDP1 submit
 * entry points, so the shared culling/sorting/submit logic is exercised
 * without real hardware.
 */

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "saturn/math3d.h"
#include "saturn/mesh3d_draw.h"

#define ASSERT_EQ(a, b) do { if ((a) != (b)) { \
    fprintf(stderr, "FAIL %s:%d: %s (%ld) != %s (%ld)\n", __FILE__, __LINE__, \
            #a, (long)(a), #b, (long)(b)); \
    exit(1); } } while(0)
#define ASSERT_TRUE(cond) do { if (!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    exit(1); } } while(0)



namespace {

constexpr uint16_t kVCap = 64;
constexpr uint16_t kFCap = 16;

sat_vec3_t g_vertices[kVCap];
uint16_t g_indices[kFCap * 4];
uint8_t g_order[kFCap];
uint32_t g_depth[kFCap];

int g_polygon_calls;
int g_sprite_calls;
const sat_vdp1_texture_t* g_last_sprite_tex;
sat_result_t g_polygon_status = SAT_OK;
sat_result_t g_sprite_status = SAT_OK;
/* World-x of each submitted face center, in call order, for order checks. */
int g_call_x[32];
int g_call_count;
int g_call_is_sprite[32];
int g_project_calls;
int g_gouraud_calls;
uint16_t g_last_gouraud[4];

sat_mat4_t g_identity;

void reset_stubs() {
    g_polygon_calls = 0;
    g_sprite_calls = 0;
    g_last_sprite_tex = nullptr;
    g_polygon_status = SAT_OK;
    g_sprite_status = SAT_OK;
    g_call_count = 0;
    g_project_calls = 0;
    g_gouraud_calls = 0;
}

void record_quad2(const sat_quad2_t* quad, int is_sprite) {
    if (g_call_count < 32) {
        g_call_x[g_call_count] = (quad->x[0] + quad->x[1] + quad->x[2] + quad->x[3]) / 4;
        g_call_is_sprite[g_call_count] = is_sprite;
        ++g_call_count;
    }
}

sat_mesh_t make_mesh() {
    sat_mesh_t mesh;
    if (sat_mesh_init(&mesh, g_vertices, kVCap, g_indices, kFCap) != SAT_OK) {
        fprintf(stderr, "FAIL: sat_mesh_init\n");
        exit(1);
    }
    return mesh;
}

void make_identity() {
    for (int i = 0; i < 16; ++i) {
        g_identity.m[i] = 0;
    }
    g_identity.m[0] = SAT_FX16_ONE;
    g_identity.m[5] = SAT_FX16_ONE;
    g_identity.m[10] = SAT_FX16_ONE;
    g_identity.m[15] = SAT_FX16_ONE;
}

/* Two quads side by side in the z=0 plane, both facing +Z. */
void build_two_quads(sat_mesh_t* mesh) {
    uint16_t a, b, c, d, e, f, g, h;
    sat_mesh_add_vertex(mesh, 0, sat_fx16_from_int(16), 0, &a);
    sat_mesh_add_vertex(mesh, sat_fx16_from_int(16), sat_fx16_from_int(16), 0, &b);
    sat_mesh_add_vertex(mesh, sat_fx16_from_int(16), 0, 0, &c);
    sat_mesh_add_vertex(mesh, 0, 0, 0, &d);
    sat_mesh_add_face(mesh, a, b, c, d);
    sat_mesh_add_vertex(mesh, sat_fx16_from_int(32), sat_fx16_from_int(16), 0, &e);
    sat_mesh_add_vertex(mesh, sat_fx16_from_int(48), sat_fx16_from_int(16), 0, &f);
    sat_mesh_add_vertex(mesh, sat_fx16_from_int(48), 0, 0, &g);
    sat_mesh_add_vertex(mesh, sat_fx16_from_int(32), 0, 0, &h);
    sat_mesh_add_face(mesh, e, f, g, h);
}

sat_mesh_draw_t base_draw(const sat_vec3_t& eye) {
    sat_mesh_draw_t p = {};
    p.view_proj = &g_identity;
    p.eye = eye;
    p.color = 0x8000u;
    p.flags = 0u;
    return p;
}

}  // namespace

/* Projection stub for the screen-space path. Native x is the whole-unit
 * world x and native y the negated world y (world up, native down), so a
 * face wound as mesh3d.h specifies and facing +Z comes out front-facing.
 * w grows with world x -- the legacy sort tests put the eye on the -X side,
 * so larger x is farther -- and a negative z stands for "behind the
 * camera". Counts every vertex it is handed. */
extern "C" sat_result_t sat_project_vertices(
    const sat_mat4_t*, const sat_vec3_t* points, uint16_t count, sat_projected_vertex_t* out) {
    g_project_calls += count;
    for (uint16_t i = 0; i < count; ++i) {
        const sat_vec3_t& p = points[i];
        out[i].x = (int16_t)sat_fx16_to_int(p.x);
        out[i].y = (int16_t)(-sat_fx16_to_int(p.y));
        out[i].w = (p.z < 0) ? 0 : (sat_fx16_from_int(100) + p.x);
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_draw_quad2_polygon(const sat_quad2_t* quad, uint16_t) {
    ++g_polygon_calls;
    record_quad2(quad, 0);
    return g_polygon_status;
}

extern "C" sat_result_t sat_draw_quad2_sprite(
    const sat_quad2_t* quad, const sat_vdp1_texture_t* texture, uint16_t, uint16_t) {
    ++g_sprite_calls;
    g_last_sprite_tex = texture;
    record_quad2(quad, 1);
    return g_sprite_status;
}

extern "C" sat_result_t sat_draw_quad2_polygon_gouraud(
    const sat_quad2_t* quad, uint16_t, const uint16_t gouraud[4]) {
    ++g_polygon_calls;
    ++g_gouraud_calls;
    for (int i = 0; i < 4; ++i) {
        g_last_gouraud[i] = gouraud[i];
    }
    record_quad2(quad, 0);
    return g_polygon_status;
}

extern "C" sat_result_t sat_draw_world_polygon_gouraud(
    const sat_mat4_t*, const sat_quad3_t* quad, uint16_t, const uint16_t gouraud[4]) {
    ++g_polygon_calls;
    ++g_gouraud_calls;
    for (int i = 0; i < 4; ++i) {
        g_last_gouraud[i] = gouraud[i];
    }
    if (g_call_count < 32) {
        g_call_x[g_call_count] = (int)sat_fx16_to_int(
            (quad->v[0].x + quad->v[1].x + quad->v[2].x + quad->v[3].x) / 4);
        g_call_is_sprite[g_call_count] = 0;
        ++g_call_count;
    }
    return g_polygon_status;
}

/* Stubs for the render3d submit layer. */
extern "C" sat_result_t sat_draw_world_polygon(
    const sat_mat4_t*, const sat_quad3_t* quad, uint16_t) {
    ++g_polygon_calls;
    if (g_call_count < 32) {
        g_call_x[g_call_count] = (int)sat_fx16_to_int(
            (quad->v[0].x + quad->v[1].x + quad->v[2].x + quad->v[3].x) / 4);
        g_call_is_sprite[g_call_count] = 0;
        ++g_call_count;
    }
    return g_polygon_status;
}

extern "C" sat_result_t sat_draw_world_sprite(
    const sat_mat4_t*, const sat_quad3_t* quad,
    const sat_vdp1_texture_t* texture, uint16_t, uint16_t) {
    ++g_sprite_calls;
    g_last_sprite_tex = texture;
    if (g_call_count < 32) {
        g_call_x[g_call_count] = (int)sat_fx16_to_int(
            (quad->v[0].x + quad->v[1].x + quad->v[2].x + quad->v[3].x) / 4);
        g_call_is_sprite[g_call_count] = 1;
        ++g_call_count;
    }
    return g_sprite_status;
}

static void legacy_null_table_draws_polygons_only() {
    sat_mesh_t mesh = make_mesh();
    build_two_quads(&mesh);
    reset_stubs();
    sat_vec3_t eye = {0, 0, sat_fx16_from_int(100)};
    sat_mesh_draw_t p = base_draw(eye);
    ASSERT_EQ(sat_vdp1_draw_mesh(&mesh, &p), SAT_OK);
    ASSERT_EQ(g_polygon_calls, 2);
    ASSERT_EQ(g_sprite_calls, 0);
}

static void mixed_textured_and_untextured_faces() {
    sat_mesh_t mesh = make_mesh();
    build_two_quads(&mesh);
    sat_vdp1_texture_t tex[1] = {};
    tex[0].valid = 1;
    const uint16_t table[2] = {0u, SAT_MESH_TEXTURE_NONE};
    reset_stubs();
    sat_vec3_t eye = {0, 0, sat_fx16_from_int(100)};
    sat_mesh_draw_t p = base_draw(eye);
    p.textures = tex;
    p.texture_count = 1;
    p.face_texture_indices = table;
    ASSERT_EQ(sat_vdp1_draw_mesh(&mesh, &p), SAT_OK);
    ASSERT_EQ(g_sprite_calls, 1);
    ASSERT_EQ(g_polygon_calls, 1);
    ASSERT_TRUE(g_last_sprite_tex == &tex[0]);
}

static void invalid_texture_index_returns_error_and_draws_nothing() {
    sat_mesh_t mesh = make_mesh();
    build_two_quads(&mesh);
    sat_vdp1_texture_t tex[1] = {};
    const uint16_t table[2] = {0u, 9u};
    reset_stubs();
    sat_vec3_t eye = {0, 0, sat_fx16_from_int(100)};
    sat_mesh_draw_t p = base_draw(eye);
    p.textures = tex;
    p.texture_count = 1;
    p.face_texture_indices = table;
    ASSERT_EQ(sat_vdp1_draw_mesh(&mesh, &p), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(g_sprite_calls, 0);
    ASSERT_EQ(g_polygon_calls, 0);
}

static void culling_parity_between_paths() {
    /* Box viewed down +Z: exactly the +Z face survives culling. */
    sat_mesh_t mesh = make_mesh();
    sat_vec3_t center = {0, 0, 0};
    ASSERT_EQ(sat_mesh_build_box(
        &mesh, &center, sat_fx16_from_int(10), sat_fx16_from_int(10), sat_fx16_from_int(10)), SAT_OK);
    sat_vec3_t eye = {0, 0, sat_fx16_from_int(100)};

    reset_stubs();
    sat_mesh_draw_t p = base_draw(eye);
    p.flags = SAT_MESH_CULL_BACKFACE;
    ASSERT_EQ(sat_vdp1_draw_mesh(&mesh, &p), SAT_OK);
    const int poly_calls = g_polygon_calls;

    sat_vdp1_texture_t tex[1] = {};
    tex[0].valid = 1;
    static uint16_t table[6] = {0u, 0u, 0u, 0u, 0u, 0u};
    reset_stubs();
    sat_mesh_draw_t q = base_draw(eye);
    q.flags = SAT_MESH_CULL_BACKFACE;
    q.textures = tex;
    q.texture_count = 1;
    q.face_texture_indices = table;
    ASSERT_EQ(sat_vdp1_draw_mesh(&mesh, &q), SAT_OK);
    /* Same single face survives; only the submit path differs. */
    ASSERT_EQ(poly_calls, 1);
    ASSERT_EQ(g_sprite_calls, 1);
    ASSERT_EQ(g_polygon_calls, 0);
}

static void sorting_parity_and_order() {
    sat_mesh_t mesh = make_mesh();
    build_two_quads(&mesh);
    /* Eye at x=-100: face 0 (x~8) is nearer than face 1 (x~40), so with
     * farthest-first sorting face 1 must submit first. */
    sat_vec3_t eye = {sat_fx16_from_int(-100), 0, sat_fx16_from_int(50)};

    reset_stubs();
    sat_mesh_draw_t p = base_draw(eye);
    p.flags = SAT_MESH_SORT;
    p.order = g_order;
    p.depth = g_depth;
    ASSERT_EQ(sat_vdp1_draw_mesh(&mesh, &p), SAT_OK);
    ASSERT_EQ(g_call_count, 2);
    ASSERT_EQ(g_call_x[0], 40);
    ASSERT_EQ(g_call_x[1], 8);

    sat_vdp1_texture_t tex[1] = {};
    const uint16_t table[2] = {0u, 0u};
    reset_stubs();
    sat_mesh_draw_t q = base_draw(eye);
    q.flags = SAT_MESH_SORT;
    q.order = g_order;
    q.depth = g_depth;
    q.textures = tex;
    q.texture_count = 1;
    q.face_texture_indices = table;
    ASSERT_EQ(sat_vdp1_draw_mesh(&mesh, &q), SAT_OK);
    ASSERT_EQ(g_call_count, 2);
    ASSERT_EQ(g_call_x[0], 40);
    ASSERT_EQ(g_call_x[1], 8);
    ASSERT_EQ(g_call_is_sprite[0], 1);
    ASSERT_EQ(g_call_is_sprite[1], 1);
}

static void degenerate_triangle_face_draws_textured() {
    sat_mesh_t mesh = make_mesh();
    uint16_t a, b, c;
    sat_mesh_add_vertex(&mesh, 0, 0, 0, &a);
    sat_mesh_add_vertex(&mesh, 0, sat_fx16_from_int(16), 0, &b);
    sat_mesh_add_vertex(&mesh, sat_fx16_from_int(16), 0, 0, &c);
    sat_mesh_add_face(&mesh, a, b, c, c);
    sat_vdp1_texture_t tex[1] = {};
    const uint16_t table[1] = {0u};
    reset_stubs();
    sat_vec3_t eye = {0, 0, sat_fx16_from_int(100)};
    sat_mesh_draw_t p = base_draw(eye);
    p.textures = tex;
    p.texture_count = 1;
    p.face_texture_indices = table;
    ASSERT_EQ(sat_vdp1_draw_mesh(&mesh, &p), SAT_OK);
    ASSERT_EQ(g_sprite_calls, 1);
    ASSERT_EQ(g_polygon_calls, 0);
}

static void capacity_error_propagates_but_draws_rest() {
    sat_mesh_t mesh = make_mesh();
    build_two_quads(&mesh);
    sat_vdp1_texture_t tex[1] = {};
    const uint16_t table[2] = {0u, SAT_MESH_TEXTURE_NONE};
    reset_stubs();
    g_sprite_status = SAT_ERR_CAPACITY;
    sat_vec3_t eye = {0, 0, sat_fx16_from_int(100)};
    sat_mesh_draw_t p = base_draw(eye);
    p.textures = tex;
    p.texture_count = 1;
    p.face_texture_indices = table;
    ASSERT_EQ(sat_vdp1_draw_mesh(&mesh, &p), SAT_ERR_CAPACITY);
    /* The failing face does not cancel the polygon face. */
    ASSERT_EQ(g_sprite_calls, 1);
    ASSERT_EQ(g_polygon_calls, 1);
}

/* A character-sized mesh spans well under one world unit. Whole-unit keys
 * tie every face there and leave index order; the draw must still go
 * farthest-first. */
static void sub_unit_faces_sort_farthest_first() {
    sat_mesh_t mesh = make_mesh();
    const sat_fx16_t t = SAT_FX16_ONE / 16;
    uint16_t a, b, c, d, e, f, g, h;
    sat_mesh_add_vertex(&mesh, 0, t, 0, &a);
    sat_mesh_add_vertex(&mesh, t, t, 0, &b);
    sat_mesh_add_vertex(&mesh, t, 0, 0, &c);
    sat_mesh_add_vertex(&mesh, 0, 0, 0, &d);
    sat_mesh_add_face(&mesh, a, b, c, d);
    sat_mesh_add_vertex(&mesh, 2 * t, t, 0, &e);
    sat_mesh_add_vertex(&mesh, 3 * t, t, 0, &f);
    sat_mesh_add_vertex(&mesh, 3 * t, 0, 0, &g);
    sat_mesh_add_vertex(&mesh, 2 * t, 0, 0, &h);
    sat_mesh_add_face(&mesh, e, f, g, h);
    /* Eye in front on the -X side: face 1 is the farther one. */
    sat_vec3_t eye = {-4 * t, 0, 4 * t};

    reset_stubs();
    sat_mesh_draw_t p = base_draw(eye);
    p.flags = SAT_MESH_SORT;
    p.order = g_order;
    p.depth = g_depth;
    ASSERT_EQ(sat_vdp1_draw_mesh(&mesh, &p), SAT_OK);
    ASSERT_EQ(g_call_count, 2);
    ASSERT_EQ(g_order[0], 1u);
    ASSERT_EQ(g_order[1], 0u);
}

/* With a projection cache, each vertex is projected once per draw, and the
 * submission order matches the per-face path exactly. */
static void screen_cache_projects_each_vertex_once() {
    sat_mesh_t mesh = make_mesh();
    uint16_t a, b, c, d, e, f;
    sat_mesh_add_vertex(&mesh, 0, sat_fx16_from_int(16), 0, &a);
    sat_mesh_add_vertex(&mesh, sat_fx16_from_int(16), sat_fx16_from_int(16), 0, &b);
    sat_mesh_add_vertex(&mesh, sat_fx16_from_int(16), 0, 0, &c);
    sat_mesh_add_vertex(&mesh, 0, 0, 0, &d);
    sat_mesh_add_vertex(&mesh, sat_fx16_from_int(32), sat_fx16_from_int(16), 0, &e);
    sat_mesh_add_vertex(&mesh, sat_fx16_from_int(32), 0, 0, &f);
    sat_mesh_add_face(&mesh, a, b, c, d);
    sat_mesh_add_face(&mesh, b, e, f, c);
    sat_mesh_add_face(&mesh, a, b, c, c); /* triangle: repeated corner */
    sat_vec3_t eye = {sat_fx16_from_int(-100), 0, sat_fx16_from_int(50)};

    reset_stubs();
    sat_mesh_draw_t p = base_draw(eye);
    p.flags = SAT_MESH_SORT;
    p.order = g_order;
    p.depth = g_depth;
    ASSERT_EQ(sat_vdp1_draw_mesh(&mesh, &p), SAT_OK);
    const int legacy_count = g_call_count;
    int legacy_x[32];
    for (int i = 0; i < legacy_count; ++i) {
        legacy_x[i] = g_call_x[i];
    }
    ASSERT_EQ(g_project_calls, 0);

    static sat_projected_vertex_t screen[kVCap];
    reset_stubs();
    p.screen = screen;
    ASSERT_EQ(sat_vdp1_draw_mesh(&mesh, &p), SAT_OK);
    ASSERT_EQ(g_call_count, legacy_count);
    ASSERT_EQ(g_call_count, 3);
    for (int i = 0; i < legacy_count; ++i) {
        ASSERT_EQ(g_call_x[i], legacy_x[i]);
    }
    /* Six distinct vertices across three faces and twelve corners. */
    ASSERT_EQ(g_project_calls, 6);
}

/* A vertex behind the camera rejects every face using it, once. */
static void screen_cache_skips_faces_behind_camera() {
    sat_mesh_t mesh = make_mesh();
    uint16_t a, b, c, d, g;
    sat_mesh_add_vertex(&mesh, 0, sat_fx16_from_int(16), 0, &a);
    sat_mesh_add_vertex(&mesh, sat_fx16_from_int(16), sat_fx16_from_int(16), 0, &b);
    sat_mesh_add_vertex(&mesh, sat_fx16_from_int(16), 0, 0, &c);
    sat_mesh_add_vertex(&mesh, 0, 0, 0, &d);
    sat_mesh_add_vertex(&mesh, 0, sat_fx16_from_int(16), -SAT_FX16_ONE, &g);
    sat_mesh_add_face(&mesh, a, b, c, d);
    sat_mesh_add_face(&mesh, g, b, c, d);
    sat_mesh_add_face(&mesh, g, a, b, c);
    sat_vec3_t eye = {0, 0, sat_fx16_from_int(100)};
    static sat_projected_vertex_t screen[kVCap];

    reset_stubs();
    sat_mesh_draw_t p = base_draw(eye);
    p.screen = screen;
    ASSERT_EQ(sat_vdp1_draw_mesh(&mesh, &p), SAT_OK);
    ASSERT_EQ(g_call_count, 1);
    ASSERT_EQ(g_project_calls, 5);
}

/* Screen-space culling drops exactly what the world-space path drops. */
static void screen_cache_culls_like_world_path() {
    sat_mesh_t mesh = make_mesh();
    build_two_quads(&mesh); /* both facing +Z */
    const uint16_t a = g_indices[0], b = g_indices[1], c = g_indices[2], d = g_indices[3];
    sat_mesh_add_face(&mesh, d, c, b, a); /* the first quad turned to face -Z */
    sat_vec3_t eye = {0, 0, sat_fx16_from_int(100)};

    reset_stubs();
    sat_mesh_draw_t p = base_draw(eye);
    p.flags = SAT_MESH_CULL_BACKFACE;
    ASSERT_EQ(sat_vdp1_draw_mesh(&mesh, &p), SAT_OK);
    const int world_calls = g_call_count;

    static sat_projected_vertex_t screen[kVCap];
    reset_stubs();
    p.screen = screen;
    ASSERT_EQ(sat_vdp1_draw_mesh(&mesh, &p), SAT_OK);
    ASSERT_EQ(g_call_count, world_calls);
    ASSERT_EQ(g_call_count, 2);
}

/* vertex_gouraud hands each face the entries of its own corners, in A..D
 * order, on both the world-space and the screen-space path. */
static void vertex_gouraud_follows_face_corners() {
    sat_mesh_t mesh = make_mesh();
    build_two_quads(&mesh);
    static uint16_t gouraud[8];
    for (int v = 0; v < 8; ++v) {
        gouraud[v] = sat_gouraud_grey(v - 4);
    }
    const uint16_t* last_face = &g_indices[4];
    sat_vec3_t eye = {0, 0, sat_fx16_from_int(100)};

    reset_stubs();
    sat_mesh_draw_t p = base_draw(eye);
    p.vertex_gouraud = gouraud;
    ASSERT_EQ(sat_vdp1_draw_mesh(&mesh, &p), SAT_OK);
    ASSERT_EQ(g_gouraud_calls, 2);
    for (int i = 0; i < 4; ++i) {
        ASSERT_EQ(g_last_gouraud[i], gouraud[last_face[i]]);
    }

    static sat_projected_vertex_t screen[kVCap];
    reset_stubs();
    p.screen = screen;
    ASSERT_EQ(sat_vdp1_draw_mesh(&mesh, &p), SAT_OK);
    ASSERT_EQ(g_gouraud_calls, 2);
    for (int i = 0; i < 4; ++i) {
        ASSERT_EQ(g_last_gouraud[i], gouraud[last_face[i]]);
    }
}

int main() {
    make_identity();
    vertex_gouraud_follows_face_corners();
    screen_cache_projects_each_vertex_once();
    screen_cache_culls_like_world_path();
    screen_cache_skips_faces_behind_camera();
    legacy_null_table_draws_polygons_only();
    mixed_textured_and_untextured_faces();
    invalid_texture_index_returns_error_and_draws_nothing();
    culling_parity_between_paths();
    sorting_parity_and_order();
    sub_unit_faces_sort_farthest_first();
    degenerate_triangle_face_draws_textured();
    capacity_error_propagates_but_draws_rest();
    printf("PASS: test_mesh3d_textured.cpp (12 tests)\n");
    return 0;
}
