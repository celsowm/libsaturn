#include <cstdio>
#include <cstdlib>

#include "saturn/audio.h"
#include "src/audio/playback/claim.hpp"
#include "src/audio/playback/state.hpp"
#include "src/core/runtime/state.hpp"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

using namespace saturn::core::audio::playback;
namespace claim = saturn::core::audio::claim;

namespace saturn::core {
RuntimeState g_state = {};
/* The stream API's answer, faked: which rings have a stream open. */
static bool g_open[kAudioStreamCapacity] = {};
bool audio_stream_ring_open(uint16_t ring) { return ring < kAudioStreamCapacity && g_open[ring]; }
}  // namespace saturn::core

namespace saturn::hal::scsp {
static uint32_t g_last_upload = 0u;
bool upload(uint32_t offset, const void*, uint32_t) {
    g_last_upload = offset;
    return true;
}
uint16_t encode_pitch(uint32_t, uint32_t) { return 0u; }
void mute_slot(uint8_t) {}
void key_off(uint8_t) {}
}  // namespace saturn::hal::scsp

/* No voice plays here (voice.cpp is not linked). */
extern "C" sat_result_t sat_sound_stop_all_instances(sat_sound_t) { return SAT_OK; }

int main() {
    reset_runtime_state();
    OK(sat_sound_ram_borrow_stream_rings(1u) == SAT_ERR_NOT_INITIALIZED);
    g_initialized = 1u;
    const uint32_t ring = kStreamRingBytes;

    // Without lent rings the resident area ends where the rings start.
    uint32_t whole = 0u, offset = 0u;
    OK(sat_sound_ram_reserve(kResidentSoundRamEnd - kSoundRamBase, &whole) == SAT_OK && whole == kSoundRamBase);
    OK(sat_sound_ram_reserve(2u, &offset) == SAT_ERR_CAPACITY);
    OK(sat_sound_ram_write(kResidentSoundRamEnd, &offset, 2u) == SAT_ERR_INVALID_ARG);

    // Lending a ring extends it by 16 KB, and a sound lands there.
    OK(sat_sound_ram_borrow_stream_rings(5u) == SAT_ERR_INVALID_ARG);
    OK(sat_sound_ram_borrow_stream_rings(1u) == SAT_OK && claim::ring_lent(0u) && !claim::ring_lent(1u));
    OK(resident_ram_end() == kResidentSoundRamEnd + ring);
    static const int8_t pcm[64] = {};
    const sat_sound_desc_t desc = {pcm, 64u, 22050u, 0u, 0u, SAT_AUDIO_PCM_S8, 0u, 0u, 0u};
    sat_sound_t sound{};
    OK(sat_sound_create(&sound, &desc) == SAT_OK && saturn::hal::scsp::g_last_upload == kResidentSoundRamEnd);
    OK(sat_sound_ram_write(kResidentSoundRamEnd + ring - 2u, &offset, 2u) == SAT_OK);
    OK(sat_sound_ram_write(kResidentSoundRamEnd + ring, &offset, 2u) == SAT_ERR_INVALID_ARG);

    // A ring with a stream open is not lent; one holding a sound is not given back.
    saturn::core::g_open[1] = true;
    OK(sat_sound_ram_borrow_stream_rings(2u) == SAT_ERR_BUSY && claim::g_lent_rings == 1u);
    saturn::core::g_open[1] = false;
    OK(sat_sound_ram_borrow_stream_rings(0u) == SAT_ERR_BUSY && claim::g_lent_rings == 1u);
    OK(sat_sound_unload(sound) == SAT_OK);
    OK(sat_sound_ram_borrow_stream_rings(0u) == SAT_OK && resident_ram_end() == kResidentSoundRamEnd);

    // Every ring at once, then the reset gives them back.
    OK(sat_sound_ram_borrow_stream_rings(4u) == SAT_OK);
    OK(sat_sound_ram_reserve(4u * ring, &offset) == SAT_OK && offset == kResidentSoundRamEnd && offset + 4u * ring == 0x80000u);
    reset_runtime_state();
    OK(claim::g_lent_rings == 0u && resident_ram_end() == kResidentSoundRamEnd);
    (void)whole;
    std::puts("test_sound_ram_borrow: OK");
    return 0;
}
