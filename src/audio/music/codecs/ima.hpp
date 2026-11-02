#ifndef SATURN_CORE_AUDIO_IMA_ADPCM_HPP
#define SATURN_CORE_AUDIO_IMA_ADPCM_HPP

#include <stdint.h>

namespace saturn::core::audio::ima_adpcm {

constexpr uint32_t kBlockFrames = 1024u;
constexpr uint32_t kChannelBytes = kBlockFrames * 2u;
constexpr uint32_t kMaxBlockBytes = 1032u;

inline uint32_t block_bytes(uint8_t channels) {
    const uint32_t payload = ((kBlockFrames - 1u) * channels + 1u) / 2u;
    return (4u * channels + payload + 3u) & ~3u;
}

inline int32_t clamp_sample(int32_t sample) {
    if (sample < -32768) return -32768;
    if (sample > 32767) return 32767;
    return sample;
}

inline constexpr uint8_t clamp_index(int32_t index) {
    if (index < 0) return 0u;
    if (index > 88) return 88u;
    return static_cast<uint8_t>(index);
}

struct DecodeTables {
    uint16_t magnitude[89][8] = {};
    uint8_t next_index[89][8] = {};

    constexpr DecodeTables() {
        constexpr uint16_t steps[89] = {
            7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
            34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
            130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371,
            408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166,
            1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
            3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845,
            8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500,
            20350, 22385, 24623, 27086, 29794, 32767,
        };
        constexpr int8_t index_delta[8] = {-1, -1, -1, -1, 2, 4, 6, 8};
        for (uint32_t index = 0u; index < 89u; ++index) {
            const uint32_t step = steps[index];
            for (uint32_t code = 0u; code < 8u; ++code) {
                uint32_t delta = step >> 3u;
                if ((code & 4u) != 0u) delta += step;
                if ((code & 2u) != 0u) delta += step >> 1u;
                if ((code & 1u) != 0u) delta += step >> 2u;
                magnitude[index][code] = static_cast<uint16_t>(delta);
                next_index[index][code] = clamp_index(
                    static_cast<int32_t>(index) + index_delta[code]);
            }
        }
    }
};

inline constexpr DecodeTables kDecodeTables{};

inline void decode_nibble(uint8_t code, int32_t& predictor, uint8_t& index,
                          uint8_t* destination) {
    const uint8_t magnitude_code = code & 7u;
    const int32_t difference = kDecodeTables.magnitude[index][magnitude_code];
    predictor = clamp_sample(predictor + ((code & 8u) != 0u ? -difference : difference));
    index = kDecodeTables.next_index[index][magnitude_code];
    const uint16_t sample = static_cast<uint16_t>(predictor);
    destination[0] = static_cast<uint8_t>(sample >> 8u);
    destination[1] = static_cast<uint8_t>(sample);
}

/* Each block starts with one exact S16BE predictor and step index per
 * channel, followed by high-nibble-first interleaved IMA deltas. Output is
 * channel-planar S16BE, ready for the two mono SCSP stream rings. */
inline bool decode_block(const uint8_t* source, uint8_t channels, uint8_t* output) {
    if (source == nullptr || output == nullptr || (channels != 1u && channels != 2u)) {
        return false;
    }
    int32_t predictor[2] = {};
    uint8_t index[2] = {};
    for (uint8_t channel = 0u; channel < channels; ++channel) {
        const uint8_t* header = source + 4u * channel;
        if (header[2] > 88u) return false;
        predictor[channel] = static_cast<int16_t>(
            (static_cast<uint16_t>(header[0]) << 8u) | header[1]);
        index[channel] = header[2];
        uint8_t* first = output + channel * kChannelBytes;
        first[0] = header[0];
        first[1] = header[1];
    }
    const uint8_t* payload = source + 4u * channels;
    if (channels == 2u) {
        uint8_t* left = output + 2u;
        uint8_t* right = output + kChannelBytes + 2u;
        for (uint32_t frame = 1u; frame < kBlockFrames; ++frame) {
            const uint8_t packed = payload[frame - 1u];
            decode_nibble(packed >> 4u, predictor[0], index[0], left);
            decode_nibble(packed & 15u, predictor[1], index[1], right);
            left += 2u;
            right += 2u;
        }
    } else {
        uint8_t* destination = output + 2u;
        for (uint32_t frame = 1u; frame < kBlockFrames; ++frame) {
            const uint32_t nibble = frame - 1u;
            const uint8_t packed = payload[nibble / 2u];
            decode_nibble((nibble & 1u) == 0u ? packed >> 4u : packed & 15u,
                          predictor[0], index[0], destination);
            destination += 2u;
        }
    }
    return true;
}

}  // namespace saturn::core::audio::ima_adpcm

#endif
