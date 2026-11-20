#ifndef WATER_LAB_H
#define WATER_LAB_H

#include "saturn/scene.h"
#include "saturn/fx_compositor.h"
#include "saturn/input.h"
#include "saturn/water_volume.h"

enum { LAB_WIDTH = 320, LAB_HEIGHT = 224, LAB_NEAR = SAT_FX16_ONE / 2 };

#define LAB_UNIT(n) ((n) * SAT_FX16_ONE)

typedef struct lab_player {
    sat_vec3_t position;
    sat_vec3_t velocity;
    sat_fx16_t immersed;
} lab_player_t;

void lab_player_reset(lab_player_t* player);
void lab_player_step(lab_player_t* player, const sat_pad_state_t* pad,
                     const sat_water_volume_t* volume);
sat_fx16_t lab_floor_height(sat_fx16_t x, sat_fx16_t z);

void lab_camera_update(sat_camera3d_t* camera, const lab_player_t* player,
                       uint32_t tick, int32_t orbit, uint8_t automatic);

enum { LAB_MAX_SUBDIVISIONS = 16 };

typedef enum lab_mode {
    LAB_DRY, LAB_OPAQUE, LAB_MESH, LAB_HALF, LAB_UNDERWATER, LAB_MULTIPASS, LAB_POOL,
    LAB_MODE_COUNT
} lab_mode_t;

typedef struct lab_water {
    lab_mode_t mode;
    sat_fx16_t height;
    uint8_t subdivisions;
    uint8_t waves;
} lab_water_t;

void lab_surface_draw(sat_scene_t* scene, const lab_water_t* water, uint32_t tick);

void lab_scene_init(void);
void lab_scene_draw(sat_scene_t* scene, const lab_player_t* player,
                    uint32_t tick, uint16_t workload, const lab_water_t* water);
uint16_t lab_unsupported_crossings(void);

void lab_benchmark_init(void);
void lab_benchmark_scene_timing(uint32_t preparation, uint32_t surface, uint32_t flush);
void lab_benchmark_draw(const sat_scene_t* scene, uint32_t tick, uint16_t workload,
                        const lab_water_t* water, uint8_t diagnostics, uint8_t paused,
                        const lab_player_t* player, sat_fx16_t camera_y, uint8_t camera_immersed,
                        sat_result_t fx_result, const sat_fx_compositor_t* fx);
void lab_benchmark_submitted(uint32_t tick);

#endif
