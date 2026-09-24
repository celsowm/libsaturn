#include <cassert>
#include <cstdint>
#include <cstdio>
#include "src/audio/playback/clock.hpp"

using saturn::core::audio::clock::State;

int main() {
    State c{};
    assert(c.tick(100u,0u)==100u);
    assert(c.tick(100u,0u)==100u);
    assert(c.tick(100u,1u)==101u);
    assert(c.tick(100u,1u)==101u); // Held high is not another VBlank.
    assert(c.tick(100u,0u)==101u);
    assert(c.tick(100u,1u)==102u);
    assert(c.tick(101u,0u)==102u); // No replay of an already counted edge.
    assert(c.tick(104u,1u)==104u); // Catch up only when newer.
    assert(c.tick(104u,0u)==104u);
    assert(c.tick(104u,1u)==105u);
    c.reset();
    assert(c.tick(7u,1u)==7u); // First high is a sample, not an edge.
    assert(c.tick(7u,0u)==7u);
    assert(c.tick(7u,1u)==8u);

    c.reset();
    assert(c.tick(UINT32_MAX-1u,0u)==UINT32_MAX-1u);
    assert(c.tick(UINT32_MAX-1u,1u)==UINT32_MAX);
    assert(c.tick(UINT32_MAX-1u,0u)==UINT32_MAX);
    assert(c.tick(UINT32_MAX-1u,1u)==0u); // 32-bit service wrap.
    assert(c.tick(0u,0u)==0u); // App-frame wrap: no double count.
    assert(c.tick(1u,0u)==1u);
    std::puts("audio service clock: OK");
}
