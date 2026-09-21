#include "ghost_model.h"

#include "saturn/grid.h"
#include "saturn/mesh3d.h"
#include "saturn/vdp1.h"
#include "saturn/example_util.h"

#include "actor_render.h"
#include "p3d_config.h"
#include "render_status.h"

/* ------------------------------------------------------------------ */
/* Body                                                                */
/* ------------------------------------------------------------------ */
/* The shape the arcade sprite reads as: a dome for a head on a short skirt,
 * flared at the hem, that zig-zags into feet. It is built around a vertical
 * axis, so a ghost never needs turning -- its pupils carry the direction it
 * is going instead, as they do in the original.
 *
 * Eight segments. Twelve gives three feet from the front instead of two, and
 * does not fit: the extra 48 projected vertices a frame drop the example
 * from 60 frames a second to about 40. */
#define GHOST_SEGMENTS 8u
#define GHOST_RADIUS 6
#define GHOST_DOME_Y 7  /* dome centre; also the top of the skirt */
#define GHOST_FOOT_Y 0  /* hem at the tip of a foot */
#define GHOST_NOTCH_Y 3 /* hem between two feet */

static sat_vec3_t g_vertices[P3D_ACTOR_VERTEX_CAP];
static uint16_t g_indices[P3D_ACTOR_FACE_CAP * 4u];
static sat_mesh_t g_mesh;
static sat_vec3_t g_normals[P3D_ACTOR_VERTEX_CAP];
static uint16_t g_gouraud[P3D_ACTOR_VERTEX_CAP];
static uint16_t g_face_materials[P3D_ACTOR_FACE_CAP]; /* all 0: one material */

/* Adds one ring of the ghost at height `y` and radius `r`, with `ny` and
 * `nh` the vertical and horizontal parts of its normal. Returns the index of
 * the ring's first vertex. A `hem` ring alternates between the foot and
 * notch heights instead of using `y`, with the feet on the odd segments:
 * segment 0 faces the default camera, and a foot pointing straight at it
 * drew the whole hem to a single point, like the bottom of an egg. */
static uint16_t add_ring(sat_fx16_t y, sat_fx16_t r, sat_fx16_t ny, sat_fx16_t nh, int hem) {
    const uint16_t first = g_mesh.vertex_count;
    uint16_t j;

    for (j = 0; j < GHOST_SEGMENTS; ++j) {
        const sat_fx16_t phi = sat_fx16_from_int((360 * (int)j) / (int)GHOST_SEGMENTS);
        const sat_fx16_t sx = sat_sin_deg(phi);
        const sat_fx16_t cz = sat_cos_deg(phi);
        const sat_fx16_t vy = !hem ? y : sat_fx16_from_int(
            ((j & 1u) != 0u) ? GHOST_FOOT_Y : GHOST_NOTCH_Y);
        uint16_t v = 0;

        p3d_note(sat_mesh_add_vertex(&g_mesh,
            sat_fx16_mul(r, sx), vy, sat_fx16_mul(r, cz), &v));
        g_normals[v].x = sat_fx16_mul(nh, sx);
        g_normals[v].y = ny;
        g_normals[v].z = sat_fx16_mul(nh, cz);
    }
    return first;
}

/* Pole, a ring at 45 degrees, the equator, and the hem. The windings follow
 * sat_mesh_build_cylinder, whose side and top faces are front-facing from
 * outside and above. No underside: the camera is always above the board. */
