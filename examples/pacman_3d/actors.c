#include "actors.h"

#include "actor_render.h"
#include "ghost_model.h"
#include "p3d_config.h"
#include "pac_model.h"

static const uint16_t kGhostColors[PAC_GHOST_COUNT] = {
    SAT_RGB555(31, 0, 0),
    SAT_RGB555(31, 18, 24),
    SAT_RGB555(0, 28, 31),
    SAT_RGB555(31, 20, 4),
};

/* Frames of fright left when the warning flash starts. Blue for most of
 * it, flashing white only once the timer is nearly out: flashing the whole
 * time hides how much is left. */
#define FRIGHT_WARNING 120u

void p3d_actors_init(void) {
    p3d_pac_init();
    p3d_ghost_init();
}

void p3d_actors_relight(const p3d_camera_t* camera) {
    sat_vec3_t light;
    p3d_actor_key_light(camera, &light);
    p3d_pac_relight(&light);
    p3d_ghost_relight(&light);
}

void p3d_actors_draw(sat_scene_t* scene, const pac_game_t* game,
                     const p3d_camera_t* camera) {
    p3d_ghost_mood_t mood = P3D_GHOST_HUNTING;
    int i;

    if (pac_game_frightened(game)) {
        const int flash = game->fright < FRIGHT_WARNING && ((game->frame / 6u) & 1u) == 0u;
        mood = flash ? P3D_GHOST_FLASHING : P3D_GHOST_FRIGHTENED;
    }

    for (i = 0; i < PAC_GHOST_COUNT; ++i) {
        const sat_grid_actor_t* a = &game->ghosts[i].actor;
        const uint16_t color = (mood == P3D_GHOST_FLASHING) ? P3D_COLOR_FRIGHT_B
            : (mood == P3D_GHOST_FRIGHTENED) ? P3D_COLOR_FRIGHT_A : kGhostColors[i];
        if (pac_game_ghost_penned(game, i)) {
            continue;
        }
        p3d_ghost_draw(scene, camera, a->x, a->y, pac_game_facing(game, i), color, mood);
    }
    /* Maze pixels (x, y) are world (x, z). */
    p3d_pac_draw(scene, game->pac.x, game->pac.y,
        pac_game_facing(game, PAC_SLOT_PAC), game->frame);
}
