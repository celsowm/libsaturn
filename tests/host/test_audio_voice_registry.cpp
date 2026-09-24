#include <cassert>
#include <cstdint>
#include <cstdio>
#include "src/audio/playback/voice_registry.hpp"

using saturn::core::audio::voice::Registry;

int main() {
    Registry<3u> voices{};
    voices.reset();
    assert(voices.active_count(3u)==0u);
    assert(voices.entries[0].generation==1u);
    assert(voices.activate(3u)==nullptr);
    auto* first=voices.activate(0u);
    assert(first && first->active && first->generation==2u);
    first->end_frame=123u;
    const sat_voice_t handle{0u,2u};
    assert(voices.resolve(handle)==first);
    assert(voices.resolve(sat_voice_t{0u,1u})==nullptr);
    assert(voices.resolve(sat_voice_t{3u,2u})==nullptr);
    assert(voices.activate(0u)==nullptr);
    auto* second=voices.activate(1u);
    assert(second && voices.active_count(3u)==2u);
    assert(voices.active_count(1u)==1u);
    assert(voices.release(0u));
    assert(!voices.release(0u));
    assert(!voices.release(3u));
    assert(voices.resolve(handle)==nullptr);
    assert(voices.entries[0].generation==2u);
    auto* renewed=voices.activate(0u);
    assert(renewed && renewed->generation==3u);
    assert(renewed->end_frame==0u);
    voices.reset();
    assert(voices.active_count(3u)==0u);
    assert(voices.resolve(sat_voice_t{0u,3u})==nullptr);
    assert(voices.entries[0].generation==4u);

    Registry<1u> wrapped{};
    wrapped.entries[0].generation=UINT16_MAX;
    auto* after_wrap=wrapped.activate(0u);
    assert(after_wrap && after_wrap->generation==1u);
    std::puts("audio voice registry: OK");
}