static void build_body(void) {
    const sat_fx16_t radius = sat_fx16_from_int(GHOST_RADIUS);
    const sat_fx16_t dome_y = sat_fx16_from_int(GHOST_DOME_Y);
    const sat_fx16_t cos45 = 46341;
    const sat_fx16_t ring45 = sat_fx16_mul(radius, cos45);
    uint16_t pole = 0;
    uint16_t r1;
    uint16_t r2;
    uint16_t r3;
    uint16_t j;

    p3d_note(sat_mesh_init(&g_mesh, g_vertices, P3D_ACTOR_VERTEX_CAP,
        g_indices, P3D_ACTOR_FACE_CAP));
    p3d_note(sat_mesh_add_vertex(&g_mesh, 0, dome_y + radius, 0, &pole));
    g_normals[pole].x = 0;
    g_normals[pole].y = SAT_FX16_ONE;
    g_normals[pole].z = 0;
    r1 = add_ring(dome_y + ring45, ring45, cos45, cos45, 0);
    r2 = add_ring(dome_y, radius, 0, SAT_FX16_ONE, 0);
    /* The hem flares out a unit, so the ghost stands on a skirt wider than
     * its head -- the arcade silhouette, rather than an egg. */
    r3 = add_ring(0, radius + SAT_FX16_ONE, 0, SAT_FX16_ONE, 1);

    for (j = 0; j < GHOST_SEGMENTS; ++j) {
        const uint16_t j1 = (uint16_t)((j + 1u) % GHOST_SEGMENTS);
        p3d_note(sat_mesh_add_face(&g_mesh, pole,
            (uint16_t)(r1 + j1), (uint16_t)(r1 + j), pole));
        p3d_note(sat_mesh_add_face(&g_mesh, (uint16_t)(r1 + j),
            (uint16_t)(r1 + j1), (uint16_t)(r2 + j1), (uint16_t)(r2 + j)));
        p3d_note(sat_mesh_add_face(&g_mesh, (uint16_t)(r2 + j),
            (uint16_t)(r2 + j1), (uint16_t)(r3 + j1), (uint16_t)(r3 + j)));
    }
}

/* ------------------------------------------------------------------ */
/* Face                                                                */
/* ------------------------------------------------------------------ */
/* The face -- both eyes, or the frightened face -- is ONE textured quad
 * rather than a panel per eye and per pupil.
 *
 * As flat polygons it took four quads a ghost, sixteen a frame, and every
 * one of them is four corners through the full projection. That was the
 * difference between this example holding 60 frames a second and dropping
 * to 30: the board is already right at the edge of an SH-2 frame. As a
 * texture the face costs four corners, and gets rounded eyes a flat quad
 * cannot draw.
 *
 * 16x8, a texel to about a screen pixel at this camera distance. Index 0 is
 * transparent; the rest is the palette in upload_faces. */
#define FACE_W 16u
#define FACE_H 8u
#define FACE_PALETTE 2u

enum { FACE_LOOK_LEFT, FACE_LOOK_RIGHT, FACE_LOOK_UP, FACE_LOOK_DOWN,
       FACE_FRIGHT, FACE_FRIGHT_FLASH, FACE_COUNT };

static const char* const kFaceArt[FACE_COUNT][FACE_H] = {
    { /* looking left */
        "..WWWW....WWWW..",
        ".WWWWWW..WWWWWW.",
        ".WWWWWW..WWWWWW.",
        ".PPWWWW..PPWWWW.",
        ".PPWWWW..PPWWWW.",
        ".PPWWWW..PPWWWW.",
        ".WWWWWW..WWWWWW.",
        "..WWWW....WWWW..",
    },
    { /* looking right */
        "..WWWW....WWWW..",
        ".WWWWWW..WWWWWW.",
        ".WWWWWW..WWWWWW.",
        ".WWWWPP..WWWWPP.",
        ".WWWWPP..WWWWPP.",
        ".WWWWPP..WWWWPP.",
        ".WWWWWW..WWWWWW.",
        "..WWWW....WWWW..",
    },
    { /* looking up the screen: away from the camera */
        "..WPPW....WPPW..",
        ".WWPPWW..WWPPWW.",
        ".WWPPWW..WWPPWW.",
        ".WWWWWW..WWWWWW.",
        ".WWWWWW..WWWWWW.",
        ".WWWWWW..WWWWWW.",
        ".WWWWWW..WWWWWW.",
        "..WWWW....WWWW..",
    },
    { /* looking down the screen: towards the camera */
        "..WWWW....WWWW..",
        ".WWWWWW..WWWWWW.",
        ".WWWWWW..WWWWWW.",
        ".WWWWWW..WWWWWW.",
        ".WWPPWW..WWPPWW.",
        ".WWPPWW..WWPPWW.",
        ".WWPPWW..WWPPWW.",
        "..WWWW....WWWW..",
    },
    { /* frightened: two dots and a wobbly mouth */
        "................",
        "....FF....FF....",
        "....FF....FF....",
        "................",
        "................",
        "..FF..FF..FF..F.",
        ".F..FF..FF..FF..",
        "................",
    },
    { /* frightened, flashing white: the same face in red */
        "................",
        "....RR....RR....",
        "....RR....RR....",
        "................",
        "................",
        "..RR..RR..RR..R.",
        ".R..RR..RR..RR..",
        "................",
    },
};

