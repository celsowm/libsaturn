/* city_walk player: first-person walking. D-pad UP/DOWN walk, LEFT/RIGHT turn,
 * L/R strafe, B held = run.
 *
 * Speeds are chosen for the demo, deliberately: at a plausible 1.4 m/s and ~2.5 m
 * per world unit a walker would cover 0.6 units a second and never leave the
 * first chunk in a whole test run, so nothing would ever page. 8 u/s walks a
 * 32-unit chunk in 4 s; 24 u/s runs one in 1.3 s and can outrun the page-in
 * queue, which the renderer tolerates by falling back to a coarser LOD.
 *
 * Movement is scaled by the VBlanks actually elapsed, so it stays the same
 * speed when the frame rate drops.
 */
#include <stdint.h>

#include "saturn/video.h"

#include "city_walk.h"

#define WALK_UNITS_PER_SECOND 8
#define RUN_UNITS_PER_SECOND 24
#define TURN_DEGREES_PER_SECOND 100
#define EYE_HEIGHT_FX (2 * 65536) /* the RBG0 ground is calibrated to this eye height */
#define MAX_DT_VBLANKS 4u         /* a hitch must not teleport the player */

/* Street viewpoints, picked from the archive itself (tools: open ground at least
 * nine units from every building footprint, on asphalt), so a jump never
 * lands inside a wall. chunk x, chunk z, local x, local z (units), yaw degrees. */
static const struct {
    int8_t cx, cz, lx, lz;
    int16_t yaw;
} k_viewpoints[] = {
    {3, 7, 31, 28, 0},   {5, 5, 11, 8, 90},  {3, 12, 31, 24, 180}, {8, 5, 31, 12, 270},
    {5, 8, 27, 28, 0},   {10, 2, 27, 16, 90},
    /* Facing a building 17.5 units ahead: walk into it to feel the collision. */
    {8, 5, 15, 12, 0},
};
#define VIEWPOINT_COUNT (sizeof(k_viewpoints) / sizeof(k_viewpoints[0]))
static uint32_t g_viewpoint;

static void place(city_player_t* p, uint32_t i) {
    p->view.pos.chunk_x = k_viewpoints[i].cx;
    p->view.pos.chunk_z = k_viewpoints[i].cz;
    p->view.pos.local_x = (int32_t)k_viewpoints[i].lx * 65536;
    p->view.pos.local_z = (int32_t)k_viewpoints[i].lz * 65536;
    p->view.yaw = (sat_fx16_t)k_viewpoints[i].yaw * 65536;
}

void player_init(city_player_t* p) {
    g_viewpoint = 0u;
    place(p, 0u);
    p->view.eye_y = 0; /* set from the archive's ground height by the caller */
}

void player_next_viewpoint(city_player_t* p) {
    g_viewpoint = (g_viewpoint + 1u) % VIEWPOINT_COUNT;
    place(p, g_viewpoint);
}

static void clamp_chunk(city_pos_t* pos) {
    /* Keep the walker inside the 16 x 16 grid. */
    if (pos->chunk_x < 0) { pos->chunk_x = 0; pos->local_x = 0; }
    if (pos->chunk_z < 0) { pos->chunk_z = 0; pos->local_z = 0; }
    if (pos->chunk_x >= CITY_GRID_X) { pos->chunk_x = CITY_GRID_X - 1; pos->local_x = CITY_CHUNK_FX - 1; }
    if (pos->chunk_z >= CITY_GRID_Z) { pos->chunk_z = CITY_GRID_Z - 1; pos->local_z = CITY_CHUNK_FX - 1; }
}

/* Body radius: a little over the 0.5 near plane, so the camera never ends up
 * inside the wall it is pressed against. */
#define PLAYER_RADIUS_FX (3 * 65536 / 4)

/* Returns 1 if the walker was pushed. */
static int collide(city_pos_t* pos) {
    int pushed = 0;
    for (int32_t dz = -1; dz <= 1; ++dz) {
        for (int32_t dx = -1; dx <= 1; ++dx) {
            int slot = city_res_lookup(&g_res, 2u, pos->chunk_x + dx, pos->chunk_z + dz,
                                       pos->chunk_x, pos->chunk_z);
            uint16_t bytes;
            const uint8_t* blob;
            if (slot < 0 || g_res.slots[slot].state != CITY_SLOT_READY) continue;
            blob = residency_slot_blob((uint16_t)slot, &bytes);
            pushed += city_blob_push_out(blob, bytes, dx * CITY_CHUNK_FX, dz * CITY_CHUNK_FX,
                                         PLAYER_RADIUS_FX, &pos->local_x, &pos->local_z);
        }
    }
    return pushed;
}

int player_update(city_player_t* p, const sat_pad_state_t* pad, uint32_t vblanks) {
    const int32_t hz = sat_video_is_ntsc_timing() ? 60 : 50;
    int32_t forward = 0, strafe = 0, turn = 0;
    int32_t speed;
    sat_fx16_t yaw, s, c;
    int moved;
    if (vblanks == 0u) vblanks = 1u;
    if (vblanks > MAX_DT_VBLANKS) vblanks = MAX_DT_VBLANKS;

    if (pad->held & SAT_PAD_UP) forward += 1;
    if (pad->held & SAT_PAD_DOWN) forward -= 1;
    if (pad->held & SAT_PAD_R) strafe += 1;
    if (pad->held & SAT_PAD_L) strafe -= 1;
    if (pad->held & SAT_PAD_LEFT) turn += 1;   /* yaw grows to the left */
    if (pad->held & SAT_PAD_RIGHT) turn -= 1;

    p->view.yaw += (turn * TURN_DEGREES_PER_SECOND * 65536 / hz) * (int32_t)vblanks;
    while (p->view.yaw >= 360 * 65536) p->view.yaw -= 360 * 65536;
    while (p->view.yaw < 0) p->view.yaw += 360 * 65536;

    speed = ((pad->held & SAT_PAD_B) ? RUN_UNITS_PER_SECOND : WALK_UNITS_PER_SECOND) * 65536 / hz;
    speed *= (int32_t)vblanks;
    yaw = p->view.yaw;
    s = sat_sin_deg(yaw);
    c = sat_cos_deg(yaw);
    /* forward = (s, c); right = (-c, s) in this right-handed frame. */
    p->view.pos.local_x += sat_fx16_mul(s, speed) * forward + sat_fx16_mul(-c, speed) * strafe;
    p->view.pos.local_z += sat_fx16_mul(c, speed) * forward + sat_fx16_mul(s, speed) * strafe;
    moved = city_pos_rebase(&p->view.pos);
    clamp_chunk(&p->view.pos);
    /* Collide against the 3x3 chunks around the walker. One push can move the
     * walker into the next chunk's frame, so re-rebase after it. */
    if (collide(&p->view.pos)) {
        ++g_city.collisions;
        moved |= city_pos_rebase(&p->view.pos);
    }
    clamp_chunk(&p->view.pos);
    return moved;
}
