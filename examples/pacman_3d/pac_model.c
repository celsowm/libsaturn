#include "pac_model.h"

#include "saturn/mesh3d.h"

#include "actor_render.h"
#include "p3d_config.h"
#include "render_status.h"

/* The mouth is real geometry -- a sector cut out of the sphere, not a dark
 * wedge drawn over one. A wedge laid on top only looks right from directly
 * above, and the camera can be turned until the sphere is seen edge-on,
 * where the cut shows in the silhouette and a decal would be a flat smear
 * across the face.
 *
 * The sector is a lune between two meridians, so the bite is taken out in
 * PLAN view -- the right choice for a camera looking down at a board, and
 * what makes the silhouette read as Pac-Man rather than as a ball. Tipping
 * the mouth to open up-and-down, the way a character in a third-person game
 * would, was tried and looks worse from up here: the upper jaw hides the
 * opening from any camera above it. */
#define PAC_RADIUS 7
/* Resting on the board rather than sunk into it. */
#define PAC_Y PAC_RADIUS

/* 12 bands put a band boundary exactly on each of the four directions an
 * actor can face: 90 degrees is three bands. An even gap centred on that
 * boundary is symmetric about the way Pac-Man is going, which an odd one
 * would not be -- hence gaps of 0, 2 and 4 rather than a smooth count. */
#define PAC_SPHERE_SEGMENTS 12u
#define PAC_SPHERE_RINGS 4u
#define PAC_MESH_VARIANTS 3u
static const uint8_t kMouthGap[PAC_MESH_VARIANTS] = {0u, 2u, 4u};

/* Chew cycle through the variants: closed, half, open, half. */
static const uint8_t kPacChew[4] = {0u, 1u, 2u, 1u};

static sat_vec3_t g_vertices[PAC_MESH_VARIANTS][P3D_ACTOR_VERTEX_CAP];
static uint16_t g_indices[PAC_MESH_VARIANTS][P3D_ACTOR_FACE_CAP * 4u];
static sat_mesh_t g_meshes[PAC_MESH_VARIANTS];
static sat_vec3_t g_normals[PAC_MESH_VARIANTS][P3D_ACTOR_VERTEX_CAP];
/* Per-vertex Gouraud words for each mouth variant in each of the four
 * facings: the light is fixed in the world, so turning Pac-Man turns the
 * light the other way in his own frame. */
static uint16_t g_gouraud[PAC_MESH_VARIANTS][4][P3D_ACTOR_VERTEX_CAP];
/* 0 for the curved skin, 1 for the cut walls inside the mouth. */
static uint16_t g_face_materials[PAC_MESH_VARIANTS][P3D_ACTOR_FACE_CAP];

/* The mouth centred on longitude 0 (+Z); the world matrix turns it to his
 * heading. Band boundaries sit every 30 degrees, so an even gap starting half
 * a gap before longitude 0 is symmetric about it. */
static void build_mesh(sat_mesh_t* mesh, uint16_t gap) {
    sat_vec3_t center = {0, 0, 0};
    const uint16_t start = (uint16_t)(((gap / 2u) > 0u)
        ? (PAC_SPHERE_SEGMENTS - (gap / 2u)) % PAC_SPHERE_SEGMENTS : 0u);

    p3d_note(sat_mesh_build_sphere_wedge(mesh, &center, sat_fx16_from_int(PAC_RADIUS),
        PAC_SPHERE_SEGMENTS, PAC_SPHERE_RINGS, start, gap));
    p3d_mesh_weld(mesh);
}

/* A sphere centred on the origin has its normal along the vertex position.
 * That is exact where sat_mesh_vertex_normals would average the skin at the
 * lips with the flat mouth walls and smear the shading there; the walls
 * themselves do not use these entries, they are drawn flat. */
static void build_normals(const sat_mesh_t* mesh, sat_vec3_t* out) {
    const sat_fx16_t inv = SAT_FX16_ONE / PAC_RADIUS;
    uint16_t v;

    for (v = 0; v < mesh->vertex_count; ++v) {
        out[v].x = sat_fx16_mul(mesh->vertices[v].x, inv);
        out[v].y = sat_fx16_mul(mesh->vertices[v].y, inv);
        out[v].z = sat_fx16_mul(mesh->vertices[v].z, inv);
    }
}

void p3d_pac_init(void) {
    uint16_t i;

    for (i = 0; i < PAC_MESH_VARIANTS; ++i) {
        const uint16_t gap = kMouthGap[i];
        /* sat_mesh_build_sphere_wedge puts the two cut walls last. */
        const uint16_t walls = (uint16_t)((gap > 0u) ? (2u * PAC_SPHERE_RINGS) : 0u);
        uint16_t f;

        p3d_note(sat_mesh_init(&g_meshes[i], g_vertices[i], P3D_ACTOR_VERTEX_CAP,
            g_indices[i], P3D_ACTOR_FACE_CAP));
        build_mesh(&g_meshes[i], gap);
        build_normals(&g_meshes[i], g_normals[i]);
        for (f = 0; f < g_meshes[i].face_count; ++f) {
            g_face_materials[i][f] =
                (f >= (uint16_t)(g_meshes[i].face_count - walls)) ? 1u : 0u;
        }
    }
}

void p3d_pac_relight(const sat_vec3_t* light) {
    int q;
    uint16_t i;

    for (q = 0; q < 4; ++q) {
        const sat_fx16_t s = sat_sin_deg(sat_fx16_from_int(q * 90));
        const sat_fx16_t c = sat_cos_deg(sat_fx16_from_int(q * 90));
        sat_vec3_t local;

        /* The inverse of sat_mat4_rotate_y, applied to the light. */
        local.x = sat_fx16_mul(c, light->x) - sat_fx16_mul(s, light->z);
        local.y = light->y;
        local.z = sat_fx16_mul(s, light->x) + sat_fx16_mul(c, light->z);
        for (i = 0; i < PAC_MESH_VARIANTS; ++i) {
            p3d_actor_gouraud(g_normals[i], g_meshes[i].vertex_count, &local,
                g_gouraud[i][q]);
        }
    }
}

void p3d_pac_draw(sat_scene_t* scene, int x, int z, int dir, uint32_t frame) {
    const uint16_t variant = kPacChew[(frame / 5u) & 3u];
    const int quarter = p3d_facing_quarter(dir);
    sat_scene3d_material_t materials[2];

    p3d_actor_material(&materials[0], P3D_COLOR_PAC, g_gouraud[variant][quarter]);
    p3d_actor_material(&materials[1], P3D_COLOR_MOUTH, 0);
    p3d_actor_submit(scene, &g_meshes[variant], materials, 2u,
        g_face_materials[variant], x, PAC_Y, z, quarter);
}
