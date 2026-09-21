#include "actor_render.h"

#include "saturn/grid.h"
#include "saturn/render3d.h"

#include "render_status.h"

/* The actors are lit by a key light fixed to the CAMERA, up and to the left
 * of the view, rather than by the maze's world light.
 *
 * The world light sits almost behind the default camera, which lights the
 * whole visible side of a sphere evenly -- and an evenly lit sphere is a
 * flat disc. An actor is a handful of pixels across; the only thing that
 * says "round" at that size is a shading gradient with a highlight on one
 * side, so the light is placed where it always produces one. Components,
 * roughly unit length: along the camera's right, world up, and towards the
 * camera on the ground plane. */
#define KEY_RIGHT (-36045) /* -0.55 */
#define KEY_UP 42598       /*  0.65 */
#define KEY_TOWARD 34079   /*  0.52 */
/* Darker floor than the walls': the contrast between the lit and the
 * turned-away side is what reads as round. */
#define ACTOR_AMBIENT ((SAT_FX16_ONE * 3) / 10)
/* Past this much facing the light, a corner is pushed brighter than its
 * base colour -- a specular highlight, which Gouraud can do because its
 * offsets go up as well as down. */
#define HIGHLIGHT_START ((SAT_FX16_ONE * 4) / 5)
#define HIGHLIGHT_GAIN 3

static sat_projected_vertex_t g_screen[P3D_ACTOR_VERTEX_CAP];
static sat_vec3_t g_world[P3D_ACTOR_VERTEX_CAP];

void p3d_actor_material(sat_scene3d_material_t* material, uint16_t color,
                        const uint16_t* gouraud) {
    material->kind = SAT_SCENE3D_RGB;
    material->rgb555 = color;
    material->texture = 0;
    material->tiled = 0;
    material->color_calc_slot = SAT_INDEXED_SOLID_OPAQUE;
    material->vertex_gouraud = gouraud;
}

void p3d_actor_submit(sat_scene_t* scene, const sat_mesh_t* mesh,
                      const sat_scene3d_material_t* materials, uint16_t material_count,
                      const uint16_t* face_materials, int x, int y, int z, int quarter) {
    sat_mat4_t rotation;
    sat_mat4_t translation;
    sat_mat4_t world;
    sat_scene3d_instance_t instance = {};

    p3d_note(sat_mat4_rotate_y(&rotation, sat_fx16_from_int(quarter * 90)));
    p3d_note(sat_mat4_translate(&translation, sat_fx16_from_int(x),
        sat_fx16_from_int(y), sat_fx16_from_int(z)));
    p3d_note(sat_mat4_multiply(&world, &translation, &rotation));

    instance.mesh = mesh;
    instance.materials = materials;
    instance.material_count = material_count;
    instance.face_materials = face_materials;
    instance.world = &world;
    instance.pass = 0u;
    instance.cull_backfaces = 1u;
    p3d_note(sat_scene_submit_instance(scene, &instance,
        SAT_SCENE3D_SLOT_INHERIT, g_screen, g_world));
}

int p3d_facing_quarter(int dir) {
    const int dx = sat_dir_dx(dir);
    const int dz = sat_dir_dy(dir);
    if (dz > 0) {
        return 0; /* +Z */
    }
    if (dx > 0) {
        return 1; /* +X */
    }
    if (dz < 0) {
        return 2; /* -Z */
    }
    return 3; /* -X */
}

void p3d_actor_key_light(const p3d_camera_t* camera, sat_vec3_t* out) {
    /* right = (cos, 0, -sin) and toward = (sin, 0, cos) on the ground. */
    out->x = sat_fx16_mul(KEY_RIGHT, camera->cos_az) + sat_fx16_mul(KEY_TOWARD, camera->sin_az);
    out->y = KEY_UP;
    out->z = sat_fx16_mul(KEY_TOWARD, camera->cos_az) - sat_fx16_mul(KEY_RIGHT, camera->sin_az);
}

void p3d_actor_gouraud(const sat_vec3_t* normals, uint16_t count,
                       const sat_vec3_t* light, uint16_t* out) {
    p3d_note(sat_gouraud_lambert_highlight(normals, count, light, ACTOR_AMBIENT,
        HIGHLIGHT_START, HIGHLIGHT_GAIN, out));
}

/* The sphere builder gives every ring its own copy of the pole and of the
 * seam vertex, which for Pac-Man is 24 of his 65 vertices. The scene
 * projects every vertex of an instance every frame, and on this board a
 * couple of dozen projections is the whole margin between 60 frames a
 * second and 30. Tolerance is a sixteenth of a unit: the pole copies come
 * out of sin/cos at different longitudes, so they are only nearly equal. */
void p3d_mesh_weld(sat_mesh_t* mesh) {
    static uint16_t remap[P3D_ACTOR_VERTEX_CAP];
    p3d_note(sat_mesh_weld_vertices(mesh, SAT_FX16_ONE / 16, remap, P3D_ACTOR_VERTEX_CAP));
}
