#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include "examples/infinite_explorer/explorer_logic.h"

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); std::exit(1); } } while (0)

static int32_t pair(const uint16_t p[48], int at) {
    return (int32_t)(((uint32_t)p[at] << 16) | p[at + 1]);
}

static void rebase_crosses_all_edges() {
    explorer_pos_t p = {4, -3, (EXPLORER_CHUNK_SIZE << 16) + 5, -7};
    explorer_rebase(&p);
    CHECK(p.chunk_x == 5 && p.local_x == 5);
    CHECK(p.chunk_z == -4 && p.local_z == (EXPLORER_CHUNK_SIZE << 16) - 7);
}

static void generation_is_deterministic() {
    CHECK(explorer_hash(123, -7, 19) == explorer_hash(123, -7, 19));
    CHECK(explorer_hash(123, -7, 19) != explorer_hash(124, -7, 19));
    CHECK(explorer_biome(99, -1, -1) == explorer_biome(99, -1, -1));
}

static void cardinal_yaw_matrices() {
    uint16_t p[48];
    explorer_build_ground_params(0,0,0,65536,0x12000,p);
    CHECK(pair(p,14) == 65536 && pair(p,16) == 0);
    CHECK(pair(p,20) == 0 && pair(p,22) == 65536);
    explorer_build_ground_params(0,0,65536,0,0x12000,p);
    CHECK(pair(p,14) == 0 && pair(p,16) == 65536);
    CHECK(pair(p,20) == -65536 && pair(p,22) == 0);
}

static void projection_turns_with_camera() {
    explorer_projection_t ahead0 = explorer_project(0, 100 << 16, 0, 65536);
    explorer_projection_t ahead90 = explorer_project(100 << 16, 0, 65536, 0);
    explorer_projection_t behind = explorer_project(0, -(100 << 16), 0, 65536);
    CHECK(ahead0.visible && ahead90.visible);
    CHECK(ahead0.x == 160 && ahead90.x == 160);
    CHECK(!behind.visible);
}

int main() {
    rebase_crosses_all_edges();
    generation_is_deterministic();
    cardinal_yaw_matrices();
    projection_turns_with_camera();
    std::printf("PASS: test_infinite_explorer.cpp (4 tests)\n");
    return 0;
}
