#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "examples/ikemen_saturn/ikemen_anim.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); std::exit(1); } } while(0)

int main() {
    {
        const uint8_t packed[] = {0u,0u,0u,0u,0x00u,0x83u,0x47u};
        const ik_sprite_source_t sprite = {
            0u, sizeof(packed), 3u, 2u, 8u, 2u, IK_SPRITE_FORMAT_LZ5
        };
        uint8_t output[16] = {};
        OK(ik_sprite_decode(&sprite, packed, sizeof(packed), output, sizeof(output)) != 0);
        const uint8_t expected[16] = {
            0u,0u,3u,3u,3u,0u,0u,0u,
            0u,0u,3u,7u,7u,0u,0u,0u
        };
        for (uint32_t i=0u;i<sizeof(expected);++i) OK(output[i] == expected[i]);
    }
    {
        const uint8_t raw[] = {1u,2u,3u};
        const ik_sprite_source_t sprite = {
            0u, sizeof(raw), 3u, 1u, 8u, 2u, IK_SPRITE_FORMAT_RAW
        };
        uint8_t output[8] = {};
        OK(ik_sprite_decode(&sprite, raw, sizeof(raw), output, sizeof(output)) != 0);
        const uint8_t expected[8] = {0u,0u,1u,2u,3u,0u,0u,0u};
        for (uint32_t i=0u;i<sizeof(expected);++i) OK(output[i] == expected[i]);
    }
    {
        const uint8_t truncated[] = {0u,0u,0u,0u,0u};
        const ik_sprite_source_t sprite = {
            0u, sizeof(truncated), 1u, 1u, 8u, 3u, IK_SPRITE_FORMAT_LZ5
        };
        uint8_t output[8] = {};
        OK(ik_sprite_decode(&sprite, truncated, sizeof(truncated), output, sizeof(output)) == 0);
    }
    std::puts("[test] ikemen_decode OK");
    return 0;
}
