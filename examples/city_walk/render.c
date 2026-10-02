/* city_walk renderer: plan the frame, decode each chosen blob, submit.
 *
 * The VDP1 painter has no Z-buffer, so three things keep it honest:
 *  - a hard face budget, enforced by city_plan_frame (LOD0 -> LOD1 -> drop),
 *  - one painter pass per chunk, nearest highest (city_plan_frame assigns it),
 *  - the geometry is decoded straight into player-frame coordinates, so the
 *    16.16 math never sees a coordinate near the 181-unit sign flip.
 *
 * Two SH-2s share the projection work. The Master decodes and submits the near
 * LOD0 cells one at a time through ONE scratch mesh (sat_scene_submit_instance
 * copies what it needs and keeps no pointers). The Slave prepares the outer
 * rings, LOD1 and LOD2, as a batch; because a batch keeps its items until it is
 * merged, each one owns a decoded mesh, which is why the Slave takes a bounded
 * number of cells. Whatever it cannot take, or fails to deliver, the Master
 * does itself, so a frame is never short of faces.
 *
 * Facades. A face whose texture index names one of its slot's textures draws
 * as an INDEX8 distorted sprite of the texels the residency copied into VDP1
 * VRAM; the rest keep their solid pool colour. Each item gets its own material
 * table (one entry per face), and the texture descriptors come from a pool
 * that lives for one frame: the painter keeps pointers to them until the
 * flush at the end of render_frame, and not a moment longer. A textured face
 * that crosses the near plane cannot be clipped as a sprite; its material
 * names the solid colour the scene draws instead (clipped), so the wall at the
 * player's shoulder never becomes a hole.
 */
#include <stdint.h>

#include "example_util.h"
#include "saturn/fade3d.h"
#include "saturn/parallel.h"
#include "saturn/render3d.h"
#include "saturn/scene3d.h"

#include "city_walk.h"

#define CITY_SCENE_FACES 640u /* CITY_FACE_CAP plus room for the near-plane splits */
#define CITY_FOV_DEG 55
#define CITY_FAR_UNITS 140

#define SLAVE_ITEMS 10u
#define SLAVE_VERT_CAP CITY_LOD1_VERTS
#define SLAVE_FACE_CAP CITY_LOD1_FACES
#define PARALLEL_TIMEOUT 60000u

typedef struct slave_arena {
    sat_vec3_t verts[SLAVE_VERT_CAP];
    uint16_t indices[SLAVE_FACE_CAP * 4u];
    uint16_t face_materials[SLAVE_FACE_CAP];
    uint8_t face_textures[SLAVE_FACE_CAP];
    uint8_t double_sided_faces[SLAVE_FACE_CAP];
    sat_scene3d_material_t materials[SLAVE_FACE_CAP];
    sat_projected_vertex_t screen[SLAVE_VERT_CAP];
    sat_mesh_t mesh;
    sat_scene3d_instance_t instance;
} slave_arena_t;

static sat_scene3d_face_t g_faces[CITY_SCENE_FACES] CITY_WRAM_L;
static uint32_t g_keys[CITY_SCENE_FACES] CITY_WRAM_L;
static uint16_t g_order[CITY_SCENE_FACES] CITY_WRAM_L;
static sat_vec3_t g_verts[CITY_LOD0_VERTS] CITY_WRAM_L;
static uint16_t g_indices[CITY_LOD0_FACES * 4u] CITY_WRAM_L;
static uint16_t g_face_materials[CITY_LOD0_FACES] CITY_WRAM_L;
static uint8_t g_face_textures[CITY_LOD0_FACES] CITY_WRAM_L;
static uint8_t g_double_sided_faces[CITY_LOD0_FACES] CITY_WRAM_L;
static sat_scene3d_material_t g_item_materials[CITY_LOD0_FACES] CITY_WRAM_L;
/* Texture descriptors for one frame: at most one per submitted face. */
static sat_vdp1_texture_t g_tex_desc[CITY_SCENE_FACES] CITY_WRAM_L;
static uint16_t g_tex_desc_used;
static uint32_t g_frame_textured;
static sat_projected_vertex_t g_screen[CITY_LOD0_VERTS] CITY_WRAM_L;
static sat_mesh_t g_mesh;
static slave_arena_t g_arena[SLAVE_ITEMS] CITY_WRAM_L;
static sat_scene3d_prepare_item_t g_items[SLAVE_ITEMS] CITY_WRAM_L;
static sat_scene3d_face_t g_slave_faces[SLAVE_ITEMS * SLAVE_FACE_CAP] CITY_WRAM_L;
static uint32_t g_slave_keys[SLAVE_ITEMS * SLAVE_FACE_CAP] CITY_WRAM_L;
static sat_scene3d_prepare_batch_t g_batch CITY_WRAM_L;
static sat_parallel_handle_t g_handle;
static sat_scene_t g_scene;
static sat_camera3d_t g_camera;
static city_plan_t g_plan;

