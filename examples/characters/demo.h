#ifndef LIBSATURN_CHARACTER_DEMO_H
#define LIBSATURN_CHARACTER_DEMO_H
typedef enum { CHARACTER_DEMO_VIEWER, CHARACTER_DEMO_ARENA, CHARACTER_DEMO_STRESS } character_demo_mode_t;
int character_demo_run(character_demo_mode_t mode);
#endif
