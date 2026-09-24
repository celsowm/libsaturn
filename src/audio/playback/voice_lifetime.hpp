#ifndef SATURN_AUDIO_PLAYBACK_VOICE_LIFETIME_HPP
#define SATURN_AUDIO_PLAYBACK_VOICE_LIFETIME_HPP

#include <stdint.h>

/* Pure timing policy for resident voices. The app frame can stall while
 * synchronous CD reads continue pumping the audio service via VBlank edges.
 * Use one service-frame timeline for stream consumption and voice expiry.
 * All signed-delta comparisons assume fewer than 2^31 frames elapse between
 * observations; durations are capped below that wrap-safe horizon. */
namespace saturn::core::audio::voice::lifetime {

inline uint32_t duration_frames(uint32_t sample_count,uint32_t sample_rate,
                                uint32_t pitch_q16,uint32_t display_rate) {
    if(!sample_count || !sample_rate || !pitch_q16 || !display_rate)
        return 1u;
    const uint64_t numerator=static_cast<uint64_t>(sample_count)*
        display_rate*65536u;
    const uint64_t denominator=static_cast<uint64_t>(sample_rate)*pitch_q16;
    const uint64_t quotient=numerator/denominator;
    const uint64_t rounded=quotient+(numerator%denominator!=0u?1u:0u);
    if(rounded==0u)return 1u;
    return rounded>0x7FFFFFFFu?0x7FFFFFFFu:
        static_cast<uint32_t>(rounded);
}

inline uint32_t play_start(uint32_t app_frame,uint32_t service_frame,
                           bool service_valid) {
    return service_valid &&
           static_cast<int32_t>(service_frame-app_frame)>0
        ? service_frame : app_frame;
}

inline bool expired(uint32_t service_frame,uint32_t end_frame) {
    return static_cast<int32_t>(service_frame-end_frame)>=0;
}

} // namespace saturn::core::audio::voice::lifetime

#endif