static sat_vdp1_texture_t g_face_textures[FACE_COUNT];

static void upload_faces(void) {
    static uint16_t palette[256];
    static uint8_t pixels[FACE_W * FACE_H];
    uint16_t f;

    palette[1] = SAT_RGB555(31, 31, 31); /* W: eye white */
    palette[2] = SAT_RGB555(3, 6, 24);   /* P: pupil */
    palette[3] = SAT_RGB555(31, 24, 18); /* F: frightened face */
    palette[4] = SAT_RGB555(31, 4, 4);   /* R: flashing face */
    sat_example_must(sat_palette_upload_indexed8(palette, FACE_PALETTE));

    for (f = 0; f < FACE_COUNT; ++f) {
        uint16_t y;
        for (y = 0; y < FACE_H; ++y) {
            uint16_t x;
            for (x = 0; x < FACE_W; ++x) {
                const char c = kFaceArt[f][y][x];
                pixels[(y * FACE_W) + x] =
                    (c == 'W') ? 1u : (c == 'P') ? 2u : (c == 'F') ? 3u : (c == 'R') ? 4u : 0u;
            }
        }
        sat_example_must(sat_tex_upload_indexed8_pixels(
            &g_face_textures[f], pixels, FACE_W, FACE_H, FACE_PALETTE));
    }
}

/* Which texture. The face is always drawn on the side towards the camera,
 * so the heading is put in screen terms: along the camera's right, and up
 * the screen for away from it. */
static uint16_t face_for(const p3d_camera_t* camera, int dir, p3d_ghost_mood_t mood) {
    const sat_fx16_t dir_x = sat_fx16_from_int(sat_dir_dx(dir));
    const sat_fx16_t dir_z = sat_fx16_from_int(sat_dir_dy(dir));
    const sat_fx16_t across =
        sat_fx16_mul(dir_x, camera->cos_az) - sat_fx16_mul(dir_z, camera->sin_az);
    const sat_fx16_t toward =
        sat_fx16_mul(dir_x, camera->sin_az) + sat_fx16_mul(dir_z, camera->cos_az);

    if (mood == P3D_GHOST_FRIGHTENED) {
        return FACE_FRIGHT;
    }
    if (mood == P3D_GHOST_FLASHING) {
        return FACE_FRIGHT_FLASH;
    }
    if (sat_fx16_abs(across) >= sat_fx16_abs(toward)) {
        return (across < 0) ? FACE_LOOK_LEFT : FACE_LOOK_RIGHT;
    }
    return (toward > 0) ? FACE_LOOK_DOWN : FACE_LOOK_UP;
}

/* The face quad, turned to face the camera.
 *
 * It is tilted back by the camera's elevation so it is seen square-on: a
 * vertical panel seen from 54 degrees up is foreshortened to about half its
 * height, which at eight texels is the difference between an eye and a
 * line. It sits on the point of the dome 30 degrees above the equator on the
 * camera's side -- the middle of the face as seen from up here -- and a unit
 * and a half proud of it, so that it sorts in front of every dome face under
 * it. */
