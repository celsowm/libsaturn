/* test_vdp1_logic.cpp — host tests for VDP1 logic helpers */

#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include "saturn/vdp1.h"
#include "src/core/logic.hpp"

#define TEST(name) static void name()
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { \
    fprintf(stderr, "FAIL %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b); \
    exit(1); } } while(0)
#define ASSERT_TRUE(cond) do { if (!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    exit(1); } } while(0)

TEST(resolve_sprite_cmd_null_cmd) {
    saturn::core::ResolvedSprite out = {};
    ASSERT_EQ(saturn::core::resolve_sprite_cmd(nullptr, &out), SAT_ERR_INVALID_ARG);
}

TEST(resolve_sprite_cmd_null_texture) {
    sat_sprite_cmd_t cmd = {};
    cmd.texture = nullptr;
    saturn::core::ResolvedSprite out = {};
    ASSERT_EQ(saturn::core::resolve_sprite_cmd(&cmd, &out), SAT_ERR_INVALID_ARG);
}

TEST(resolve_sprite_cmd_invalid_texture) {
    sat_texture_t tex = {};
    tex.valid = 0;
    sat_sprite_cmd_t cmd = {};
    cmd.texture = &tex;
    saturn::core::ResolvedSprite out = {};
    ASSERT_EQ(saturn::core::resolve_sprite_cmd(&cmd, &out), SAT_ERR_INVALID_ARG);
}

TEST(resolve_sprite_cmd_defaults) {
    sat_texture_t tex = {};
    tex.valid = 1;
    tex.width = 32;
    tex.height = 32;
    tex.srca = 0x8000;
    tex.palette = 0;

    sat_sprite_cmd_t cmd = {};
    cmd.x = 10 * SAT_FX16_ONE;
    cmd.y = 20 * SAT_FX16_ONE;
    cmd.width = 0;  /* default to texture */
    cmd.height = 0; /* default to texture */
    cmd.texture = &tex;
    cmd.palette_override = 0;

    saturn::core::ResolvedSprite out = {};
    ASSERT_EQ(saturn::core::resolve_sprite_cmd(&cmd, &out), SAT_OK);
    ASSERT_EQ(out.x, 10);
    ASSERT_EQ(out.y, 20);
    ASSERT_EQ(out.width, 32);
    ASSERT_EQ(out.height, 32);
    ASSERT_EQ(out.srca, 0x8000);
    ASSERT_EQ(out.palette, 0);
}

TEST(resolve_sprite_cmd_palette_override) {
    sat_texture_t tex = {};
    tex.valid = 1;
    tex.width = 16;
    tex.height = 16;
    tex.srca = 0x8000;
    tex.palette = 0;

    sat_sprite_cmd_t cmd = {};
    cmd.x = 0;
    cmd.y = 0;
    cmd.width = 0;
    cmd.height = 0;
    cmd.texture = &tex;
    cmd.palette_override = 3;  /* override */

    saturn::core::ResolvedSprite out = {};
    ASSERT_EQ(saturn::core::resolve_sprite_cmd(&cmd, &out), SAT_OK);
    ASSERT_EQ(out.palette, 3);
}

int main() {
    resolve_sprite_cmd_null_cmd();
    resolve_sprite_cmd_null_texture();
    resolve_sprite_cmd_invalid_texture();
    resolve_sprite_cmd_defaults();
    resolve_sprite_cmd_palette_override();

    printf("PASS: test_vdp1_logic.cpp (%d tests)\n", 5);
    return 0;
}