/* Distance fade. The far rings dissolve into the sky and ground behind them,
 * instead of appearing whole at the edge of the paged area: eight VDP2
 * colour-calculation slots from opaque (up to FADE_START units) to fully
 * see-through (FADE_END and beyond). One state byte per chunk lets
 * sat_fade3d_slot keep a cell on its slot until the player has crossed the
 * transition band, so a cell at a boundary does not shimmer. */
#define FADE_START_UNITS 64
#define FADE_END_UNITS 128
static const sat_fade3d_slots_t g_fade = {
    {FADE_START_UNITS * 65536, FADE_END_UNITS * 65536, 8u, 0u, 0u}, 2 * 65536, 0u, 1u, 1u, 0u
};
static uint8_t g_fade_state[CITY_CHUNK_COUNT];

static uint8_t fade_slot(const city_draw_item_t* item) {
    uint8_t slot = SAT_INDEXED_SOLID_OPAQUE;
    uint8_t* state = &g_fade_state[city_chunk_index(item->chunk_x, item->chunk_z)];
    if (sat_fade3d_slot(&g_fade, (sat_fx16_t)item->distance * 65536, state, &slot) != SAT_OK) {
        return SAT_INDEXED_SOLID_OPAQUE;
    }
    if (slot != SAT_INDEXED_SOLID_OPAQUE) ++g_city.faded_cells;
    return slot;
}

/* Per-frame state the frustum test reads. */
static struct {
    sat_fx16_t eye_x, eye_z, fwd_x, fwd_z, right_x, right_z;
    int32_t pcx, pcz;
} g_cull;

uint32_t render_wram_bytes(void) {
    return (uint32_t)(sizeof(g_faces) + sizeof(g_keys) + sizeof(g_order) + sizeof(g_verts) +
                      sizeof(g_indices) + sizeof(g_face_materials) + sizeof(g_screen) +
                      sizeof(g_face_textures) + sizeof(g_double_sided_faces) +
                      sizeof(g_item_materials) + sizeof(g_tex_desc) +
                      sizeof(g_arena) + sizeof(g_items) + sizeof(g_slave_faces) +
                      sizeof(g_slave_keys));
}

void render_init(void) {
    const sat_parallel_config_t parallel = {SAT_PARALLEL_AUTO, 0, 0u, 0u, PARALLEL_TIMEOUT};
    const sat_vec3_t eye = {0, 0, 0};
    const sat_vec3_t target = {0, 0, SAT_FX16_ONE};
    const sat_vec3_t up = {0, SAT_FX16_ONE, 0};
    sat_example_must(sat_camera3d_init(
        &g_camera, &eye, &target, &up, sat_fx16_from_int(CITY_FOV_DEG),
        (sat_fx16_t)(((int64_t)CITY_W << 16) / CITY_H), SAT_FX16_ONE / 2,
        sat_fx16_from_int(CITY_FAR_UNITS)));
    sat_example_must(sat_scene_init(&g_scene, g_faces, g_keys, g_order, CITY_SCENE_FACES));
    g_mesh.vertices = g_verts;
    g_mesh.indices = g_indices;
    g_mesh.vertex_cap = CITY_LOD0_VERTS;
    g_mesh.face_cap = CITY_LOD0_FACES;
    g_mesh.vertex_count = 0u;
    g_mesh.face_count = 0u;
    for (uint32_t i = 0u; i < CITY_CHUNK_COUNT; ++i) g_fade_state[i] = SAT_INDEXED_SOLID_OPAQUE;
    /* Without the Slave every batch runs on the Master; the frame is correct
     * either way. The backend is recorded so a test can tell which one ran. */
    (void)sat_scene3d_prepare_parallel_register();
    (void)sat_parallel_init(&parallel);
    g_city.parallel_backend = sat_parallel_slave_available() ? 1u : 0u;
}

