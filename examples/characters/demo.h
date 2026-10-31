#ifndef LIBSATURN_CHARACTER_DEMO_H
#define LIBSATURN_CHARACTER_DEMO_H
typedef enum {
    CHARACTER_DEMO_VIEWER,
    CHARACTER_DEMO_ARENA,
    CHARACTER_DEMO_STRESS,
    /* Same fighter simulation and arena rendering as ARENA, with the E11
     * VDP2 stage behind it. Compiled only into vdp2_fighting_stage. */
    CHARACTER_DEMO_STAGE
} character_demo_mode_t;
int character_demo_run(character_demo_mode_t mode);
#endif
