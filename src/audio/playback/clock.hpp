#ifndef SATURN_AUDIO_PLAYBACK_CLOCK_HPP
#define SATURN_AUDIO_PLAYBACK_CLOCK_HPP

#include <stdint.h>

/* Pure, caller-owned audio service clock. sat_frame_count() advances during
 * ordinary frames, while CD Block waits may pump audio while the app's frame
 * number is stalled. Count rising VBlank edges then reconcile (never add)
 * the next app-frame observation, avoiding duplicate stream consumption.
 * This policy does not poll VDP2, reference SCSP or store global state.
 * Assumes fewer than 2^31 ticks between observations (wrap-safe ordering). */
namespace saturn::core::audio::clock {

struct State {
    uint32_t service_frame;
    uint32_t last_app_frame;
    uint8_t last_vblank;
    uint8_t valid;

    void reset() {
        service_frame=0u;
        last_app_frame=0u;
        last_vblank=0u;
        valid=0u;
    }

    uint32_t tick(uint32_t now,uint8_t vblank) {
        const uint8_t high=vblank!=0u?1u:0u;
        if(!valid) {
            service_frame=now;
            last_app_frame=now;
            valid=1u;
        } else if(now!=last_app_frame) {
            /* A polled VBlank may already have advanced service_frame while
             * synchronous I/O held the application frame number constant. */
            if(static_cast<int32_t>(now-service_frame)>0)
                service_frame=now;
            last_app_frame=now;
        } else if(high!=0u && last_vblank==0u) {
            ++service_frame;
        }
        last_vblank=high;
        return service_frame;
    }
};

}  // namespace saturn::core::audio::clock

#endif