/* Waits for the Slave task; a task still running after the timeout is stopped
 * before its buffers are reused. */
static sat_result_t finish_task(sat_parallel_handle_t handle) {
    sat_result_t st = sat_parallel_wait(handle, PARALLEL_TIMEOUT);
    if (st == SAT_OK) st = sat_parallel_result(handle, 0);
    if (sat_parallel_state(handle) == SAT_PARALLEL_RUNNING) {
        (void)sat_parallel_abort(handle, PARALLEL_TIMEOUT);
        st = SAT_ERR_TIMEOUT;
    }
    return st;
}

/* Rejects a cell whose XZ footprint lies wholly behind the eye or wholly
 * outside one side of the view wedge. The half-width slope is 0.78, a little
 * wider than the true 0.744 (55 degrees vertical at 320:224), so a rounding
 * error can only keep a cell, never lose one. Height never rejects: the
 * buildings are tall and the eye is low, so the vertical test would almost
 * never fire and would cost the same. */
static int visible(void* context, int32_t cx, int32_t cz, uint8_t lod, uint16_t slot) {
    uint16_t bytes;
    const uint8_t* blob = residency_slot_blob(slot, &bytes);
    const sat_fx16_t slope = 51118; /* 0.78 in 16.16 */
    sat_fx16_t ox = city_origin_rel_fx(cx, g_cull.pcx);
    sat_fx16_t oz = city_origin_rel_fx(cz, g_cull.pcz);
    int32_t x_lo = ox + ((int32_t)city_bes16(blob + 0x0C) << CITY_QUANT_SHIFT);
    int32_t x_hi = ox + ((int32_t)city_bes16(blob + 0x12) << CITY_QUANT_SHIFT);
    int32_t z_lo = oz + ((int32_t)city_bes16(blob + 0x10) << CITY_QUANT_SHIFT);
    int32_t z_hi = oz + ((int32_t)city_bes16(blob + 0x16) << CITY_QUANT_SHIFT);
    int all_behind = 1, all_left = 1, all_right = 1;
    (void)context;
    (void)lod;
    (void)bytes;
    for (int corner = 0; corner < 4; ++corner) {
        sat_fx16_t dx = (corner & 1 ? x_hi : x_lo) - g_cull.eye_x;
        sat_fx16_t dz = (corner & 2 ? z_hi : z_lo) - g_cull.eye_z;
        sat_fx16_t depth = sat_fx16_mul(dx, g_cull.fwd_x) + sat_fx16_mul(dz, g_cull.fwd_z);
        sat_fx16_t side = sat_fx16_mul(dx, g_cull.right_x) + sat_fx16_mul(dz, g_cull.right_z);
        sat_fx16_t reach = sat_fx16_mul(depth, slope);
        if (depth > 0) all_behind = 0;
        if (side >= -reach) all_left = 0;
        if (side <= reach) all_right = 0;
    }
    return !(all_behind || all_left || all_right);
}

/* Decodes one plan item into `mesh`; returns SAT_OK or the decode error. */
/* Counted, and the first one kept with where it happened: site 1 = Slave
 * arena, 2 = Master scratch; bits 8..15 of the site half carry the LOD. */
static void note_decode_failure(uint32_t site, sat_result_t st, const city_draw_item_t* item) {
    if (g_city.decode_failures++ == 0u) {
        g_city.decode_first_error =
            ((site | ((uint32_t)item->lod << 8)) << 16) | (uint16_t)(int16_t)st;
    }
}

/* Decodes one plan item into `mesh` and builds its per-face material table
 * in `table` (face_materials then index it one to one). Returns SAT_OK or the
 * decode error. */