static void draw_face(sat_scene_t* scene, const p3d_camera_t* camera,
                      int x, int z, uint16_t face) {
    /* Towards the camera on the ground plane, and its right perpendicular. */
    const sat_fx16_t toward_x = camera->sin_az;
    const sat_fx16_t toward_z = camera->cos_az;
    const sat_fx16_t right_x = camera->cos_az;
    const sat_fx16_t right_z = -camera->sin_az;
    /* Screen-up, as (along toward, along Y). */
    const sat_fx16_t up_h = -P3D_CAM_ELEV_SIN;
    const sat_fx16_t up_y = P3D_CAM_ELEV_COS;
    const sat_fx16_t out = (SAT_FX16_ONE * 3) / 2;
    const sat_fx16_t face_r = sat_fx16_from_int(GHOST_RADIUS);
    const sat_fx16_t along = sat_fx16_mul(face_r, 56756) /* cos 30 */
                             + sat_fx16_mul(out, P3D_CAM_ELEV_COS);
    const sat_fx16_t cx = sat_fx16_from_int(x) + sat_fx16_mul(toward_x, along);
    const sat_fx16_t cz = sat_fx16_from_int(z) + sat_fx16_mul(toward_z, along);
    const sat_fx16_t cy = sat_fx16_from_int(GHOST_DOME_Y) + (face_r / 2)
                          + sat_fx16_mul(out, P3D_CAM_ELEV_SIN);
    /* Half-extents, in world units: 16x8 texels at about 1.4 pixels a
     * unit. */
    const sat_fx16_t half_w = (SAT_FX16_ONE * 11) / 2;
    const sat_fx16_t half_h = (SAT_FX16_ONE * 11) / 4;
    const sat_fx16_t wx = sat_fx16_mul(right_x, half_w);
    const sat_fx16_t wz = sat_fx16_mul(right_z, half_w);
    const sat_fx16_t hh = sat_fx16_mul(up_h, half_h);
    const sat_fx16_t hx = sat_fx16_mul(toward_x, hh);
    const sat_fx16_t hz = sat_fx16_mul(toward_z, hh);
    const sat_fx16_t hy = sat_fx16_mul(up_y, half_h);
    sat_quad3_t quad;
    sat_scene3d_material_t material = {};

    /* Top-left, top-right, bottom-right, bottom-left: the texture's own
     * corner order. */
    quad.v[0].x = cx - wx + hx; quad.v[0].y = cy + hy; quad.v[0].z = cz - wz + hz;
    quad.v[1].x = cx + wx + hx; quad.v[1].y = cy + hy; quad.v[1].z = cz + wz + hz;
    quad.v[2].x = cx + wx - hx; quad.v[2].y = cy - hy; quad.v[2].z = cz + wz - hz;
    quad.v[3].x = cx - wx - hx; quad.v[3].y = cy - hy; quad.v[3].z = cz - wz - hz;
    material.kind = SAT_SCENE3D_INDEXED_TEXTURED;
    material.texture = &g_face_textures[face];
    material.color_calc_slot = SAT_INDEXED_SOLID_OPAQUE;
    p3d_note(sat_scene_submit_quad(scene, &quad, &material, 0u));
}

/* ------------------------------------------------------------------ */
/* Public                                                              */
/* ------------------------------------------------------------------ */

void p3d_ghost_init(void) {
    build_body();
    upload_faces();
}

void p3d_ghost_relight(const sat_vec3_t* light) {
    p3d_actor_gouraud(g_normals, g_mesh.vertex_count, light, g_gouraud);
}

void p3d_ghost_draw(sat_scene_t* scene, const p3d_camera_t* camera,
                    int x, int z, int dir, uint16_t color, p3d_ghost_mood_t mood) {
    sat_scene3d_material_t material;

    p3d_actor_material(&material, color, g_gouraud);
    p3d_actor_submit(scene, &g_mesh, &material, 1u, g_face_materials, x, 0, z, 0);
    draw_face(scene, camera, x, z, face_for(camera, dir, mood));
}
