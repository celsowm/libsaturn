#include <cassert>
#include <cstdint>
#include <cstdio>
#include "src/audio/playback/sound_registry.hpp"

using saturn::core::audio::sound::Registry;

int main() {
    Registry<2u> registry{};
    registry.reset();
    assert(registry.active_count()==0u && registry.first_free()==0u);
    assert(registry.entries[0].generation==1u);
    auto* sound=registry.activate(0u);
    assert(sound && sound->used && sound->generation==2u);
    sound->ram_offset=512u;
    sound->allocation_slot=7u;
    const sat_sound_t first{0u,sound->generation};
    assert(registry.resolve(first)==sound);
    assert(registry.resolve(sat_sound_t{0u,1u})==nullptr);
    assert(registry.resolve(sat_sound_t{2u,2u})==nullptr);
    assert(registry.activate(0u)==nullptr);
    assert(registry.activate(2u)==nullptr);
    assert(registry.activate(1u)!=nullptr);
    assert(registry.first_free()==2u && registry.active_count()==2u);
    assert(registry.invalidate(first));
    assert(!registry.invalidate(first));
    assert(registry.resolve(first)==nullptr);
    assert(registry.entries[0].ram_offset==0u);
    assert(registry.entries[0].allocation_slot==0u);
    assert(registry.first_free()==0u && registry.active_count()==1u);
    const auto* next=registry.activate(0u);
    assert(next && next->generation==4u); // Creation + unload + recreation.
    registry.reset();
    assert(registry.active_count()==0u);
    assert(registry.resolve(sat_sound_t{0u,4u})==nullptr);
    assert(registry.entries[0].generation==5u);

    Registry<1u> wrapped{};
    wrapped.entries[0].generation=UINT16_MAX;
    auto* after_wrap=wrapped.activate(0u);
    assert(after_wrap && after_wrap->generation==1u);
    assert(wrapped.invalidate(sat_sound_t{0u,1u}));
    assert(wrapped.entries[0].generation==2u);
    std::puts("audio sound registry: OK");
}