static void billboard_face(sat_mesh_t* mesh, uint16_t face, sat_fx16_t right_x,
                           sat_fx16_t right_z) {
    uint16_t* idx = mesh->indices + (uint32_t)face * 4u;
    sat_vec3_t* a = &mesh->vertices[idx[0]];
    sat_vec3_t* b = &mesh->vertices[idx[1]];
    sat_vec3_t* c = &mesh->vertices[idx[2]];
    sat_vec3_t* d = &mesh->vertices[idx[3]];
    const sat_fx16_t cx = (sat_fx16_t)(((int64_t)a->x + b->x + c->x + d->x) / 4);
    const sat_fx16_t cz = (sat_fx16_t)(((int64_t)a->z + b->z + c->z + d->z) / 4);
    sat_fx16_t width = b->x - a->x;
    if (width < 0) width = -width;
    if (width == 0) {
        width = b->z - a->z;
        if (width < 0) width = -width;
    }
    const sat_fx16_t half = width / 2;
    const sat_fx16_t hx = sat_fx16_mul(right_x, half);
    const sat_fx16_t hz = sat_fx16_mul(right_z, half);
    const sat_fx16_t top_a = a->y, top_b = b->y;
    const sat_fx16_t bottom_c = c->y, bottom_d = d->y;
    a->x = cx - hx; a->z = cz - hz; a->y = top_a;
    b->x = cx + hx; b->z = cz + hz; b->y = top_b;
    c->x = cx + hx; c->z = cz + hz; c->y = bottom_c;
    d->x = cx - hx; d->z = cz - hz; d->y = bottom_d;
}

static sat_result_t decode_item(const city_draw_item_t* item, const city_view_t* view,
                                sat_mesh_t* mesh, uint16_t* face_materials,
                                uint8_t* face_textures, uint8_t* double_sided_faces,
                                sat_scene3d_material_t* table, uint16_t face_cap) {
    uint16_t bytes, pool_count;
    const uint8_t* blob = residency_slot_blob(item->slot, &bytes);
    const sat_scene3d_material_t* pool = materials_table(&pool_count);
    const city_slot_texture_t* textures;
    uint32_t vram;
    const uint16_t texture_count = residency_slot_textures(item->slot, &textures, &vram);
    /* Texture bytes beyond what the slot holds (a block that failed to load)
     * are accepted here and drawn solid below. */
    sat_result_t st = city_blob_decode_ex(
        blob, bytes, city_origin_rel_fx(item->chunk_x, view->pos.chunk_x),
        city_origin_rel_fx(item->chunk_z, view->pos.chunk_z), g_archive.header.world_min_y_fx,
        g_archive.header.material_count, g_material_map, mesh, face_materials, face_cap,
        255u, face_textures);
    if (st != SAT_OK) return st;
    for (uint16_t f = 0u; f < mesh->face_count; ++f) double_sided_faces[f] = 0u;
    const sat_fx16_t right_x = -sat_cos_deg(view->yaw);
    const sat_fx16_t right_z = sat_sin_deg(view->yaw);
    for (uint16_t f = 0u; f < mesh->face_count; ++f) {
        const uint16_t handle = face_materials[f];
        const uint8_t tex = face_textures[f];
        if (tex != 0u && tex <= texture_count && g_tex_desc_used < CITY_SCENE_FACES) {
            const city_slot_texture_t* t = &textures[tex - 1u];
            sat_vdp1_texture_t* d = &g_tex_desc[g_tex_desc_used++];
            d->srca = (uint16_t)(vram / 8u + t->offset8);
            d->width = (uint16_t)(t->width8 * 8u);
            d->height = t->height;
            d->palette = CITY_PALETTE_TEXTURE;
            d->valid = 1u;
            d->format = SAT_VDP1_TEXTURE_INDEXED8;
            table[f].kind = SAT_SCENE3D_INDEXED_TEXTURED;
            table[f].rgb555 = (t->flags & CITY_TEXTURE_FLAG_CUTOUT) != 0u
                                  ? 0u : materials_rgb(handle);
            table[f].texture = d;
            table[f].tiled = 0;
            table[f].color_calc_slot = SAT_INDEXED_SOLID_OPAQUE;
            table[f].vertex_gouraud = 0;
            if ((t->flags & CITY_TEXTURE_FLAG_BILLBOARD) != 0u) {
                billboard_face(mesh, f, right_x, right_z);
                double_sided_faces[f] = 1u;
            } else if ((t->flags & CITY_TEXTURE_FLAG_CUTOUT) != 0u) {
                double_sided_faces[f] = 1u;
            }
            ++g_frame_textured;
        } else {
            table[f] = pool[handle < pool_count ? handle : 0u];
        }
        face_materials[f] = f;
    }
    return SAT_OK;
}

