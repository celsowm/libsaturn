#ifndef SATURN_HAL_SCSP_HPP
#define SATURN_HAL_SCSP_HPP

#include <stdint.h>

namespace saturn::hal::scsp {

constexpr uint32_t kSoundRamBytes = 0x80000u;
constexpr uint32_t kSystemReservedBytes = 0x0B000u;
constexpr uint8_t kSlotCount = 32u;

struct SlotConfig {
    uint32_t start_address;
    uint32_t sample_rate;
    uint16_t sample_count;
    uint16_t loop_start;
    uint16_t loop_end;
    uint32_t pitch_scale_q16;
    uint8_t pcm8;
    uint8_t loop;
    uint8_t total_level;
    uint8_t direct_level;
    uint8_t pan;
};

bool init();
void shutdown();
bool is_ready();

bool upload(uint32_t offset, const void* data, uint32_t byte_count);
void clear_sound_ram(uint32_t offset, uint32_t byte_count);

bool configure_slot(uint8_t slot, const SlotConfig& config);
// Read CA through the MSLC slot monitor. The returned value is the current
// sample offset in the SCSP's native 4096-sample units.
bool read_current_sample_block(uint8_t slot, uint8_t* out_block);
void key_on(uint8_t slot);
void key_off(uint8_t slot);
// TL=0xFF: silences a slot whose envelope release may still read Sound RAM.
void mute_slot(uint8_t slot);
void set_slot_level_pan(uint8_t slot, uint8_t total_level, uint8_t direct_level, uint8_t pan);
void stop_all_slots();
void set_master_volume(uint8_t level);

uint16_t encode_pitch(uint32_t sample_rate, uint32_t pitch_scale_q16);
uint8_t encode_pan(int16_t pan);

}  // namespace saturn::hal::scsp

#endif
