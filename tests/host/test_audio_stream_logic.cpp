#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "src/audio/streaming/runtime.hpp"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

int main() {
    uint8_t storage[8u] = {};
    saturn::core::AudioStreamRing ring{};
    ring.buffer = storage;
    ring.capacity_frames = 4u;
    ring.frame_bytes = 2u;
    saturn::core::audio_stream_ring_reset(ring);

    const uint8_t first[] = {1u, 2u, 3u, 4u, 5u, 6u};
    saturn::core::audio_stream_copy_in(ring, first, 3u);
    OK(ring.buffered_frames == 3u && ring.minimum_fill == 3u && ring.maximum_fill == 3u);

    uint8_t out[8u] = {};
    OK(saturn::core::audio_stream_copy_out(ring, out, 2u) == 2u);
    OK(std::memcmp(out, first, 4u) == 0);
    OK(ring.buffered_frames == 1u);

    const uint8_t second[] = {7u, 8u, 9u, 10u, 11u, 12u};
    saturn::core::audio_stream_copy_in(ring, second, 3u);
    OK(ring.buffered_frames == 4u);
    OK(saturn::core::audio_stream_copy_out(ring, out, 4u) == 4u);
    const uint8_t expected[] = {5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u};
    OK(std::memcmp(out, expected, sizeof(expected)) == 0);

    OK(saturn::core::audio_stream_copy_out(ring, out, 1u) == 0u);
    OK(ring.underrun_count == 1u);
    std::puts("audio stream logic: OK");
    return 0;
}