void render_frame(const city_view_t* view) {
    const sat_fx16_t yaw = view->yaw;
    const sat_fx16_t s = sat_sin_deg(yaw);
    const sat_fx16_t c = sat_cos_deg(yaw);
    sat_scene_stats_t stats;
    uint16_t material_count;
    uint16_t t_mark;
    uint16_t slave_count = 0u;
    uint8_t on_slave[CITY_MAX_DRAW_ITEMS];
    int slave_pending = 0;
    (void)materials_table(&material_count);
    g_tex_desc_used = 0u;
    g_frame_textured = 0u;

    /* Player-chunk frame: the eye's XZ is just its local position. Looking
     * along +Z at yaw 0, forward = (sin, 0, cos), and screen-right in this
     * right-handed frame is forward x up = (-cos, 0, sin). */
    g_cull.eye_x = view->pos.local_x;
    g_cull.eye_z = view->pos.local_z;
    g_cull.fwd_x = s;
    g_cull.fwd_z = c;
    g_cull.right_x = -c;
    g_cull.right_z = s;
    g_cull.pcx = view->pos.chunk_x;
    g_cull.pcz = view->pos.chunk_z;

    g_camera.eye.x = view->pos.local_x;
    g_camera.eye.y = view->eye_y;
    g_camera.eye.z = view->pos.local_z;
    g_camera.target.x = g_camera.eye.x + s * 64;
    g_camera.target.y = g_camera.eye.y;
    g_camera.target.z = g_camera.eye.z + c * 64;
    sat_example_must(sat_camera3d_update(&g_camera));

    sat_example_must(sat_scene_begin(&g_scene, &g_camera, SAT_FX16_ONE / 2, CITY_W, CITY_H,
                                     CITY_HUD_COMMANDS));
    t_mark = city_frt_now();
    city_plan_frame(&g_res, &view->pos, CITY_FACE_CAP, visible, 0, &g_plan);
    g_city.t_plan += (uint16_t)(city_frt_now() - t_mark);
    g_city.pop_in_events += g_plan.pop_in;

    /* Slave share: the FARTHEST cells, up to half of this frame's blob faces so
     * that neither CPU waits for the other. Handing it a fixed count of outer
     * cells left the Master idle for ~15 ms a frame once the far-to-near budget
     * had shrunk its own share. Each is decoded into its own arena, then the
     * batch starts and runs while the Master works on the rest. */
    {
        const uint32_t slave_target = g_plan.faces / 2u;
        uint32_t slave_faces = 0u;
        for (uint16_t i = 0u; i < g_plan.count; ++i) on_slave[i] = 0u;
        for (int32_t i = (int32_t)g_plan.count - 1; i >= 0; --i) {
            const city_draw_item_t* item = &g_plan.items[i];
            slave_arena_t* a;
            if (item->lod < 1u || slave_count >= SLAVE_ITEMS || !g_city.parallel_backend) continue;
            if (slave_faces + item->faces > slave_target) continue;
            a = &g_arena[slave_count];
            a->mesh.vertices = a->verts;
            a->mesh.indices = a->indices;
            a->mesh.vertex_cap = SLAVE_VERT_CAP;
            a->mesh.face_cap = SLAVE_FACE_CAP;
            a->mesh.vertex_count = a->mesh.face_count = 0u;
            t_mark = city_frt_now();
            sat_result_t st = decode_item(item, view, &a->mesh, a->face_materials,
                                          a->face_textures, a->double_sided_faces,
                                          a->materials, SLAVE_FACE_CAP);
            if (st != SAT_OK) {
                note_decode_failure(1u, st, item);
                on_slave[i] = 1u; /* dropped: neither CPU draws it */
                continue;
            }
            g_city.t_decode += (uint16_t)(city_frt_now() - t_mark);
            /* sat_scene3d_instance_t has optional pointer fields. Reset the
             * reused descriptor before filling the required ones so adding a
             * new optional field cannot turn stale WRAM into a live pointer. */
            a->instance = (sat_scene3d_instance_t){0};
            a->instance.mesh = &a->mesh;
            a->instance.materials = a->materials;
            a->instance.material_count = a->mesh.face_count;
            a->instance.face_materials = a->face_materials;
            a->instance.world = 0;
            a->instance.pass = item->pass;
            a->instance.cull_backfaces = 1u;
            a->instance.double_sided_faces = a->double_sided_faces;
            g_items[slave_count].instance = &a->instance;
            g_items[slave_count].screen_scratch = a->screen;
            g_items[slave_count].world_scratch = 0;
            g_items[slave_count].color_calc_slot = fade_slot(item);
            g_items[slave_count].reserved = 0u;
            on_slave[i] = 1u;
            slave_faces += item->faces;
            ++slave_count;
        }
    }
    if (slave_count > 0u &&
        sat_scene3d_prepare_batch_init(&g_batch, g_items, slave_count, g_slave_faces, g_slave_keys,
                                       SLAVE_ITEMS * SLAVE_FACE_CAP) == SAT_OK) {
        g_batch.dispatch = SAT_SCENE3D_PREPARE_DISPATCH_RUNTIME;
        slave_pending = sat_scene_prepare_batch_async(&g_scene, &g_batch, &g_handle) == SAT_OK;
    }

    /* Master share: everything the Slave did not take. */
    for (uint16_t i = 0u; i < g_plan.count; ++i) {
        /* Zero-init is required: optional fields such as bounds,
         * double_sided_faces and validated_binding must default to NULL. */
        sat_scene3d_instance_t instance = {0};
        if (on_slave[i]) continue;
        t_mark = city_frt_now();
        sat_result_t st = decode_item(&g_plan.items[i], view, &g_mesh, g_face_materials,
                                      g_face_textures, g_double_sided_faces,
                                      g_item_materials, CITY_LOD0_FACES);
        if (st != SAT_OK) {
            note_decode_failure(2u, st, &g_plan.items[i]);
            continue;
        }
        g_city.t_decode += (uint16_t)(city_frt_now() - t_mark);
        instance.mesh = &g_mesh;
        instance.materials = g_item_materials;
        instance.material_count = g_mesh.face_count;
        instance.face_materials = g_face_materials;
        instance.world = 0;
        instance.pass = g_plan.items[i].pass;
        instance.cull_backfaces = 1u;
        instance.double_sided_faces = g_double_sided_faces;
        t_mark = city_frt_now();
        (void)sat_scene_submit_instance(&g_scene, &instance, fade_slot(&g_plan.items[i]), g_screen, 0);
        g_city.t_submit += (uint16_t)(city_frt_now() - t_mark);
    }

    /* Slave result, or the Master doing the same work if it did not deliver. */
    if (slave_count > 0u) {
        int merged = 0;
        t_mark = city_frt_now();
        if (slave_pending) {
            if (finish_task(g_handle) == SAT_OK &&
                sat_scene_merge_prepared_batch(&g_scene, &g_batch, g_handle) == SAT_OK) {
                merged = 1;
            }
            (void)sat_scene_prepare_batch_release(&g_scene, &g_batch, g_handle);
        }
        g_city.t_wait += (uint16_t)(city_frt_now() - t_mark);
        ++g_city.slave_batches;
        g_city.slave_items += slave_count;
        if (!merged) {
            ++g_city.slave_fallbacks;
            for (uint16_t k = 0u; k < slave_count; ++k) {
                (void)sat_scene_submit_instance(&g_scene, &g_arena[k].instance,
                                                g_items[k].color_calc_slot, g_arena[k].screen, 0);
            }
        }
    }

    if (g_frame_textured > g_city.max_textured_faces) g_city.max_textured_faces = g_frame_textured;
    t_mark = city_frt_now();
    (void)sat_scene_flush(&g_scene);
    g_city.t_flush += (uint16_t)(city_frt_now() - t_mark);

    if (sat_scene_stats(&g_scene, &stats) == SAT_OK) {
        g_city.last_submitted = stats.submitted_faces;
        g_city.last_flushed = stats.flushed_faces;
        g_city.last_culled = stats.culled_faces;
        g_city.last_skipped = stats.skipped_faces;
        g_city.last_rejected = stats.rejected_faces;
        g_city.last_budget_blocked = stats.budget_blocked_faces;
        if (stats.world_commands > g_city.max_vdp1_commands) g_city.max_vdp1_commands = stats.world_commands;
        if (stats.submitted_faces > g_city.max_world_faces) g_city.max_world_faces = stats.submitted_faces;
        if (stats.result != SAT_OK && g_city.scene_first_error == 0u) {
            g_city.scene_first_error = (uint32_t)(-stats.result);
        }
    }
}
