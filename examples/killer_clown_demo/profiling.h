#ifndef CLOWN_PROFILING_H
#define CLOWN_PROFILING_H

/* Example-local wire format: bounded telemetry, no allocation or core API. */
enum {
    CLOWN_PROFILE_MAGIC = 0x434c5031, CLOWN_PROFILE_CAPACITY = 192
};
enum {
    PROFILE_WAIT, PROFILE_INPUT, PROFILE_EVALUATE, PROFILE_ENVIRONMENT,
    PROFILE_LIGHTS, PROFILE_NORMALS, PROFILE_GEOMETRY, PROFILE_FLUSH,
    PROFILE_HUD, PROFILE_END, PROFILE_POSE, PROFILE_SKIN_JOIN,
    PROFILE_BOUNDS, PROFILE_STAGE_COUNT
};
typedef struct {
    uint32_t frame, fields, simulation, ui, lights, commands, late_swaps;
    uint32_t elapsed, stage[PROFILE_STAGE_COUNT], painter, emission;
} clown_profile_row_t;
volatile struct {
    uint32_t magic, capacity, row_bytes, frequency, count, overwritten;
    clown_profile_row_t rows[CLOWN_PROFILE_CAPACITY];
} g_clown_profile __attribute__((section(".wram_l")));
#if CLOWN_PROFILE
static clown_profile_row_t profile_row;
static uint64_t profile_marks[PROFILE_STAGE_COUNT];
static void profile_start(unsigned stage) {profile_marks[stage]=sat_time_ticks();}
static void profile_stop(unsigned stage) {
    profile_row.stage[stage]+=(uint32_t)(sat_time_ticks()-profile_marks[stage]);
}
#else
static void profile_start(unsigned stage) {(void)stage;}
static void profile_stop(unsigned stage) {(void)stage;}
#endif
#endif
